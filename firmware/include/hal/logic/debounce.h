#pragma once

// Pure debounce state machine, free of any hardware/Arduino dependency so it
// can be unit-tested on the host (see test/test_hal_logic).

#include <cstdint>

namespace hal::logic {

struct DebounceState {
  bool stable = false;
  bool lastRaw = false;
  uint32_t lastChangeMs = 0;
};

// Feeds one new raw sample into the debounce state machine and returns the
// current stable (debounced) value. `nowMs` and `lastChangeMs` share the same
// time base (e.g. millis()).
inline bool debounce_update(DebounceState& state, bool rawValue, uint32_t nowMs, uint32_t debounceMs) {
  if (rawValue != state.lastRaw) {
    state.lastRaw = rawValue;
    state.lastChangeMs = nowMs;
  }
  if (nowMs - state.lastChangeMs >= debounceMs) {
    state.stable = state.lastRaw;
  }
  return state.stable;
}

}  // namespace hal::logic
