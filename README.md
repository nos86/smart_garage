# smart_garage

`smart_garage` is a 24 V garage-automation controller node built around an RP2040. It combines local sensing, isolated field inputs, relay switching, and CAN networking so garage lighting and related logic can be controlled both locally and by distributed CAN commands.

## Project goals

- Control two independent 24 V relay channels for garage loads
- Read local sensors (PIR + ultrasonic) for motion/presence-aware behavior
- Accept two galvanically isolated external inputs from field wiring
- Exchange commands and state over CAN bus with other nodes/controllers
- Run deterministic embedded control logic on RP2040 firmware

## System-level architecture

1. **Power domain**
   - Board input: +24 V DC
   - Buck stage generates ~5 V for module-level supply
   - RP2040 module LDO generates 3.3 V logic domain

2. **Control and processing**
   - Waveshare RP2040-Tiny is the central controller
   - Firmware coordinates GPIO, SPI CAN interface, sensor timing, and relay logic

3. **Connectivity and I/O**
   - CAN interface via MCP25625 (SPI + transceiver)
   - Two relay outputs for switched 24 V loads
   - Two isolated digital inputs (PC817C)
   - PIR and ultrasonic sensor interfaces
   - Two status LEDs and a 5-bit hardware configuration DIP input

## Project structure

- `hardware/` — schematics, PCB data, Gerbers, and electrical integration details
- `firmware/` — PlatformIO firmware project skeleton

## Documentation map

- Hardware technical details (electrical integration, RP2040 signal mapping):
  - `hardware/README.md`
- Firmware project structure and implementation area:
  - `firmware/README.md`
