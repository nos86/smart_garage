#pragma once

// Pure scheduling policy for the ultrasonic scan rate, per the energy plan in
// docs/architecture-plan.md: 1 measurement/minute at rest, 1/second for a
// sliding 60 s window after the last stimulus (button, PIR, master event).
// Hardware-free so it can be unit-tested on the host.

#include <cstdint>

namespace hal::logic {

class UltrasonicScheduler {
 public:
  static constexpr uint32_t kActiveWindowMs = 60000;
  static constexpr uint32_t kActivePeriodMs = 1000;
  static constexpr uint32_t kBaselinePeriodMs = 60000;

  void onStimulus(uint32_t nowMs) {
    hasStimulus_ = true;
    lastStimulusMs_ = nowMs;
  }

  bool isActive(uint32_t nowMs) const {
    return hasStimulus_ && (nowMs - lastStimulusMs_ < kActiveWindowMs);
  }

  uint32_t periodMs(uint32_t nowMs) const {
    return isActive(nowMs) ? kActivePeriodMs : kBaselinePeriodMs;
  }

 private:
  bool hasStimulus_ = false;
  uint32_t lastStimulusMs_ = 0;
};

}  // namespace hal::logic
