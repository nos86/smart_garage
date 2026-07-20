#include "relay.h"

namespace hal {

Relay::Relay(uint8_t pin) : pin_(pin) {}

void Relay::begin() {
  pinMode(pin_, OUTPUT);
  off();
}

void Relay::on() { set(true); }

void Relay::off() { set(false); }

void Relay::set(bool energized) {
  state_ = energized;
  digitalWrite(pin_, energized ? HIGH : LOW);
}

bool Relay::isOn() const { return state_; }

}  // namespace hal
