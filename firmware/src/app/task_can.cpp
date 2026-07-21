#include <Arduino.h>

#include "board.h"
#include "canopen/canopen_bridge.h"
#include "hal/pins.h"
#include "tasks.h"

// Interrupt-driven CAN RX: the MCP25625's INT line (active-low) wakes this
// task via a task notification given from the ISR -- SPI cannot be touched
// from interrupt context, so the ISR only notifies, and the actual RX
// buffer drain happens here at task priority. canBusMutex is shared with
// any CAN TX path (CANopenNode's CO_CANsend, once wired) since both sides
// touch the same non-thread-safe SPI device.

namespace app
{

  namespace
  {

    TaskHandle_t canTaskHandle_ = nullptr;

    void canIsr()
    {
      BaseType_t woken = pdFALSE;
      vTaskNotifyGiveFromISR(canTaskHandle_, &woken);
      portYIELD_FROM_ISR(woken);
    }

    void reportFrame()
    {
      Event ev{EventType::kCanFrameReceived};
      xQueueSend(eventQueue, &ev, 0);
    }

    void reportErrorIfAny()
    {
      uint8_t flags;
      xSemaphoreTake(canBusMutex, portMAX_DELAY);
      flags = board::can.errorFlags();
      if (flags != 0)
      {
        board::can.clearErrors();
      }
      xSemaphoreGive(canBusMutex);

      if (flags != 0)
      {
        Event ev{EventType::kCanError, /*index=*/flags};
        xQueueSend(eventQueue, &ev, 0);
      }
    }

    void canTask(void *)
    {
      pinMode(hal::pins::kCanInt, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(hal::pins::kCanInt), canIsr, FALLING);

      for (;;)
      {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // A single IRQ can represent more than one pending frame (the
        // MCP25625 has two RX buffers) -- drain until empty.
        for (;;)
        {
          hal::CanFrame frame;
          xSemaphoreTake(canBusMutex, portMAX_DELAY);
          bool got = board::can.receive(frame);
          xSemaphoreGive(canBusMutex);
          if (!got)
          {
            break;
          }
          canopenDispatchRxFrame(frame.id, frame.dlc, frame.data);
          reportFrame();
        }

        reportErrorIfAny();
      }
    }

  } // namespace

  void startCanTask()
  {
    xTaskCreate(canTask, "can_rx", 1024, nullptr, /*priority=*/3, &canTaskHandle_);
  }

} // namespace app
