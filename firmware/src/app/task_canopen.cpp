#include <Arduino.h>

#include "board.h"
#include "canopen/CO_storageBlank.h"
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

    // NMT_CONTROL: self-start to Operational without waiting for a master,
    // plus the stack's own recommended default error-reactive behavior
    // (drop back to pre-operational/stopped on bus-off, HB consumer
    // timeout, or a nonzero masked error register) -- matches
    // CANopenNode's own reference main_blank.c default.
    constexpr uint16_t kNmtControl = CO_NMT_STARTUP_TO_OPERATIONAL | CO_NMT_ERR_ON_ERR_REG | CO_ERR_REG_GENERIC_ERR |
                                      CO_ERR_REG_COMMUNICATION;
    constexpr uint16_t kFirstHbTimeMs = 500;
    constexpr uint16_t kSdoServerTimeoutMs = 1000;
    constexpr uint16_t kSdoClientTimeoutMs = 500;
    constexpr bool kSdoClientBlockTransfer = false;
    constexpr uint16_t kBitrateKbps = 250;
    constexpr TickType_t kProcessPeriod = pdMS_TO_TICKS(10);

    uint8_t nodeIdFromDip()
    {
      // CANopen node-IDs are 1-127 (0 is reserved/unconfigured); the DIP
      // switch gives a direct 0-31 value (hal::DipSwitch), so +1 avoids 0.
      // Provisional, direct mapping -- the real node-ID/addressing scheme
      // for the 5-bit DIP is still open (architecture-plan.md P0 backlog).
      return static_cast<uint8_t>(board::dipSwitch.read() + 1);
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

    bool initCanopen(CO_t **co, uint8_t *nodeId)
    {
      uint32_t heapMemoryUsed = 0;
      *co = CO_new(nullptr, &heapMemoryUsed);
      if (*co == nullptr)
      {
        return false;
      }
      CO_t *coRef = *co;

      CO_CANsetConfigurationMode(nullptr);

      if (CO_CANinit(coRef, nullptr, kBitrateKbps) != CO_ERROR_NO)
      {
        return false;
      }

      // LSS slave is compiled in (CO_CONFIG_LSS_SLAVE default-enabled), so
      // it must be initialized even though this bring-up has no LSS master
      // driving runtime node-ID/bitrate reconfiguration -- pendingNodeId
      // starts at (and, absent a master, stays at) the DIP-derived value.
      uint8_t pendingNodeId = nodeIdFromDip();
      uint16_t pendingBitRate = kBitrateKbps;
      CO_LSS_address_t lssAddress{};
      lssAddress.identity.vendorID = OD_PERSIST_COMM.x1018_identity.vendor_ID;
      lssAddress.identity.productCode = OD_PERSIST_COMM.x1018_identity.productCode;
      lssAddress.identity.revisionNumber = OD_PERSIST_COMM.x1018_identity.revisionNumber;
      lssAddress.identity.serialNumber = OD_PERSIST_COMM.x1018_identity.serialNumber;
      if (CO_LSSinit(coRef, &lssAddress, &pendingNodeId, &pendingBitRate) != CO_ERROR_NO)
      {
        return false;
      }

      *nodeId = pendingNodeId;
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

      if (!coRef->nodeIdUnconfigured)
      {
        CO_storage_t storage{};
        CO_storage_entry_t storageEntries[] = {{.addr = &OD_PERSIST_COMM,
                                                 .len = sizeof(OD_PERSIST_COMM),
                                                 .subIndexOD = 2,
                                                 .attr = CO_storage_cmd | CO_storage_restore,
                                                 .addrNV = nullptr}};
        uint32_t storageInitError = 0;
        CO_storageBlank_init(&storage, coRef->CANmodule, OD_ENTRY_H1010_storeParameters,
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
      CO_t *co = nullptr;
      uint8_t nodeId = 0;
      if (!initCanopen(&co, &nodeId))
      {
        // Nothing productive to do without a working stack -- surface
        // nothing further and idle, rather than spin retrying an init that
        // already failed once against the same hardware/config.
        for (;;)
        {
          vTaskDelay(portMAX_DELAY);
        }
      }

      reportNodeId(nodeId);

      CO_NMT_internalState_t lastNmtState = CO_NMT_INITIALIZING;
      uint32_t lastHbTimer = 0;
      bool lastLed1State = false;
      bool lastLed2State = false;
      uint32_t lastCallMicros = micros();

      TickType_t lastWake = xTaskGetTickCount();
      for (;;)
      {
        vTaskDelayUntil(&lastWake, kProcessPeriod);

        uint32_t nowMicros = micros();
        uint32_t timeDifferenceUs = nowMicros - lastCallMicros;
        lastCallMicros = nowMicros;

        CO_process(co, /*enableGateway=*/false, timeDifferenceUs, nullptr);

        if (co->NMT != nullptr)
        {
          reportNmtStateIfChanged(co->NMT->operatingState, lastNmtState);
          reportHeartbeatIfSent(co->NMT->HBproducerTimer, lastHbTimer);
        }

        applyLedIndicators(co, lastLed1State, lastLed2State);
      }
    }

  } // namespace

  void startCanopenTask()
  {
    xTaskCreate(canopenTask, "canopen", 2048, nullptr, /*priority=*/2, nullptr);
  }

} // namespace app
