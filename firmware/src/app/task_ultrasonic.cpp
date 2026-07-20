#include <Arduino.h>

#include "board.h"
#include "hal/logic/ultrasonic_schedule.h"
#include "tasks.h"

// Measures distance at the rate decided by UltrasonicScheduler: baseline
// 1/min, boosted to 1/s for a sliding 60 s window after each stimulus
// (delivered as a task notification from the sensors/control tasks).
//
// The sensor's load switch is only kept powered while in the active window;
// in baseline mode it is powered per-measurement to honor the standby power
// budget.

namespace app
{

  namespace
  {

    constexpr uint32_t kForcedPeriodMs = 1000;
    constexpr TickType_t kAutoPowerServicePeriod = pdMS_TO_TICKS(100);

    bool isMode(ForceMode mode, ForceMode expected)
    {
      return mode == expected;
    }

    void measureAndReport()
    {
      float cm = board::ultrasonic.measureDistanceCm();
      Event ev{};
      if (cm >= 0)
      {
        ev.type = EventType::kUltrasonicReading;
        ev.distanceCm = cm;
      }
      else
      {
        ev.type = EventType::kUltrasonicTimeout;
      }
      xQueueSend(eventQueue, &ev, 0);
    }

    void ultrasonicTask(void *)
    {
      hal::logic::UltrasonicScheduler scheduler;

      for (;;)
      {
        ForceMode mode = static_cast<ForceMode>(debugControls.ultrasonicMode.load());
        bool measureNow = debugControls.ultrasonicMeasureNow.exchange(false);

        board::ultrasonic.setAlwaysOn(isMode(mode, ForceMode::kOn));
        board::ultrasonic.setAutoPowerCutEnabled(isMode(mode, ForceMode::kAuto));

        if (isMode(mode, ForceMode::kOff) && !measureNow)
        {
          board::ultrasonic.powerOff();
          if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0)
          {
            scheduler.onStimulus(millis());
          }
          continue;
        }

        uint32_t now = millis();
        bool active = isMode(mode, ForceMode::kOn) || scheduler.isActive(now);

        measureAndReport();
        board::ultrasonic.servicePower();

        // Sleep until the next scheduled measurement, but wake early on a
        // stimulus notification so the active window starts immediately.
        TickType_t timeout = isMode(mode, ForceMode::kOn) ? pdMS_TO_TICKS(kForcedPeriodMs)
                                                          : pdMS_TO_TICKS(scheduler.periodMs(millis()));
        if (!isMode(mode, ForceMode::kOn) && board::ultrasonic.isPowered())
        {
          timeout = (timeout < kAutoPowerServicePeriod) ? timeout : kAutoPowerServicePeriod;
        }
        if (ulTaskNotifyTake(pdTRUE, timeout) > 0 && !isMode(mode, ForceMode::kOn))
        {
          scheduler.onStimulus(millis());
        }

        if (!active && !measureNow)
        {
          board::ultrasonic.servicePower();
        }
      }
    }

  } // namespace

  void startUltrasonicTask()
  {
    xTaskCreate(ultrasonicTask, "ultrasonic", 1024, nullptr, /*priority=*/2, &ultrasonicTaskHandle);
  }

} // namespace app
