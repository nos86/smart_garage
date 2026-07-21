#include "board.h"

#include <Arduino.h>

#include "hal/pins.h"

namespace app::board {

namespace {

bool canOk_ = false;

// Exercises the SPI link and the TX/RX frame path without needing a working
// bus or a second node: put the chip in loopback, send a known frame, and
// check it comes back unchanged. Run once at boot before switching to
// Normal mode for real operation (see docs in firmware/README.md, Stage A
// verification plan).
bool runCanLoopbackSelfTest() {
  if (!can.begin(hal::CanMode::kLoopback)) {
    return false;
  }

  hal::CanFrame sent{};
  sent.id = 0x123;
  sent.dlc = 2;
  sent.data[0] = 0xAA;
  sent.data[1] = 0x55;

  if (!can.send(sent)) {
    return false;
  }

  hal::CanFrame received{};
  uint32_t startMs = millis();
  while (!can.receive(received)) {
    if (millis() - startMs > 50) {
      return false;
    }
  }

  return received.id == sent.id && received.dlc == sent.dlc && received.data[0] == sent.data[0] &&
         received.data[1] == sent.data[1];
}

}  // namespace

hal::DigitalInput input1(hal::pins::kInput1, /*activeLow=*/true);
hal::DigitalInput input2(hal::pins::kInput2, /*activeLow=*/true);
hal::Relay relay1(hal::pins::kRelay1);
hal::Relay relay2(hal::pins::kRelay2);
hal::PirSensor pir(hal::pins::kPir);
hal::UltrasonicSensor ultrasonic(hal::pins::kUltrasonicPower, hal::pins::kUltrasonicTrig,
                                  hal::pins::kUltrasonicEcho);
hal::Led led1(hal::pins::kLed1);
hal::Led led2(hal::pins::kLed2);
hal::OnboardRgbLed onboardLed(hal::pins::kOnboardRgbLed);
hal::DipSwitch dipSwitch(hal::pins::kDipPins);
hal::CanTransceiver can(hal::pins::kCanCs, hal::pins::kCanInt);

bool canOk() { return canOk_; }

void init() {
  input1.begin();
  input2.begin();
  relay1.begin();
  relay2.begin();
  pir.begin();
  ultrasonic.begin();
  led1.begin();
  led2.begin();
  onboardLed.begin();
  dipSwitch.begin();

  canOk_ = runCanLoopbackSelfTest() && can.begin(hal::CanMode::kNormal);
}

}  // namespace app::board
