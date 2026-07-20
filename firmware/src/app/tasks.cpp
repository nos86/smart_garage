#include "tasks.h"

namespace app
{

  QueueHandle_t eventQueue = nullptr;
  TaskHandle_t ultrasonicTaskHandle = nullptr;
  DebugControls debugControls{};

  void startTasks()
  {
    eventQueue = xQueueCreate(16, sizeof(Event));

    // Control first so no early event is dropped, then producers.
    startControlTask();
    startUltrasonicTask();
    startSensorsTask();
  }

} // namespace app
