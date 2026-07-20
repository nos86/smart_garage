#pragma once

// Pure DIP/microswitch bit decoding, free of any hardware dependency.

#include <cstdint>

namespace hal::logic {

// bits[0] is switch 1 (value bit 0, LSB) ... bits[4] is switch 5 (bit 4).
// Each entry is the already-inverted logical "closed" state (true = switch
// ON / closed), not the raw electrical level.
inline uint8_t dip_switch_decode(const bool bits[5]) {
  uint8_t value = 0;
  for (uint8_t i = 0; i < 5; ++i) {
    if (bits[i]) {
      value |= static_cast<uint8_t>(1u << i);
    }
  }
  return value;
}

}  // namespace hal::logic
