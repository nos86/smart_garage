#pragma once

#include <Arduino.h>

namespace hal {

// Simple GPIO-driven status LED (LED1/LED2 on the carrier board), active-high.
class Led {
 public:
  explicit Led(uint8_t pin);

  void begin();
  void on();
  void off();
  void set(bool lit);
  void toggle();
  bool isOn() const;

 private:
  uint8_t pin_;
  bool state_ = false;
};

}  // namespace hal
