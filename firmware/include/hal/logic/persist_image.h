#pragma once

// Pure framing/validation logic for the flash A/B persistence scheme
// (docs/canopen-network-spec.md §8): header layout, CRC32, and slot
// selection. Free of any flash/hardware dependency so it can be
// unit-tested on the host (see test/test_hal_logic).

#include <cstddef>
#include <cstdint>

namespace hal::logic {

constexpr uint32_t kPersistMagic = 0x53474630u;  // "SGF0" -- Smart Garage, Flash layout 0
constexpr uint16_t kPersistLayoutVersion = 1;

// On-flash header, immediately followed by `payloadLen` bytes of payload.
// Field sizes/order are chosen so the compiler inserts no padding (verified
// by the static_assert below) -- the in-memory layout is the on-flash one.
struct PersistHeader {
  uint32_t magic = 0;
  uint16_t layoutVersion = 0;
  uint16_t payloadLen = 0;
  uint32_t sequence = 0;
  uint32_t crc32 = 0;  // over [layoutVersion, payloadLen, sequence, payload]
};
static_assert(sizeof(PersistHeader) == 16, "PersistHeader must be padding-free for a stable on-flash layout");

inline uint32_t persist_crc32_update(const uint8_t* data, size_t len, uint32_t crc) {
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      uint32_t mask = static_cast<uint32_t>(-static_cast<int32_t>(crc & 1u));
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return crc;
}

inline uint32_t persist_compute_crc(uint16_t layoutVersion, uint16_t payloadLen, uint32_t sequence,
                                     const uint8_t* payload) {
  uint32_t crc = 0xFFFFFFFFu;
  crc = persist_crc32_update(reinterpret_cast<const uint8_t*>(&layoutVersion), sizeof(layoutVersion), crc);
  crc = persist_crc32_update(reinterpret_cast<const uint8_t*>(&payloadLen), sizeof(payloadLen), crc);
  crc = persist_crc32_update(reinterpret_cast<const uint8_t*>(&sequence), sizeof(sequence), crc);
  crc = persist_crc32_update(payload, payloadLen, crc);
  return crc ^ 0xFFFFFFFFu;
}

inline PersistHeader persist_build_header(uint16_t payloadLen, uint32_t sequence, const uint8_t* payload) {
  PersistHeader header;
  header.magic = kPersistMagic;
  header.layoutVersion = kPersistLayoutVersion;
  header.payloadLen = payloadLen;
  header.sequence = sequence;
  header.crc32 = persist_compute_crc(header.layoutVersion, header.payloadLen, header.sequence, payload);
  return header;
}

// `payloadCapacity` is the caller's buffer size backing `payload` -- guards
// against a corrupt/foreign payloadLen driving an out-of-bounds CRC read.
inline bool persist_header_valid(const PersistHeader& header, const uint8_t* payload, size_t payloadCapacity) {
  if (header.magic != kPersistMagic) {
    return false;
  }
  if (header.layoutVersion != kPersistLayoutVersion) {
    return false;
  }
  if (header.payloadLen == 0 || header.payloadLen > payloadCapacity) {
    return false;
  }
  return persist_compute_crc(header.layoutVersion, header.payloadLen, header.sequence, payload) == header.crc32;
}

// Sequence-wraparound-safe "is a newer than b" (RFC1982-style serial number
// comparison) -- correct even after the uint32_t sequence counter wraps.
inline bool persist_sequence_newer(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) > 0; }

struct PersistSlotDecision {
  int loadSlot = -1;      // -1: neither copy is valid (first boot / corrupt flash)
  uint8_t writeSlot = 0;  // always the *other* (older/invalid) slot -- spec §8: "si scrive sempre sulla copia più vecchia"
  uint32_t nextSequence = 1;
};

inline PersistSlotDecision persist_select_slots(bool slot0Valid, uint32_t slot0Sequence, bool slot1Valid,
                                                 uint32_t slot1Sequence) {
  PersistSlotDecision decision;
  if (slot0Valid && slot1Valid) {
    bool zeroIsNewer = persist_sequence_newer(slot0Sequence, slot1Sequence);
    decision.loadSlot = zeroIsNewer ? 0 : 1;
    decision.writeSlot = zeroIsNewer ? 1 : 0;
    decision.nextSequence = (zeroIsNewer ? slot0Sequence : slot1Sequence) + 1;
  } else if (slot0Valid) {
    decision.loadSlot = 0;
    decision.writeSlot = 1;
    decision.nextSequence = slot0Sequence + 1;
  } else if (slot1Valid) {
    decision.loadSlot = 1;
    decision.writeSlot = 0;
    decision.nextSequence = slot1Sequence + 1;
  }
  return decision;
}

}  // namespace hal::logic
