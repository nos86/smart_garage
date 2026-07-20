#include "onboard_rgb_led.h"

namespace hal {

OnboardRgbLed::OnboardRgbLed(uint8_t pin) : pixel_(1, pin, NEO_GRB + NEO_KHZ800) {}

void OnboardRgbLed::begin() {
  pixel_.begin();
  off();
}

void OnboardRgbLed::setColor(uint8_t r, uint8_t g, uint8_t b) {
  pixel_.setPixelColor(0, pixel_.Color(r, g, b));
  pixel_.show();
}

void OnboardRgbLed::off() { setColor(0, 0, 0); }

void OnboardRgbLed::setBrightness(uint8_t brightness) {
  pixel_.setBrightness(brightness);
}

}  // namespace hal
