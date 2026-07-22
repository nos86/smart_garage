#include "can_transceiver.h"

namespace hal {

namespace {

// CNF1-3 values for each bitrate, straight from the same MCP_8MHz_*_CFG
// #defines mcp2515.h's own setBitrate(speed, MCP_8MHZ) uses -- kept here too
// so computeBaudrateBps() can re-derive the actual bit rate from the timing
// registers without the private register-readback API the vendored library
// doesn't expose.
struct BitTiming {
  uint8_t cnf1;
  uint8_t cnf2;
  uint8_t cnf3;
};

// Board's MCP25625 is clocked by an 8 MHz crystal (hardware/README.md), so
// every bitrate is resolved against MCP_8MHZ. Only the bitrates plausible
// for this project's CANopen bus are mapped; anything else fails begin()
// rather than silently picking the wrong bit-timing registers.
bool resolveTiming(long bitrateKbps, CAN_SPEED &speed, BitTiming &timing) {
  switch (bitrateKbps) {
    case 1000:
      speed = CAN_1000KBPS;
      timing = {MCP_8MHz_1000kBPS_CFG1, MCP_8MHz_1000kBPS_CFG2, MCP_8MHz_1000kBPS_CFG3};
      return true;
    case 500:
      speed = CAN_500KBPS;
      timing = {MCP_8MHz_500kBPS_CFG1, MCP_8MHz_500kBPS_CFG2, MCP_8MHz_500kBPS_CFG3};
      return true;
    case 250:
      speed = CAN_250KBPS;
      timing = {MCP_8MHz_250kBPS_CFG1, MCP_8MHz_250kBPS_CFG2, MCP_8MHz_250kBPS_CFG3};
      return true;
    case 125:
      speed = CAN_125KBPS;
      timing = {MCP_8MHz_125kBPS_CFG1, MCP_8MHz_125kBPS_CFG2, MCP_8MHz_125kBPS_CFG3};
      return true;
    case 100:
      speed = CAN_100KBPS;
      timing = {MCP_8MHz_100kBPS_CFG1, MCP_8MHz_100kBPS_CFG2, MCP_8MHz_100kBPS_CFG3};
      return true;
    default:
      return false;
  }
}

// MCP2515 bit time = 1 (sync seg) + PRSEG + PS1 + PS2, each a TQ; TQ = 2 *
// (BRP+1) / Fosc. Fosc is fixed at 8 MHz for this board (see resolveTiming),
// so baudrate = Fosc / (2 * (BRP+1) * bit-time-in-TQ). Field layouts are the
// MCP2515 datasheet's CNF1/CNF2/CNF3 registers.
uint32_t computeBaudrateBps(const BitTiming &timing) {
  constexpr uint32_t kFoscHz = 8000000UL;
  uint32_t brp = (timing.cnf1 & 0x3Fu) + 1u;
  uint32_t prseg = (timing.cnf2 & 0x07u) + 1u;
  uint32_t ps1 = ((timing.cnf2 >> 3) & 0x07u) + 1u;
  uint32_t ps2 = (timing.cnf3 & 0x07u) + 1u;
  uint32_t tqPerBit = 1u + prseg + ps1 + ps2;
  return kFoscHz / (2u * brp * tqPerBit);
}

void toDriverFrame(const CanFrame &in, can_frame &out) {
  out.can_id = in.id & (in.extended ? CAN_EFF_MASK : CAN_SFF_MASK);
  if (in.extended) {
    out.can_id |= CAN_EFF_FLAG;
  }
  if (in.remote) {
    out.can_id |= CAN_RTR_FLAG;
  }
  out.can_dlc = in.dlc > 8 ? 8 : in.dlc;
  for (uint8_t i = 0; i < out.can_dlc; ++i) {
    out.data[i] = in.data[i];
  }
}

void fromDriverFrame(const can_frame &in, CanFrame &out) {
  out.extended = (in.can_id & CAN_EFF_FLAG) != 0;
  out.remote = (in.can_id & CAN_RTR_FLAG) != 0;
  out.id = in.can_id & (out.extended ? CAN_EFF_MASK : CAN_SFF_MASK);
  out.dlc = in.can_dlc > 8 ? 8 : in.can_dlc;
  for (uint8_t i = 0; i < out.dlc; ++i) {
    out.data[i] = in.data[i];
  }
}

}  // namespace

CanTransceiver::CanTransceiver(uint8_t csPin, uint8_t intPin) : csPin_(csPin), intPin_(intPin) {}

bool CanTransceiver::begin(CanMode mode, long bitrateKbps) {
  // Constructed here, not in CanTransceiver's constructor -- see the mcp_
  // member comment in can_transceiver.h. begin() is only called from
  // app::board::init() (from setup()), so the global `SPI` object is
  // guaranteed to be fully constructed by now.
  mcp_.emplace(csPin_);

  if (mcp_->reset() != MCP2515::ERROR_OK) {
    return false;
  }

  CAN_SPEED speed;
  BitTiming timing;
  if (!resolveTiming(bitrateKbps, speed, timing)) {
    return false;
  }
  if (mcp_->setBitrate(speed, MCP_8MHZ) != MCP2515::ERROR_OK) {
    return false;
  }
  bitrateKbps_ = bitrateKbps;
  computedBaudrateBps_ = computeBaudrateBps(timing);
  rxFrameCount_ = 0;
  txFrameCount_ = 0;

  MCP2515::ERROR modeResult;
  switch (mode) {
    case CanMode::kLoopback:
      modeResult = mcp_->setLoopbackMode();
      break;
    case CanMode::kListenOnly:
      modeResult = mcp_->setListenOnlyMode();
      break;
    case CanMode::kNormal:
    default:
      modeResult = mcp_->setNormalMode();
      break;
  }
  return modeResult == MCP2515::ERROR_OK;
}

bool CanTransceiver::setFilterMask(uint8_t maskIndex, bool extended, uint32_t mask) {
  MCP2515::MASK num = maskIndex == 0 ? MCP2515::MASK0 : MCP2515::MASK1;
  return mcp_->setFilterMask(num, extended, mask) == MCP2515::ERROR_OK;
}

bool CanTransceiver::setFilter(uint8_t filterIndex, bool extended, uint32_t filter) {
  if (filterIndex > 5) {
    return false;
  }
  auto num = static_cast<MCP2515::RXF>(filterIndex);
  return mcp_->setFilter(num, extended, filter) == MCP2515::ERROR_OK;
}

bool CanTransceiver::send(const CanFrame &frame) {
  can_frame driverFrame{};
  toDriverFrame(frame, driverFrame);
  if (mcp_->sendMessage(&driverFrame) != MCP2515::ERROR_OK) {
    return false;
  }
  ++txFrameCount_;
  return true;
}

bool CanTransceiver::receive(CanFrame &frame) {
  can_frame driverFrame{};
  if (mcp_->readMessage(&driverFrame) != MCP2515::ERROR_OK) {
    return false;
  }
  fromDriverFrame(driverFrame, frame);
  ++rxFrameCount_;
  return true;
}

bool CanTransceiver::hasPendingMessage() { return mcp_->checkReceive(); }

uint8_t CanTransceiver::errorFlags() { return mcp_->getErrorFlags(); }

uint8_t CanTransceiver::txErrorCount() { return mcp_->errorCountTX(); }

uint8_t CanTransceiver::rxErrorCount() { return mcp_->errorCountRX(); }

void CanTransceiver::clearErrors() {
  mcp_->clearRXnOVR();
  mcp_->clearMERR();
  mcp_->clearERRIF();
}

}  // namespace hal
