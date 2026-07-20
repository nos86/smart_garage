#include "pir.h"

namespace hal {

PirSensor::PirSensor(uint8_t pin) : input_(pin, /*activeLow=*/false, /*debounceMs=*/20) {}

void PirSensor::begin() { input_.begin(); }

bool PirSensor::motionDetected() { return input_.read(); }

bool PirSensor::motionJustStarted() {
  bool active = motionDetected();
  bool edge = active && !wasActive_;
  wasActive_ = active;
  return edge;
}

}  // namespace hal
