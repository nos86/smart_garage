#pragma once

#include <Arduino.h>

#include "hal/logic/debounce.h"

namespace hal
{

  // Debounced digital input with active-high/active-low logical inversion.
  // Used directly for the isolated field inputs (INPUT1/INPUT2), and reused by
  // PirSensor and DipSwitch.
  class DigitalInput
  {
  public:
    DigitalInput(uint8_t pin, bool activeLow, uint32_t debounceMs = 20,
                 bool enableInternalPullup = false);

    void begin();

    // Debounced logical state (true = active), advancing the debounce state
    // machine using the current time. Call this periodically from loop().
    bool read();

  private:
    bool readLogicalRaw() const;

    uint8_t pin_;
    bool activeLow_;
    uint32_t debounceMs_;
    bool enableInternalPullup_;
    logic::DebounceState state_;
  };

} // namespace hal
