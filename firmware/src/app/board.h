#pragma once

// Single aggregation point for all HAL instances, so tasks share one set of
// peripheral objects. Construction is static; call board::init() once before
// starting the scheduler-driven tasks.

#include "can_transceiver.h"
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
extern hal::CanTransceiver can;

// Result of the CAN loopback self-test run once by init(). Diagnostic only
// -- init() does not fail/abort if this is false, it just leaves canOk()
// reporting the problem so the CLI's CAN tab can surface it.
bool canOk();

void init();

}  // namespace app::board
