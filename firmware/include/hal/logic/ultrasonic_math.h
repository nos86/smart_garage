#pragma once

// Pure trig/echo-to-distance conversion, free of any hardware dependency.

#include <cstdint>

namespace hal::logic {

// Speed of sound at ~20 degC, expressed as cm per microsecond.
constexpr float kSoundSpeedCmPerUs = 0.0343f;

// pulseIn() timeout guard: ~5 m round trip at the speed of sound above.
constexpr uint32_t kEchoTimeoutUs = 30000;

// Converts a trig->echo round-trip duration into a one-way distance in cm.
inline float ultrasonic_duration_us_to_cm(uint32_t echoDurationUs) {
  return static_cast<float>(echoDurationUs) * kSoundSpeedCmPerUs / 2.0f;
}

}  // namespace hal::logic
