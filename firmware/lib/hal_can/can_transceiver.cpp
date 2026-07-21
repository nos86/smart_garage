#include "can_transceiver.h"

namespace hal {

namespace {

// Board's MCP25625 is clocked by an 8 MHz crystal (hardware/README.md), so
// every bitrate is resolved against MCP_8MHZ. Only the bitrates plausible
// for this project's CANopen bus are mapped; anything else fails begin()
// rather than silently picking the wrong bit-timing registers.
bool resolveSpeed(long bitrateKbps, CAN_SPEED &speed) {
  switch (bitrateKbps) {
    case 1000:
      speed = CAN_1000KBPS;
      return true;
    case 500:
      speed = CAN_500KBPS;
      return true;
    case 250:
      speed = CAN_250KBPS;
      return true;
    case 125:
      speed = CAN_125KBPS;
      return true;
    case 100:
      speed = CAN_100KBPS;
      return true;
    default:
      return false;
  }
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
  if (!resolveSpeed(bitrateKbps, speed)) {
    return false;
  }
  if (mcp_->setBitrate(speed, MCP_8MHZ) != MCP2515::ERROR_OK) {
    return false;
  }

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
  return mcp_->sendMessage(&driverFrame) == MCP2515::ERROR_OK;
}

bool CanTransceiver::receive(CanFrame &frame) {
  can_frame driverFrame{};
  if (mcp_->readMessage(&driverFrame) != MCP2515::ERROR_OK) {
    return false;
  }
  fromDriverFrame(driverFrame, frame);
  return true;
}

bool CanTransceiver::hasPendingMessage() { return mcp_->checkReceive(); }

uint8_t CanTransceiver::errorFlags() { return mcp_->getErrorFlags(); }

void CanTransceiver::clearErrors() {
  mcp_->clearRXnOVR();
  mcp_->clearMERR();
  mcp_->clearERRIF();
}

}  // namespace hal
