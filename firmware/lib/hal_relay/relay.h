#pragma once

#include <Arduino.h>

namespace hal {

// Single relay channel driven through a gate MOSFET (active-high energizes
// the coil).
class Relay {
 public:
  explicit Relay(uint8_t pin);

  void begin();  // configures the pin and de-energizes the relay
  void on();
  void off();
  void set(bool energized);
  bool isOn() const;

 private:
  uint8_t pin_;
  bool state_ = false;
};

}  // namespace hal
