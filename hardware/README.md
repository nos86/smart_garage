# Hardware

This directory stores the EasyEDA hardware design files and the electrical integration reference for the project.

- `schematic/` for schematics
- `pcb/` for PCB layouts
- `gerber/` for Gerber exports

## Hardware overview

### Power supply

The board accepts **+24 V DC** supply input. An onboard **TPS560430XDBVR** synchronous buck converter steps the 24 V rail down to **5.3 V** (nominally 5 V), which feeds the RP2040-Tiny module and related 5 V logic. A 3.3 V LDO on the RP2040-Tiny module provides the 3.3 V rail used by logic interfaces and sensors.

A 4-pin connector (CANH / CANL / 24 V / GND) carries CAN bus and main power into the board.

### Microcontroller

The **Waveshare RP2040-Tiny** module (RP2040 dual-core Cortex-M0+) is the central processor. It interfaces to peripherals via GPIO and SPI.

### CAN bus communication

A **MCP25625-EML** (SPI CAN controller + transceiver) provides CAN 2.0B connectivity. It is clocked by an onboard **8 MHz crystal** and connected to RP2040 over SPI.

### Relay output channels

Two **G5NB-1A-E-24VDC** relays switch 24 V circuits. Each coil is driven by a **BSS138** N-channel MOSFET gate controlled by RP2040 GPIO.

### Isolated digital inputs

Two **PC817C** optocouplers provide galvanically isolated digital inputs (INPUT1, INPUT2). The optocoupler output side is pulled up to 3.3 V and read by RP2040.

### Sensor interfaces

| Sensor | Connector | Notes |
|--------|-----------|-------|
| **PIR** motion detector | U11 — 2541FV-3P | 3.3 V supply; digital output with 1 kΩ series resistor, 100 kΩ pull-down, 100 nF filter |
| **Ultrasonic** (HC-SR04 compatible) | CN1 — 2541FV-4P | VCC switched by **TPS22918DBVR** load switch; Trig/Echo interface |

### Hardware configuration input

A **5-position DIP switch** (SW1) provides a 5-bit hardware configuration input to RP2040 GPIOs. Firmware may interpret these bits for node addressing, mode selection, or other configuration policy.

### Status LEDs

Two LEDs (LED1, LED2) are driven from RP2040 GPIOs through current-limiting resistors.

---

## Technical reference — RP2040 pin assignments

> **Note:** Pin assignments are derived from schematic analysis; verify against EasyEDA sources in `schematic/` before final firmware constants.

### GPIO table

| RP2040 GPIO | Net / Signal | Direction | Description |
|-------------|-------------|-----------|-------------|
| GP0 | INPUT1 | Input | Isolated digital input 1 — optocoupler output (active-low, pulled to 3.3 V via 10 kΩ) |
| GP1 | INPUT2 | Input | Isolated digital input 2 — optocoupler output (active-low, pulled to 3.3 V via 10 kΩ) |
| GP2 | SPI0_SCK | Output | SPI clock to MCP25625 |
| GP3 | SPI0_TX | Output | SPI MOSI to MCP25625 (SI) |
| GP4 | SPI0_RX | Input | SPI MISO from MCP25625 (SO) |
| GP5 | SPI0_CSn | Output | SPI chip-select for MCP25625 (active-low) |
| GP6 | MCP_INT | Input | MCP25625 interrupt output (active-low) |
| GP7 | MCP_RST | Output | MCP25625 hardware reset (active-low) |
| GP8 | MCP_STBY | Output | MCP25625 standby control (high = standby) |
| GP9 | DIP1 | Input | DIP switch position 1 (firmware-defined configuration bit 0) |
| GP10 | DIP2 | Input | DIP switch position 2 (firmware-defined configuration bit 1) |
| GP11 | DIP3 | Input | DIP switch position 3 (firmware-defined configuration bit 2) |
| GP12 | DIP4 | Input | DIP switch position 4 (firmware-defined configuration bit 3) |
| GP13 | DIP5 | Input | DIP switch position 5 (firmware-defined configuration bit 4) |
| GP14 | RELAY1 | Output | Relay 1 gate drive — high activates MOSFET and energizes relay coil J1 |
| GP15 | RELAY2 | Output | Relay 2 gate drive — high activates MOSFET and energizes relay coil J2 |
| GP26 | PIR | Input | PIR motion digital output (active-high) |
| GP27 | ULTRA_ON | Output | Ultrasonic sensor VCC enable via TPS22918 load switch |
| GP28 | ULTRA_TRIG | Output | Ultrasonic trigger pulse output |
| GP29 | ULTRA_ECHO | Input | Ultrasonic echo pulse timing input |
| LED1 pin | LED1 | Output | Status LED 1 drive (active-high) |
| LED2 pin | LED2 | Output | Status LED 2 drive (active-high) |

> **SPI bus:** GP2–GP5 form SPI0 to MCP25625. MCP25625 uses external crystal X1 (8 MHz).

### Detailed electrical connectivity to RP2040

| Subsystem | RP2040 signal(s) | Interface conditioning between peripheral and RP2040 | Electrical behavior at RP2040 side |
|-----------|------------------|-------------------------------------------------------|------------------------------------|
| CAN controller/transceiver (U2 MCP25625) | GP2 (SCK), GP3 (MOSI), GP4 (MISO), GP5 (CSn), GP6 (INT), GP7 (RST), GP8 (STBY) | Direct 3.3 V digital SPI wiring, dedicated active-low interrupt line, reset line, and standby control line | SPI and control lines are 3.3 V CMOS; INT is active-low; CSn and RST are active-low outputs from RP2040 |
| Isolated input channel 1 (INPUT1 via U5 PC817C) | GP0 | 24 V field input drives optocoupler LED; transistor output side referenced to 3.3 V with 10 kΩ pull-up | RP2040 reads active-low logic (line pulled high when input is idle, pulled low when optocoupler conducts) |
| Isolated input channel 2 (INPUT2 via U8 PC817C) | GP1 | Same front-end as INPUT1 (optocoupler isolation + 3.3 V pull-up) | RP2040 reads active-low logic |
| DIP hardware configuration input (SW1, 5 positions) | GP9–GP13 | Each switch line uses 1 kΩ pull-up to 3.3 V and switch-to-GND when closed | Open switch reads high; closed switch reads low (inverted logic bits) |
| Relay driver 1 (J1 coil path) | GP14 | RP2040 drives BSS138 gate through 1 kΩ resistor; MOSFET sinks 24 V relay coil current | GPIO high turns MOSFET on and energizes relay; GPIO low releases relay |
| Relay driver 2 (J2 coil path) | GP15 | Same gate-drive topology as relay 1 | GPIO high turns MOSFET on and energizes relay; GPIO low releases relay |
| PIR motion sensor connector (U11) | GP26 | Sensor digital output routed through 1 kΩ series resistor, with 100 kΩ pull-down and 100 nF filtering | Motion event presented as active-high pulse to RP2040 |
| Ultrasonic power control (CN1 VCC path) | GP27 | GP27 controls TPS22918 load switch ON pin, which switches sensor supply rail | GPIO high powers ultrasonic module; GPIO low removes power |
| Ultrasonic trigger (CN1 TRIG) | GP28 | Direct digital output from RP2040 to sensor trigger pin | Firmware emits ≥10 µs high pulse to start measurement |
| Ultrasonic echo (CN1 ECHO) | GP29 | Echo line routed as digital input to RP2040 timing capture path | Pulse width at RP2040 input is proportional to measured distance |
| Status LED1 | LED1 GPIO (module pin) | RP2040 output drives LED through series resistor | Active-high LED drive |
| Status LED2 | LED2 GPIO (module pin) | RP2040 output drives LED through series resistor | Active-high LED drive |
