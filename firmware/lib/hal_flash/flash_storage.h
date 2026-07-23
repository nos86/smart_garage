#pragma once

#include <cstddef>
#include <cstdint>

namespace hal {

// Raw sector-level access to a fixed, reserved region of the RP2040's QSPI
// flash (see hal/flash_layout.h), used as the physical backing store for
// CANopen's A/B persisted-configuration scheme
// (docs/canopen-network-spec.md §8). No knowledge of what is stored -- pure
// erase/program/read primitives, kept separate so higher layers (the
// header/CRC framing in hal/logic/persist_image.h, the CO_storage glue in
// src/app/canopen) stay hardware-free and testable.
class FlashStorage {
 public:
  FlashStorage(uint32_t baseOffset, uint32_t sectorSize, uint8_t sectorCount);

  void begin();

  uint32_t sectorSize() const { return sectorSize_; }
  uint8_t sectorCount() const { return sectorCount_; }

  // Reads `len` bytes starting at `offset` within the given sector. Flash is
  // memory-mapped (XIP) for reads, so this is safe to call at any time, from
  // any core.
  void read(uint8_t sectorIndex, size_t offset, uint8_t* dest, size_t len) const;

  // Erases the sector, then programs it with `data` (the remainder of the
  // sector is left erased, i.e. reads back as 0xFF). Blocks for the
  // duration of the erase+program and briefly halts the other core
  // (rp2040.idleOtherCore(), the same mechanism the framework's own EEPROM
  // library uses) so no XIP fetch happens while the flash is unreadable.
  // Returns false if the arguments are out of range or the written data
  // fails read-back verification.
  bool writeSector(uint8_t sectorIndex, const uint8_t* data, size_t len);

 private:
  uint32_t baseOffset_;
  uint32_t sectorSize_;
  uint8_t sectorCount_;
};

}  // namespace hal
