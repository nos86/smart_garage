#include <Arduino.h>

#include "board.h"
#include "canopen/CO_storageFlash.h"
#include "canopen/OD.h"
#include "cli/model.h"
#include "tasks.h"

#include "CANopen.h"

// Minimal CANopen bring-up on top of hal::CanTransceiver/task_can.cpp (see
// src/app/canopen/ for the CANopenNode glue). Scope, per
// docs/architecture-plan.md: join the bus, NMT self-start (no master
// required -- CO_NMT_STARTUP_TO_OPERATIONAL implements the documented
// "every node promotes itself to Operational at end of local init" decision,
// object 1F80h in the CiA 301 text this was modeled on), heartbeat
// producer, respond to basic SDO reads against CANopenNode's shipped
// default Object Dictionary (src/app/canopen/OD.c), and drive board::led1/
// led2 as the CiA 303-3 run/error indicator pair via CANopenNode's LEDs
// module. Full custom OD design, COB-ID/priority scheme, NMT master
// supervision, and OTA are explicitly deferred -- see the architecture
// doc's P0/P1 backlog.

namespace app
{

  namespace
  {

    // NMT_CONTROL: self-start to Operational without waiting for a master
    // (CO_NMT_STARTUP_TO_OPERATIONAL). CO_NMT_ERR_ON_ERR_REG +
    // CO_ERR_REG_GENERIC_ERR still drop the node to
    // pre-operational/stopped on a nonzero generic error register, but
    // CO_ERR_REG_COMMUNICATION is deliberately *not* in the mask (unlike
    // CANopenNode's own reference main_blank.c default): per
    // docs/canopen-network-spec.md §7, a peer HB timeout or a comm error
    // must not demote this node's own NMT state -- that would drop its
    // TPDOs and propagate the fault to neighbours, the opposite of the
    // "works without a master" requirement. The node stays Operational,
    // reports via EMCY (already wired through CO_CANopenInit), and
    // recovers on its own; bus-off recovery remains the driver's job.
    constexpr uint16_t kNmtControl = CO_NMT_STARTUP_TO_OPERATIONAL | CO_NMT_ERR_ON_ERR_REG | CO_ERR_REG_GENERIC_ERR;
    constexpr uint16_t kFirstHbTimeMs = 500;
    constexpr uint16_t kSdoServerTimeoutMs = 1000;
    constexpr uint16_t kSdoClientTimeoutMs = 500;
    constexpr bool kSdoClientBlockTransfer = false;
    constexpr uint16_t kBitrateKbps = 250;
    constexpr TickType_t kProcessPeriod = pdMS_TO_TICKS(10);

    // OD_PERSIST_COMM storage entry -- module-level (not a stack local of
    // initCanopen()): CO_storage_init() wires OD_extension_init() pointers
    // into `storage`/`storageEntries` that CANopenNode dereferences for the
    // whole life of the OD 1010h/1011h objects, i.e. for as long as the
    // node runs.
    CO_storage_t storage{};
    CO_storage_entry_t storageEntries[] = {{.addr = &OD_PERSIST_COMM,
                                             .len = sizeof(OD_PERSIST_COMM),
                                             .subIndexOD = 2,
                                             .attr = CO_storage_cmd | CO_storage_restore,
                                             .addrNV = nullptr}};

    // Resolves the node-ID to bring up, per spec §3: normally the value
    // persisted in flash (0xFF/LSS-waiting if none yet) -- canopen::persist
    // already loaded it into persist::lssPendingNodeId by the time this
    // runs. DIP 1-31 is a bench/debug override that bypasses flash
    // entirely and is never itself persisted; DIP 0 means "normal mode".
    void applyNodeIdOverride()
    {
      uint8_t dipValue = board::dipSwitch.read();
      if (dipValue != 0)
      {
        canopen::persist::lssPendingNodeId = dipValue;
      }
    }

    uint8_t compactNmtState(CO_NMT_internalState_t state)
    {
      switch (state)
      {
      case CO_NMT_OPERATIONAL:
        return cli::kNmtOperational;
      case CO_NMT_STOPPED:
        return cli::kNmtStopped;
      case CO_NMT_PRE_OPERATIONAL:
        return cli::kNmtPreOperational;
      case CO_NMT_INITIALIZING:
      default:
        return cli::kNmtInitializing;
      }
    }

    // One-time allocation of the CANopenNode object graph -- called once
    // from canopenTask() before its communication-reset loop starts. Never
    // called again: CO_RESET_COMM re-runs bringUpCommunication() below on
    // the same CO_t (see CO_driver.c's scope note on CO_CANmodule_disable()).
    CO_t *createCanopenObject()
    {
      uint32_t heapMemoryUsed = 0;
      return CO_new(nullptr, &heapMemoryUsed);
    }

    // Runs (or re-runs, on CO_RESET_COMM) the full CANopenNode bring-up:
    // CAN init, node-ID resolution (flash + DIP override, spec §3), LSS,
    // NMT/HB/SDO/PDO init, OD 1010h/1011h storage wiring, normal mode.
    // Idempotent -- every step here is safe to repeat on the same `coRef`
    // (OD_extension_init() is a plain pointer assignment; CO_LSSinit()/
    // CO_CANopenInit()/CO_CANopenInitPDO() are designed by CANopenNode to be
    // re-run this way, see its own reference main loop in
    // lib/CANopenNode/vendor/example/main_blank.c).
    bool bringUpCommunication(CO_t *coRef, uint8_t *nodeId)
    {
      // Gate canopenDispatchRxFrame() (task_can.cpp, a different task) off
      // before touching anything below -- see CO_driver.c's scope note.
      coRef->CANmodule->CANnormal = false;
      CO_CANsetConfigurationMode(nullptr);
      CO_CANmodule_disable(coRef->CANmodule);

      if (CO_CANinit(coRef, nullptr, kBitrateKbps) != CO_ERROR_NO)
      {
        return false;
      }

      // Load the persisted A/B snapshot (spec §8), if any, before anything
      // reads OD_PERSIST_COMM or the LSS pending node-ID/bit-rate: this
      // populates canopen::persist::lssPendingNodeId/lssPendingBitRate and
      // overwrites OD_PERSIST_COMM's compiled defaults in place. Leaves
      // both at their compiled/default state on first boot or corrupt
      // flash (0xFF node-ID -- LSS waiting).
      canopen::persist::loadAtBoot();
      applyNodeIdOverride();

      // LSS slave is compiled in (CO_CONFIG_LSS_SLAVE default-enabled) and
      // is the addressing authority now (spec §3): CO_LSSinit() keeps a
      // pointer to persist::lssPendingNodeId/lssPendingBitRate for the
      // whole life of the node (see CO_storageFlash.h), which is why those
      // live in canopen::persist rather than as locals here.
      CO_LSS_address_t lssAddress{};
      lssAddress.identity.vendorID = OD_PERSIST_COMM.x1018_identity.vendor_ID;
      lssAddress.identity.productCode = OD_PERSIST_COMM.x1018_identity.productCode;
      lssAddress.identity.revisionNumber = OD_PERSIST_COMM.x1018_identity.revisionNumber;
      lssAddress.identity.serialNumber = OD_PERSIST_COMM.x1018_identity.serialNumber;
      if (CO_LSSinit(coRef, &lssAddress, &canopen::persist::lssPendingNodeId,
                      &canopen::persist::lssPendingBitRate) != CO_ERROR_NO)
      {
        return false;
      }
      // Persists node-ID/bit-rate to flash when the LSS master sends "store
      // configuration" (spec §3 commissioning flow, §8 storage table).
      CO_LSSslave_initCfgStoreCall(coRef->LSSslave, nullptr, canopen::persist::lssStoreCallback);

      *nodeId = canopen::persist::lssPendingNodeId;
      uint32_t errInfo = 0;
      CO_ReturnError_t err = CO_CANopenInit(coRef, nullptr, nullptr, OD, nullptr, kNmtControl, kFirstHbTimeMs,
                                             kSdoServerTimeoutMs, kSdoClientTimeoutMs, kSdoClientBlockTransfer,
                                             *nodeId, &errInfo);
      if (err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS)
      {
        return false;
      }

      err = CO_CANopenInitPDO(coRef, coRef->em, OD, *nodeId, &errInfo);
      if (err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS)
      {
        return false;
      }

      // On an LSS-unconfigured node coRef->em exists but is not initialized
      // (CO_CANopenInit() returns early) and cannot transmit anyway (no EMCY
      // COB-ID without a node-ID; CO_process() skips CO_EM_process()) --
      // reportFlashWriteFailure() is effectively a no-op until the node is
      // configured, and first-commissioning store failures surface through
      // the LSS "store configuration" error reply instead.
      canopen::persist::bindEmergency(coRef->em);

      if (!coRef->nodeIdUnconfigured)
      {
        uint32_t storageInitError = 0;
        canopen::persist::init(&storage, coRef->CANmodule, OD_ENTRY_H1010_storeParameters,
                                OD_ENTRY_H1011_restoreDefaultParameters, storageEntries,
                                sizeof(storageEntries) / sizeof(storageEntries[0]), &storageInitError);
      }

      CO_CANsetNormalMode(coRef->CANmodule);
      return true;
    }

    void reportNodeId(uint8_t nodeId)
    {
      Event ev{EventType::kCanopenNodeId, nodeId};
      xQueueSend(eventQueue, &ev, 0);
    }

    void reportNmtStateIfChanged(CO_NMT_internalState_t state, CO_NMT_internalState_t &lastReported)
    {
      if (state == lastReported)
      {
        return;
      }
      lastReported = state;
      Event ev{EventType::kNmtStateChanged, compactNmtState(state)};
      xQueueSend(eventQueue, &ev, 0);
    }

    void reportHeartbeatIfSent(uint32_t timer, uint32_t &lastTimer)
    {
      // HBproducerTimer counts down from the OD 1017h period to 0, then
      // resets -- an increase between two polls means a reset just
      // happened, i.e. a heartbeat was just sent. Heuristic, but safe: it
      // only drives a CLI counter, nothing protocol-critical.
      if (timer > lastTimer)
      {
        Event ev{EventType::kHeartbeatSent};
        xQueueSend(eventQueue, &ev, 0);
      }
      lastTimer = timer;
    }

    void reportLedStateIfChanged(uint8_t index, bool state, bool &lastState)
    {
      if (state == lastState)
      {
        return;
      }
      lastState = state;
      Event ev{EventType::kLedStateChanged, index, state};
      xQueueSend(eventQueue, &ev, 0);
    }

    // Drives LED1/LED2 as the CiA 303-3 indicator pair (green run / red
    // error) computed by CANopenNode's LEDs module (CO_CONFIG_LEDS is
    // enabled by default and not overridden -- see
    // src/app/canopen/CO_driver_target.h), instead of leaving that output
    // unread. Must run every cycle, not just on change, since the module's
    // blink/flicker/flash patterns need continuous re-evaluation of the
    // same logical state.
    void applyLedIndicators(const CO_t *co, bool &lastLed1, bool &lastLed2)
    {
      if (co->LEDs == nullptr)
      {
        return;
      }

      bool runOn = CO_LED_GREEN(co->LEDs, CO_LED_CANopen) != 0;
      bool errorOn = CO_LED_RED(co->LEDs, CO_LED_CANopen) != 0;
      board::led1.set(runOn);
      board::led2.set(errorOn);
      reportLedStateIfChanged(0, runOn, lastLed1);
      reportLedStateIfChanged(1, errorOn, lastLed2);
    }

    void canopenTask(void *)
    {
      CO_t *co = createCanopenObject();
      if (co == nullptr)
      {
        for (;;)
        {
          vTaskDelay(portMAX_DELAY);
        }
      }

      // Outer "communication reset" loop, matching CANopenNode's own
      // reference main loop (lib/CANopenNode/vendor/example/main_blank.c).
      // CO_process() returns CO_RESET_COMM once LSS commissioning completes
      // -- the master sends "switch state global -> waiting" after storing
      // the new node-ID, which CO_LSSslave.c turns into exactly this reset
      // request -- or on an NMT "reset communication" command; CO_RESET_APP
      // on an NMT "reset node" command. Re-running bringUpCommunication() on
      // the same `co` brings the freshly-configured node-ID up live, no
      // power cycle needed.
      for (;;)
      {
        uint8_t nodeId = 0;
        if (!bringUpCommunication(co, &nodeId))
        {
          // Nothing productive to do without a working stack -- surface
          // nothing further and idle, rather than spin retrying a bring-up
          // that already failed once against the same hardware/config.
          for (;;)
          {
            vTaskDelay(portMAX_DELAY);
          }
        }

        reportNodeId(nodeId);

        CO_NMT_internalState_t lastNmtState = CO_NMT_INITIALIZING;
        uint32_t lastHbTimer = 0;
        uint8_t lastLed1Pattern = cli::kLedPatternOff;
        uint8_t lastLed2Pattern = cli::kLedPatternOff;
        uint32_t lastCallMicros = micros();

        CO_NMT_reset_cmd_t resetCmd = CO_RESET_NOT;
        TickType_t lastWake = xTaskGetTickCount();
        while (resetCmd == CO_RESET_NOT)
        {
          vTaskDelayUntil(&lastWake, kProcessPeriod);

          uint32_t nowMicros = micros();
          uint32_t timeDifferenceUs = nowMicros - lastCallMicros;
          lastCallMicros = nowMicros;

          resetCmd = CO_process(co, /*enableGateway=*/false, timeDifferenceUs, nullptr);

          if (co->NMT != nullptr)
          {
            reportNmtStateIfChanged(co->NMT->operatingState, lastNmtState);
            reportHeartbeatIfSent(co->NMT->HBproducerTimer, lastHbTimer);
          }

          applyLedIndicators(co, lastLed1Pattern, lastLed2Pattern);
        }

        applyLedIndicators(co, lastLed1State, lastLed2State);
        if (resetCmd == CO_RESET_APP)
        {
          // NMT "reset node": this project has no in-place teardown of
          // application-level state, so reboot the MCU outright -- setup()
          // reruns board::init() and this task from a clean slate, the same
          // place CO_RESET_COMM's in-place re-init would otherwise reach.
          rp2040.reboot();
        }
        // else CO_RESET_COMM: loop and re-run bringUpCommunication().
      }
    }

  } // namespace

  void startCanopenTask()
  {
    xTaskCreate(canopenTask, "canopen", 2048, nullptr, /*priority=*/2, nullptr);
  }

} // namespace app
