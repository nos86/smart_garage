#pragma once

// Event types flowing from the sensor tasks to the control task. Plain POD,
// passed by copy through a FreeRTOS queue.

#include <cstdint>

namespace app
{

  enum class EventType : uint8_t
  {
    kInputChanged,      // isolated input edge; index = 0/1, state = new level
    kPirMotionStarted,  // PIR rising edge
    kPirChanged,        // PIR logical state changed; state = current level
    kDipChanged,        // DIP value changed; index = new 0-31 value
    kUltrasonicReading, // distanceCm holds the measurement
    kUltrasonicTimeout, // no echo within timeout
    kCanFrameReceived,  // a frame was drained from the MCP25625 RX buffers
    kCanError,          // index = current hal::CanTransceiver::errorFlags()
    kCanopenNodeId,     // index = CANopen node-ID in use, sent once after init
    kNmtStateChanged,   // index = one of cli::kNmt* (see cli/model.h)
    kHeartbeatSent,     // the HB producer just sent a heartbeat
  };

  struct Event
  {
    EventType type;
    uint8_t index = 0;
    bool state = false;
    float distanceCm = 0.0f;
  };

} // namespace app
