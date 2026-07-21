#pragma once

// Task topology:
//
//   sensorsTask ----\                       (5 ms polling: inputs, PIR, DIP)
//                    +--> eventQueue --> controlTask --> relays/LEDs/UART
//   ultrasonicTask -/                        |
//        ^-- stimulusNotify (task notification) on PIR/input activity
//
//   canTask ---------------------------------> eventQueue --> controlTask
//        ^-- MCP_INT GPIO IRQ (task notification), drains board::can
//
// All UART output goes through controlTask only, so no serial mutex is
// needed. canBusMutex guards board::can itself, since canTask's RX drain
// and any CAN TX path (CLI-triggered sends, and CANopenNode's CO_CANsend in
// a later phase) run from different tasks against the same SPI device.

#include <FreeRTOS.h>
#include <atomic>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "events.h"

namespace app
{

    enum class ForceMode : uint8_t
    {
        kAuto = 0,
        kOff = 1,
        kOn = 2,
    };

    struct DebugControls
    {
        std::atomic<uint8_t> ultrasonicMode{static_cast<uint8_t>(ForceMode::kAuto)};
        std::atomic<bool> ultrasonicMeasureNow{false};
    };

    extern QueueHandle_t eventQueue;
    extern TaskHandle_t ultrasonicTaskHandle;
    extern SemaphoreHandle_t canBusMutex;
    extern DebugControls debugControls;

    // Creates the queue and all tasks. Call once from setup(), after board::init().
    void startTasks();

    // Individual task starters, called by startTasks().
    void startSensorsTask();
    void startUltrasonicTask();
    void startControlTask();
    void startCanTask();
    void startCanopenTask();

} // namespace app
