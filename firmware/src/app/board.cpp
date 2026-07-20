#include "board.h"

#include "hal/pins.h"

namespace app::board {

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
}

}  // namespace app::board
