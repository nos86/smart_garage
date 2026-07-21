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

        constexpr uint8_t kOutputCount = 5;

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
            bool led1 = false;
            bool led2 = false;
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
            uint32_t canRxCount = 0;
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
