#include <Arduino.h>

#include "board.h"
#include "tasks.h"

// Polls the debounced inputs, PIR and DIP switch every 5 ms and turns state
// changes into events. Polling at 5 ms keeps the 20 ms software debounce
// sampled well above its time resolution.

namespace app
{

  namespace
  {

    constexpr TickType_t kPollPeriod = pdMS_TO_TICKS(5);

    bool pirState()
    {
      return board::pir.motionDetected();
    }

    void notifyStimulus()
    {
      if (ultrasonicTaskHandle != nullptr)
      {
        xTaskNotifyGive(ultrasonicTaskHandle);
      }
    }

    void sensorsTask(void *)
    {
      bool lastInput1 = board::input1.read();
      bool lastInput2 = board::input2.read();
      bool lastPir = pirState();
      uint8_t lastDip = board::dipSwitch.read();

      TickType_t lastWake = xTaskGetTickCount();
      for (;;)
      {
        vTaskDelayUntil(&lastWake, kPollPeriod);

        if (board::pir.motionJustStarted())
        {
          Event ev{EventType::kPirMotionStarted};
          xQueueSend(eventQueue, &ev, 0);
        }

        bool pir = pirState();
        if (pir != lastPir)
        {
          lastPir = pir;
          Event ev{EventType::kPirChanged, /*index=*/0, /*state=*/pir};
          xQueueSend(eventQueue, &ev, 0);
        }

        bool in1 = board::input1.read();
        if (in1 != lastInput1)
        {
          lastInput1 = in1;
          Event ev{EventType::kInputChanged, /*index=*/0, /*state=*/in1};
          xQueueSend(eventQueue, &ev, 0);
          notifyStimulus();
        }

        bool in2 = board::input2.read();
        if (in2 != lastInput2)
        {
          lastInput2 = in2;
          Event ev{EventType::kInputChanged, /*index=*/1, /*state=*/in2};
          xQueueSend(eventQueue, &ev, 0);
          notifyStimulus();
        }

        uint8_t dip = board::dipSwitch.read();
        if (dip != lastDip)
        {
          lastDip = dip;
          Event ev{EventType::kDipChanged, /*index=*/dip};
          xQueueSend(eventQueue, &ev, 0);
        }
      }
    }

  } // namespace

  void startSensorsTask()
  {
    xTaskCreate(sensorsTask, "sensors", 1024, nullptr, /*priority=*/3, nullptr);
  }

} // namespace app
