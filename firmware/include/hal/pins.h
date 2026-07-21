#pragma once

// Central pin map for the smart_garage RP2040 node.
// Source: hardware/README.md GPIO table (schematic-derived; verify against
// EasyEDA sources before relying on unconfirmed entries).
//
// This HAL phase covers isolated inputs, relays, PIR, ultrasonic, LEDs,
// the DIP/microswitch bank, and the MCP25625 CAN controller.

#include <cstdint>

namespace hal::pins
{

    // Isolated digital inputs (PC817C optocouplers, active-low: pulled high via
    // external 10k, pulled low when the optocoupler conducts).
    constexpr uint8_t kInput1 = 0;
    constexpr uint8_t kInput2 = 1;

    // MCP25625 CAN controller (SPI0 + interrupt). Reset is done via the
    // chip's SPI soft-reset instruction (no RST GPIO wired); STBY is never
    // asserted -- the chip stays always-active per architecture-plan.md (no
    // STBY GPIO wired either). GP7/GP8 stay assigned to LED2/LED1 (existing
    // wiring, see below) -- hardware/README.md's GPIO table used to list
    // GP7=MCP_RST/GP8=MCP_STBY, which conflicted with the LEDs already
    // implemented here; resolved by not wiring RST/STBY as GPIOs at all.
    constexpr uint8_t kCanSck = 2;
    constexpr uint8_t kCanMosi = 3;
    constexpr uint8_t kCanMiso = 4;
    constexpr uint8_t kCanCs = 5;
    constexpr uint8_t kCanInt = 6;

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
