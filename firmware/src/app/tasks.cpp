#include "tasks.h"

namespace app
{

  QueueHandle_t eventQueue = nullptr;
  TaskHandle_t ultrasonicTaskHandle = nullptr;
  SemaphoreHandle_t canBusMutex = nullptr;
  DebugControls debugControls{};

  void startTasks()
  {
    eventQueue = xQueueCreate(16, sizeof(Event));
    canBusMutex = xSemaphoreCreateMutex();

    // Control first so no early event is dropped, then producers.
    startControlTask();
    startUltrasonicTask();
    startSensorsTask();
    startCanTask();
    startCanopenTask();
  }

} // namespace app
