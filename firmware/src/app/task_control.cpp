#include <Arduino.h>

#include "serial_cli.h"
#include "tasks.h"

// Consumes events from the queue and forwards them to the dedicated serial CLI
// renderer. The UI lives in src/app/serial_cli.* and src/app/cli/ so the
// control task stays focused on event dispatch.

namespace app
{

  namespace
  {

    SerialCli cli;

    void controlTask(void *)
    {
      cli.begin();

      for (;;)
      {
        Event ev;
        if (xQueueReceive(eventQueue, &ev, pdMS_TO_TICKS(20)) == pdTRUE)
        {
          cli.handleEvent(ev);
        }
        cli.pollInput();
      }
    }

  } // namespace

  void startControlTask()
  {
    xTaskCreate(controlTask, "control", 2048, nullptr, /*priority=*/2, nullptr);
  }

} // namespace app
