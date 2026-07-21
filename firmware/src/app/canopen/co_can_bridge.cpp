// C-linkage bridge letting CO_driver.c (plain C, part of the CANopenNode
// glue) reach the app-level hal::CanTransceiver instance and its shared
// mutex, both of which are C++ (namespace app::board / app::canBusMutex).
// This is the only file in src/app/canopen/ that touches board.h/tasks.h
// directly -- CO_driver.c itself only sees this small extern "C" surface.

#include "../board.h"
#include "../tasks.h"

extern "C" {

void coCanLockSend() { xSemaphoreTake(app::canBusMutex, portMAX_DELAY); }

void coCanUnlockSend() { xSemaphoreGive(app::canBusMutex); }

bool coCanBridgeSend(uint32_t ident, uint8_t dlc, const uint8_t data[8]) {
  hal::CanFrame frame{};
  frame.id = ident;
  frame.dlc = dlc > 8 ? 8 : dlc;
  for (uint8_t i = 0; i < frame.dlc; ++i) {
    frame.data[i] = data[i];
  }
  return app::board::can.send(frame);
}

}  // extern "C"
