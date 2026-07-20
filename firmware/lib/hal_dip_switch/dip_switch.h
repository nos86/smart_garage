#pragma once

#include <Arduino.h>

#include "digital_input.h"
#include "hal/logic/dip_switch_decode.h"

namespace hal
{

  // 5-position hardware configuration DIP switch (SW1, five microswitches),
  // decoded into a single 0-31 value.
  class DipSwitch
  {
  public:
    explicit DipSwitch(const uint8_t (&pins)[5]);

    void begin();
    uint8_t read();                // decoded value, bit0 = switch 1 .. bit4 = switch 5
    bool getSwitch(uint8_t index); // raw state of switch at index [0..4]

  private:
    DigitalInput switches_[5];
  };

} // namespace hal
