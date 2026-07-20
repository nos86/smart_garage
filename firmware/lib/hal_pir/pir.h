#pragma once

#include <Arduino.h>

#include "digital_input.h"

namespace hal {

// PIR motion sensor, active-high (hardware-filtered via series R + pull-down
// + 100nF, see hardware/README.md).
class PirSensor {
 public:
  explicit PirSensor(uint8_t pin);

  void begin();
  bool motionDetected();     // current debounced state
  bool motionJustStarted();  // true exactly once on the rising edge

 private:
  DigitalInput input_;
  bool wasActive_ = false;
};

}  // namespace hal
