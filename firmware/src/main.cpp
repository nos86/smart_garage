#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

#include "app/board.h"
#include "app/tasks.h"

// RTOS-based HAL bring-up. Including FreeRTOS.h with the earlephilhower
// Arduino-Pico core links its bundled FreeRTOS SMP port: setup()/loop() then
// run inside a FreeRTOS task and the scheduler is already started.

namespace
{

  void initUsbSerial()
  {
    Serial.begin(115200);

    uint32_t startMs = millis();
    while (!Serial && (millis() - startMs) < 2000)
    {
      delay(10);
    }
  }

} // namespace

void setup()
{
  initUsbSerial();

  app::board::init();
  app::startTasks();
}

void loop()
{
  // All work happens in the app tasks; park the loop task.
  vTaskDelay(portMAX_DELAY);
}
