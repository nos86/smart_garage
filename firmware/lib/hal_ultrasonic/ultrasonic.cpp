#include "ultrasonic.h"

#include <FreeRTOS.h>
#include <task.h>

namespace
{

  void cooperativeDelayMs(uint32_t ms)
  {
    if (ms == 0)
    {
      return;
    }

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
      vTaskDelay(pdMS_TO_TICKS(ms));
      return;
    }

    delay(ms);
  }

  uint32_t measurePulseHighCooperative(uint8_t pin, uint32_t timeoutUs)
  {
    uint32_t start = micros();

    // Wait for the line to go idle in case a stale HIGH is present.
    while (digitalRead(pin) == HIGH)
    {
      if (static_cast<uint32_t>(micros() - start) >= timeoutUs)
      {
        return 0;
      }
      yield();
    }

    // Wait for the rising edge.
    while (digitalRead(pin) == LOW)
    {
      if (static_cast<uint32_t>(micros() - start) >= timeoutUs)
      {
        return 0;
      }
      yield();
    }

    uint32_t rise = micros();
    while (digitalRead(pin) == HIGH)
    {
      if (static_cast<uint32_t>(micros() - rise) >= timeoutUs)
      {
        return 0;
      }
      yield();
    }

    return static_cast<uint32_t>(micros() - rise);
  }

} // namespace

namespace hal
{

  UltrasonicSensor::UltrasonicSensor(uint8_t powerPin, uint8_t trigPin, uint8_t echoPin)
      : powerPin_(powerPin), trigPin_(trigPin), echoPin_(echoPin) {}

  void UltrasonicSensor::begin()
  {
    pinMode(powerPin_, OUTPUT);
    pinMode(trigPin_, OUTPUT);
    pinMode(echoPin_, INPUT);
    digitalWrite(trigPin_, LOW);
    powerOff();
  }

  void UltrasonicSensor::powerOn()
  {
    if (powered_)
    {
      return;
    }
    digitalWrite(powerPin_, HIGH);
    powered_ = true;
    poweredSinceMs_ = millis();
    lastActivityMs_ = poweredSinceMs_;
  }

  void UltrasonicSensor::powerOff()
  {
    if (!powered_)
    {
      return;
    }
    digitalWrite(powerPin_, LOW);
    powered_ = false;
  }

  bool UltrasonicSensor::isPowered() const { return powered_; }

  void UltrasonicSensor::setAlwaysOn(bool enabled)
  {
    alwaysOn_ = enabled;
    if (alwaysOn_)
    {
      powerOn();
    }
  }

  void UltrasonicSensor::setAutoPowerCutEnabled(bool enabled) { autoPowerCutEnabled_ = enabled; }

  void UltrasonicSensor::servicePower()
  {
    if (alwaysOn_)
    {
      powerOn();
      return;
    }

    if (!autoPowerCutEnabled_ || !powered_)
    {
      return;
    }

    if (millis() - lastActivityMs_ >= kAutoPowerCutMs)
    {
      powerOff();
    }
  }

  float UltrasonicSensor::measureDistanceCm()
  {
    powerOn();

    uint32_t warmupMs = millis() - poweredSinceMs_;
    if (warmupMs < kPowerSettleMs)
    {
      cooperativeDelayMs(kPowerSettleMs - warmupMs);
    }

    digitalWrite(trigPin_, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin_, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin_, LOW);

    uint32_t durationUs = measurePulseHighCooperative(echoPin_, logic::kEchoTimeoutUs);
    lastActivityMs_ = millis();
    if (durationUs == 0)
    {
      return -1.0f; // no echo within timeout
    }
    return logic::ultrasonic_duration_us_to_cm(durationUs);
  }

} // namespace hal
