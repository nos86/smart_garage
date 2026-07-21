# Firmware

PlatformIO project for the smart_garage RP2040 node.

```
pio run -e rp2040       # build for the target board
pio run -e rp2040 -t upload
pio test -e native       # host-side unit tests, no board needed (requires a host gcc on PATH)
```

The `rp2040` environment uses the earlephilhower Arduino-Pico core (via the maxgerhardt platform fork) with the `waveshare_rp2040_zero` board definition — electrically equivalent to the RP2040-Tiny actually mounted (same RP2040, 2MB flash, WS2812 on GP16). The upstream `raspberrypi` platform was rejected because its mbed core bundles a pico-sdk too old for current Adafruit NeoPixel releases.

The firmware runs on the FreeRTOS SMP port bundled with the arduino-pico core, enabled by the `PIO_FRAMEWORK_ARDUINO_ENABLE_FREERTOS` build flag (`setup()`/`loop()` then run as a FreeRTOS task on core 0; the scheduler is started by the core itself).

## Layout

- `include/hal/pins.h` — central pin map (single source of truth, mirrors `hardware/README.md`'s GPIO table).
- `include/hal/logic/` — hardware-independent HAL logic (debounce, ultrasonic distance conversion, DIP decoding). No Arduino dependency, unit-tested natively.
- `lib/hal_*/` — one PlatformIO library per peripheral, each a thin Arduino-framework wrapper around the logic above:
  - `hal_digital_input` — debounced digital input with active-high/low inversion (used for INPUT1/INPUT2, and internally by PIR/DIP switch).
  - `hal_relay` — relay channel driver.
  - `hal_pir` — PIR motion sensor (debounced state + rising-edge detection).
  - `hal_ultrasonic` — HC-SR04-compatible sensor with power gating.
  - `hal_led` — carrier-board status LEDs (`Led`) and the RP2040-Tiny module's onboard WS2812 RGB LED (`OnboardRgbLed`).
  - `hal_dip_switch` — 5-position hardware configuration DIP switch (microswitches), decoded to a single 0-31 value.
  - `hal_can` — MCP25625 CAN controller/transceiver, wrapping the `autowp-mcp2515` Arduino library (register-compatible driver). Kept permanently in Normal mode once `begin()` succeeds — never put in standby, per `docs/architecture-plan.md`. No RST/STBY GPIO: reset goes through the chip's SPI soft-reset instruction.
- `src/main.cpp` — FreeRTOS entry point: initializes the board and starts the app tasks.
- `src/app/` — RTOS application layer on top of the HAL:
  - `board.h/.cpp` — single aggregation point for all HAL instances. Also runs a CAN loopback self-test once at `init()` time (exercises the SPI link/TX/RX path without needing a working bus or a second node) before switching the transceiver to Normal mode; the result is exposed via `board::canOk()`.
  - `events.h` — POD event types flowing through the FreeRTOS queue.
  - `tasks.*` — task topology: `sensorsTask` (5 ms polling of inputs/PIR/DIP) and `ultrasonicTask` produce events into `eventQueue`; `controlTask` consumes them and owns all actuators and UART output (so no serial mutex is needed). `ultrasonicTask` implements the two-rate scan policy from `docs/architecture-plan.md` (1/min baseline, 1/s for a sliding 60 s window after a stimulus, sensor power-gated in baseline mode) and is woken early by a task notification on PIR/input stimulus.
- `test/test_hal_logic/` — Unity tests for `include/hal/logic/`, run on the host via the `native` environment.

## Scope of this HAL phase

Covers: isolated inputs, relays, PIR, ultrasonic, LEDs (carrier-board + onboard), DIP/microswitch bank, the MCP25625 CAN transceiver (HAL + interrupt-driven RX), and a minimal CANopenNode bring-up on top of it — the node joins the bus, self-starts to NMT Operational without a master present, runs a heartbeat producer, and answers basic SDO reads against CANopenNode's shipped default Object Dictionary (e.g. 1000h device type, 1018h identity). Node-ID is read directly off the 5-bit hardware DIP switch (`dipValue + 1`).

Not covered yet — see `docs/architecture-plan.md`'s P0/P1 backlog, deliberately left alone by this phase: a custom Object Dictionary design (beyond the one heartbeat-time field), the real node-ID/COB-ID addressing scheme for the DIP switch, a COB-ID priority/bulk-traffic scheme, full NMT/heartbeat/emergency behavior (consumer timeout handling, emergency object usage, master-orchestrated NMT), the `MCP_STBY` pin policy (no STBY GPIO is wired at all — see `include/hal/pins.h`), a CAN-side diagnostic message format, and the end-to-end OTA flow.
