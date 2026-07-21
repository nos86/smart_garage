#pragma once

#include <Arduino.h>
#include <mcp2515.h>

#include <cstdint>
#include <optional>

namespace hal {

// A CAN 2.0B data or remote frame, decoupled from the mcp2515 library's
// SocketCAN-style struct can_frame so callers outside this HAL do not need
// to know about CAN_EFF_FLAG/CAN_RTR_FLAG bit-packing.
struct CanFrame {
  uint32_t id = 0;
  bool extended = false;
  bool remote = false;
  uint8_t dlc = 0;
  uint8_t data[8] = {};
};

enum class CanMode : uint8_t { kNormal, kLoopback, kListenOnly };

// MCP25625 CAN controller/transceiver (SPI, register-compatible with
// MCP2515, driven via the autowp-mcp2515 library). The chip is kept in
// Normal mode permanently once begin() succeeds -- per
// docs/architecture-plan.md it is never put in standby, so there is no
// sleep()/wake() surface here. Reset is done through the chip's SPI
// soft-reset instruction; there is no RST/STBY GPIO (see hal::pins).
//
// The interrupt pin is exposed as data only -- attaching the ISR and
// draining received frames is an RTOS concern that lives in the app layer
// (src/app/task_can.cpp), not in this Arduino-only HAL class.
class CanTransceiver {
 public:
  CanTransceiver(uint8_t csPin, uint8_t intPin);

  // Resets the chip, programs the bitrate for the board's 8 MHz crystal,
  // and switches to the requested operating mode. Returns false if any
  // MCP2515 register-programming step fails (e.g. SPI/chip not responding).
  bool begin(CanMode mode = CanMode::kNormal, long bitrateKbps = 250);

  bool setFilterMask(uint8_t maskIndex, bool extended, uint32_t mask);
  bool setFilter(uint8_t filterIndex, bool extended, uint32_t filter);

  bool send(const CanFrame &frame);
  bool receive(CanFrame &frame);  // non-blocking; false if RX empty
  bool hasPendingMessage();       // not const: the underlying driver call is not either

  uint8_t errorFlags();
  void clearErrors();

  uint8_t interruptPin() const { return intPin_; }

 private:
  uint8_t csPin_;
  uint8_t intPin_;
  // Constructed lazily in begin(), not here: MCP2515's constructor calls
  // SPI.begin() directly, and this object is instantiated as a namespace-
  // scope global (app::board::can) -- running that before setup() risks
  // executing before the global `SPI` object itself is constructed
  // (unspecified C++ static-init order across translation units), which
  // hard-faults the MCU before Serial ever comes up. begin() is only ever
  // called from app::board::init(), safely after all static init completes.
  std::optional<MCP2515> mcp_;
};

}  // namespace hal
