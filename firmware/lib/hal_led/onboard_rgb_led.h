#pragma once

#include <Adafruit_NeoPixel.h>
#include <cstdint>

namespace hal {

// Onboard single-pixel WS2812 RGB LED present on the RP2040-Tiny module
// itself (as opposed to the carrier-board LED1/LED2, see Led).
class OnboardRgbLed {
 public:
  explicit OnboardRgbLed(uint8_t pin);

  void begin();
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  void off();
  void setBrightness(uint8_t brightness);

 private:
  Adafruit_NeoPixel pixel_;
};

}  // namespace hal
