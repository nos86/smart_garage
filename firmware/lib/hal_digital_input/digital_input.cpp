#include "digital_input.h"

namespace hal
{

  DigitalInput::DigitalInput(uint8_t pin, bool activeLow, uint32_t debounceMs,
                             bool enableInternalPullup)
      : pin_(pin),
        activeLow_(activeLow),
        debounceMs_(debounceMs),
        enableInternalPullup_(enableInternalPullup) {}

  void DigitalInput::begin()
  {
    pinMode(pin_, enableInternalPullup_ ? INPUT_PULLUP : INPUT);
  }

  bool DigitalInput::readLogicalRaw() const
  {
    bool level = digitalRead(pin_) == HIGH;
    return activeLow_ ? !level : level;
  }

  bool DigitalInput::read()
  {
    return logic::debounce_update(state_, readLogicalRaw(), millis(), debounceMs_);
  }

} // namespace hal
