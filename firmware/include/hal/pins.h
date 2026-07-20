#pragma once

// Central pin map for the smart_garage RP2040 node.
// Source: hardware/README.md GPIO table (schematic-derived; verify against
// EasyEDA sources before relying on unconfirmed entries).
//
// This HAL phase covers isolated inputs, relays, PIR, ultrasonic, LEDs and
// the DIP/microswitch bank. The MCP25625 CAN controller pins (GP2-GP8) are
// intentionally not wrapped here yet.

#include <cstdint>

namespace hal::pins
{

    // Isolated digital inputs (PC817C optocouplers, active-low: pulled high via
    // external 10k, pulled low when the optocoupler conducts).
    constexpr uint8_t kInput1 = 0;
    constexpr uint8_t kInput2 = 1;

    // GP2-GP6 are reserved for the MCP25625 CAN controller (SPI + INT) --
    // out of scope for this HAL phase. GP7/GP8 are used by status LEDs.

    // Hardware configuration DIP switch / microswitches (SW1, active-low: each
    // line pulled high externally via 1k, pulled low when the switch is closed).
    constexpr uint8_t kDip1 = 9;
    constexpr uint8_t kDip2 = 10;
    constexpr uint8_t kDip3 = 11;
    constexpr uint8_t kDip4 = 12;
    constexpr uint8_t kDip5 = 13;
    constexpr uint8_t kDipPins[5] = {kDip1, kDip2, kDip3, kDip4, kDip5};

    // Relay outputs (drive BSS138 gate, active-high energizes the coil).
    constexpr uint8_t kRelay1 = 14;
    constexpr uint8_t kRelay2 = 15;

    // PIR motion sensor (active-high pulse, hardware-filtered).
    constexpr uint8_t kPir = 29;

    // Ultrasonic sensor (HC-SR04-compatible, power gated by a TPS22918 load switch).
    constexpr uint8_t kUltrasonicPower = 28;
    constexpr uint8_t kUltrasonicTrig = 26;
    constexpr uint8_t kUltrasonicEcho = 27;

    // Onboard addressable RGB LED (WS2812) on the Waveshare RP2040-Tiny module
    // itself. Not present in hardware/README.md (that doc covers the carrier
    // board only) -- GP16 is sourced from Waveshare's public RP2040-Zero/Tiny
    // reference design and should be double-checked against the module datasheet.
    constexpr uint8_t kOnboardRgbLed = 16;

    // Status LEDs LED1/LED2 (active-high).
    constexpr uint8_t kLed1 = 8;
    constexpr uint8_t kLed2 = 7;

} // namespace hal::pins
