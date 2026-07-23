#include "flash_storage.h"

#include <cstring>

#include <Arduino.h>
#include <hardware/flash.h>

namespace hal {

FlashStorage::FlashStorage(uint32_t baseOffset, uint32_t sectorSize, uint8_t sectorCount)
    : baseOffset_(baseOffset), sectorSize_(sectorSize), sectorCount_(sectorCount) {}

void FlashStorage::begin() {
  // Flash is memory-mapped and readable without any setup; nothing to do.
  // Kept for symmetry with the rest of the board::* HAL (see board.cpp).
}

void FlashStorage::read(uint8_t sectorIndex, size_t offset, uint8_t* dest, size_t len) const {
  if (sectorIndex >= sectorCount_ || offset + len > sectorSize_) {
    return;
  }
  const uint8_t* src =
      reinterpret_cast<const uint8_t*>(XIP_BASE + baseOffset_ + (static_cast<uint32_t>(sectorIndex) * sectorSize_) +
                                        offset);
  memcpy(dest, src, len);
}

bool FlashStorage::writeSector(uint8_t sectorIndex, const uint8_t* data, size_t len) {
  if (sectorIndex >= sectorCount_ || len > sectorSize_ || sectorSize_ != FLASH_SECTOR_SIZE) {
    return false;
  }

  // flash_range_program requires a page-aligned (256 B) length; pad the
  // sector-sized scratch buffer with 0xFF (flash's erased state) beyond the
  // real payload. Static (not stack-allocated): this is the only writer
  // (the CANopen persistence save path, see src/app/canopen/CO_storageFlash),
  // never called concurrently, so reuse is safe and avoids a ~4 KB stack frame.
  static uint8_t scratch[FLASH_SECTOR_SIZE];
  memset(scratch, 0xFF, sizeof(scratch));
  memcpy(scratch, data, len);

  uint32_t offset = baseOffset_ + (static_cast<uint32_t>(sectorIndex) * sectorSize_);

  // Same write recipe as the framework's own EEPROM library (EEPROM.cpp):
  // rp2040.idleOtherCore() parks the other core and, under FreeRTOS
  // (__freertos_idle_other_core, cores/rp2040/freertos/freertos-main.cpp),
  // also disables interrupts and suspends the scheduler on this core -- the
  // framework's dedicated "flash is being written" mechanism. NOT pico-sdk's
  // flash_safe_execute(): that needs compile-time FreeRTOS support inside
  // pico_flash (or flash_safe_execute_core_init() on the other core) that
  // the arduino-pico build does not provide, so it refuses with
  // PICO_ERROR_NOT_PERMITTED without ever attempting the write.
  rp2040.idleOtherCore();
  flash_range_erase(offset, FLASH_SECTOR_SIZE);
  flash_range_program(offset, scratch, sizeof(scratch));
  // The QSPI program path bypasses the XIP cache, which may still hold stale
  // lines for this region -- flush before anyone (read(), the verify below)
  // reads it back through XIP. Must happen while the other core is idle.
  flash_flush_cache();
  rp2040.resumeOtherCore();

  // flash_range_erase/program return void -- verify by read-back instead.
  const uint8_t* written = reinterpret_cast<const uint8_t*>(XIP_BASE + offset);
  return memcmp(written, scratch, sizeof(scratch)) == 0;
}

}  // namespace hal
