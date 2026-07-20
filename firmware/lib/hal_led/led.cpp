#include "led.h"

namespace hal {

Led::Led(uint8_t pin) : pin_(pin) {}

void Led::begin() {
  pinMode(pin_, OUTPUT);
  off();
}

void Led::on() { set(true); }

void Led::off() { set(false); }

void Led::set(bool lit) {
  state_ = lit;
  digitalWrite(pin_, lit ? HIGH : LOW);
}

void Led::toggle() { set(!state_); }

bool Led::isOn() const { return state_; }

}  // namespace hal
