#pragma once

// Declares the C entry point (defined in CO_driver.c) that task_can.cpp's
// RX drain loop calls to hand a received frame to CANopenNode's software
// ident/mask dispatch. Kept separate from CO_driver_target.h so C++
// callers do not need to pull in CANopenNode's own headers.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

void canopenDispatchRxFrame(uint32_t ident, uint8_t dlc, const uint8_t data[8]);

#ifdef __cplusplus
}
#endif
