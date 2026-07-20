#include "dip_switch.h"

namespace hal
{

  namespace
  {
    DigitalInput makeSwitch(uint8_t pin)
    {
      return DigitalInput(pin, /*activeLow=*/true, /*debounceMs=*/20,
                          /*enableInternalPullup=*/true);
    }
  } // namespace

  DipSwitch::DipSwitch(const uint8_t (&pins)[5])
      : switches_{makeSwitch(pins[0]), makeSwitch(pins[1]), makeSwitch(pins[2]), makeSwitch(pins[3]),
                  makeSwitch(pins[4])} {}

  void DipSwitch::begin()
  {
    for (auto &sw : switches_)
    {
      sw.begin();
    }
  }

  uint8_t DipSwitch::read()
  {
    bool bits[5];
    for (uint8_t i = 0; i < 5; ++i)
    {
      bits[i] = switches_[i].read();
    }
    return logic::dip_switch_decode(bits);
  }

  bool DipSwitch::getSwitch(uint8_t index)
  {
    if (index >= 5)
      return false;
    return switches_[index].read();
  }

} // namespace hal
