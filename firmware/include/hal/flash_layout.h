#pragma once

// Reserved flash region for CANopen's persisted-configuration A/B scheme
// (docs/canopen-network-spec.md §8): the last two erase sectors of the
// RP2040's QSPI flash, kept out of the earlephilhower core's program image
// and out of the future OTA dual-bank slots.

#include <cstdint>

#include <hardware/flash.h>
#include <pico/platform.h>

namespace hal::flash_layout {

// Erase granularity of the flash chip; also the size of one A/B copy.
constexpr uint32_t kSectorSize = FLASH_SECTOR_SIZE;  // 4096

// Two complete copies (A/B), per spec §8 -- a power-fail during a write to
// one leaves the other intact.
constexpr uint8_t kSectorCount = 2;

// Last kSectorCount sectors of the chip. On this module's 2 MiB QSPI flash
// this evaluates to 0x1FE000, matching the offset named in the spec.
constexpr uint32_t kBaseOffset = PICO_FLASH_SIZE_BYTES - (kSectorCount * kSectorSize);

}  // namespace hal::flash_layout
