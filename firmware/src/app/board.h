#pragma once

// Single aggregation point for all HAL instances, so tasks share one set of
// peripheral objects. Construction is static; call board::init() once before
// starting the scheduler-driven tasks.

#include "digital_input.h"
#include "dip_switch.h"
#include "led.h"
#include "onboard_rgb_led.h"
#include "pir.h"
#include "relay.h"
#include "ultrasonic.h"

namespace app::board {

extern hal::DigitalInput input1;
extern hal::DigitalInput input2;
extern hal::Relay relay1;
extern hal::Relay relay2;
extern hal::PirSensor pir;
extern hal::UltrasonicSensor ultrasonic;
extern hal::Led led1;
extern hal::Led led2;
extern hal::OnboardRgbLed onboardLed;
extern hal::DipSwitch dipSwitch;

void init();

}  // namespace app::board
