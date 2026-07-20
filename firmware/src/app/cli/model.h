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
        constexpr uint8_t kTabCount = 3;

        constexpr uint8_t kOutputCount = 5;

        // Raw values of app::ForceMode (tasks.h), mirrored here so this layer
        // needs no FreeRTOS includes. serial_cli.cpp static_asserts they stay
        // in sync.
        constexpr uint8_t kModeAuto = 0;
        constexpr uint8_t kModeOff = 1;
        constexpr uint8_t kModeOn = 2;

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
            uint8_t activeTab = kTabStatus;
            uint8_t selection[kTabCount] = {};
        };

        inline uint8_t activeSelection(const Model &model)
        {
            return model.selection[model.activeTab];
        }

    } // namespace cli
} // namespace app
