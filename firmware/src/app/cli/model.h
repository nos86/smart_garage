#pragma once

// Pure-data snapshot of everything the serial CLI shows. No Arduino or
// FreeRTOS dependencies, so the pages layer stays hardware-agnostic. The
// renderer keeps the last-drawn copy and diffs against the current one to
// repaint only the fields that changed.

#include <cstdint>

namespace app
{
    namespace cli
    {

        constexpr uint8_t kTabStatus = 0;
        constexpr uint8_t kTabInputs = 1;
        constexpr uint8_t kTabOutputs = 2;
        constexpr uint8_t kTabRtos = 3;
        constexpr uint8_t kTabCan = 4;
        constexpr uint8_t kTabCanOpen = 5;
        constexpr uint8_t kTabCount = 6;

        // Compact NMT state encoding chosen by this app layer (not
        // CANopenNode's own CO_NMT_internalState_t raw values, which this
        // layer stays free of so it needs no CANopenNode include). Mapped
        // in src/app/task_canopen.cpp when it emits EventType::kNmtStateChanged.
        constexpr uint8_t kNmtInitializing = 0;
        constexpr uint8_t kNmtPreOperational = 1;
        constexpr uint8_t kNmtOperational = 2;
        constexpr uint8_t kNmtStopped = 3;

        // CiA 303-3 indicator patterns, named after the standard's own
        // vocabulary (see lib/CANopenNode/vendor/303/CO_LEDs.h). Computed in
        // task_canopen.cpp from NMT state/LSS/error inputs -- the same
        // priority chain CANopenNode's CO_LEDs_process() uses internally --
        // since the library only exposes the raw, fast-toggling on/off
        // level of the currently selected pattern, not which pattern is
        // selected.
        constexpr uint8_t kLedPatternOff = 0;
        constexpr uint8_t kLedPatternFlicker = 1;
        constexpr uint8_t kLedPatternBlink = 2;
        constexpr uint8_t kLedPatternFlash1 = 3;
        constexpr uint8_t kLedPatternFlash2 = 4;
        constexpr uint8_t kLedPatternFlash3 = 5;
        constexpr uint8_t kLedPatternFlash4 = 6;
        constexpr uint8_t kLedPatternOn = 7;

        // Forceable outputs on the OUTPUTS tab: relay1, relay2, onboard RGB.
        // LED1/LED2 are excluded -- they are CiA 303-3 run/error indicators
        // driven by the CANopen stack (see task_canopen.cpp), not
        // user-forceable, and shown read-only on the CANOPEN tab instead.
        constexpr uint8_t kOutputCount = 3;

        // Raw bit values of MCP2515::EFLG (autowp-mcp2515's can.h/mcp2515.h),
        // mirrored here so this layer needs no HAL/driver includes.
        // serial_cli.cpp static_asserts they stay in sync.
        constexpr uint8_t kCanEflgRx1Ovr = 1u << 7;
        constexpr uint8_t kCanEflgRx0Ovr = 1u << 6;
        constexpr uint8_t kCanEflgTxbo = 1u << 5;
        constexpr uint8_t kCanEflgTxep = 1u << 4;
        constexpr uint8_t kCanEflgRxep = 1u << 3;
        constexpr uint8_t kCanEflgTxwar = 1u << 2;
        constexpr uint8_t kCanEflgRxwar = 1u << 1;
        constexpr uint8_t kCanEflgEwarn = 1u << 0;

        // Raw values of app::ForceMode (tasks.h), mirrored here so this layer
        // needs no FreeRTOS includes. serial_cli.cpp static_asserts they stay
        // in sync.
        constexpr uint8_t kModeAuto = 0;
        constexpr uint8_t kModeOff = 1;
        constexpr uint8_t kModeOn = 2;

        // Raw values of FreeRTOS' eTaskState, mirrored here so this layer
        // needs no FreeRTOS includes. serial_cli.cpp static_asserts they stay
        // in sync.
        constexpr uint8_t kTaskStateRunning = 0;
        constexpr uint8_t kTaskStateReady = 1;
        constexpr uint8_t kTaskStateBlocked = 2;
        constexpr uint8_t kTaskStateSuspended = 3;
        constexpr uint8_t kTaskStateDeleted = 4;
        constexpr uint8_t kTaskStateInvalid = 5;

        // Upper bound on tasks shown on the RTOS page (app tasks, FreeRTOS
        // timer/idle tasks, arduino-pico's per-core wrapper/idle tasks, the
        // USB task -- ~10 on this firmware). serial_cli.cpp's
        // uxTaskGetSystemState() call needs a buffer sized for *all* tasks
        // or it returns zero rows, not a truncated list, so keep this above
        // the real task count with some headroom if more tasks get added.
        constexpr uint8_t kMaxRtosTasks = 12;
        // configMAX_TASK_NAME_LEN (FreeRTOSConfig.h) is 10 incl. NUL, so a
        // FreeRTOS task name is never longer than 9 chars to begin with --
        // match that exactly so this layer does not clip it further.
        constexpr uint8_t kTaskNameMax = 10;

        struct RtosTaskInfo
        {
            char name[kTaskNameMax] = {};
            uint8_t state = kTaskStateInvalid;
            uint8_t priority = 0;
            uint32_t stackFreeWords = 0;
            uint8_t cpuPercent = 0;
        };

        struct Model
        {
            bool input1 = false;
            bool input2 = false;
            bool pir = false;
            uint8_t dipValue = 0;
            bool relay1 = false;
            bool relay2 = false;
            // CiA 303-3 green run / red error indicator patterns
            // (task_canopen.cpp), read-only. One of the kLedPattern* values
            // above, not an instantaneous on/off level -- shown only on the
            // CANOPEN tab, not on STATUS.
            uint8_t led1Pattern = kLedPatternOff;
            uint8_t led2Pattern = kLedPatternOff;
            bool onboard = false;
            float distanceCm = -1.0f;
            bool ultrasonicTimeout = false;
            bool ultrasonicPowered = false;
            uint8_t ultrasonicMode = kModeAuto;
            RtosTaskInfo rtosTasks[kMaxRtosTasks] = {};
            uint8_t rtosTaskCount = 0;
            uint32_t rtosFreeHeapBytes = 0;
            uint32_t rtosTotalHeapBytes = 0;
            uint32_t rtosUptimeMs = 0;
            bool canOk = false;
            uint32_t canBitrateKbps = 0;
            uint32_t canComputedBaudrateBps = 0;
            uint32_t canRxCount = 0;
            uint32_t canTxCount = 0;
            uint32_t canRxRatePs = 0;
            uint32_t canTxRatePs = 0;
            uint8_t canTxErrorCount = 0;
            uint8_t canRxErrorCount = 0;
            uint8_t canErrorFlags = 0;
            uint8_t nmtState = kNmtInitializing;
            uint8_t canopenNodeId = 0;
            uint32_t heartbeatCount = 0;
            uint32_t lastHeartbeatMs = 0;
            uint8_t activeTab = kTabStatus;
            uint8_t selection[kTabCount] = {};
        };

        inline uint8_t activeSelection(const Model &model)
        {
            return model.selection[model.activeTab];
        }

    } // namespace cli
} // namespace app
