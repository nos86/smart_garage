#pragma once

#include <Arduino.h>

#include "hal/logic/ultrasonic_math.h"

namespace hal
{

  // HC-SR04-compatible ultrasonic sensor whose VCC is gated by a load switch
  // (ULTRA_ON), so it can be fully powered down between measurement bursts.
  class UltrasonicSensor
  {
  public:
    UltrasonicSensor(uint8_t powerPin, uint8_t trigPin, uint8_t echoPin);

    void begin();
    void powerOn();
    void powerOff();
    bool isPowered() const;

    // Keeps the sensor powered continuously (used by forced ON mode).
    void setAlwaysOn(bool enabled);

    // Enables/disables automatic power cut in AUTO mode.
    void setAutoPowerCutEnabled(bool enabled);

    // Applies the configured power policy (always-on or auto cut timeout).
    void servicePower();

    // Triggers one measurement and waits cooperatively until echo or timeout.
    // Returns a negative value if no echo was seen.
    float measureDistanceCm();

  private:
    static constexpr uint32_t kPowerSettleMs = 250;
    static constexpr uint32_t kAutoPowerCutMs = 1000;

    uint8_t powerPin_;
    uint8_t trigPin_;
    uint8_t echoPin_;
    bool powered_ = false;
    bool alwaysOn_ = false;
    bool autoPowerCutEnabled_ = true;
    uint32_t poweredSinceMs_ = 0;
    uint32_t lastActivityMs_ = 0;
  };

} // namespace hal
