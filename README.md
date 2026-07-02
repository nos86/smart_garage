# smart_garage
Smart relay controller for garage lights, built around an RP2040 microcontroller and a CAN bus interface. The board is designed to be installed in a garage environment and allows centralized control of two lighting circuits, with inputs from motion (PIR) and distance (ultrasonic) sensors plus two isolated external inputs, all coordinated over a CAN bus network.

## Project structure

- `hardware/schematic/` — EasyEDA schematic files (two pages: Power Supply, Microcontroller)
- `hardware/pcb/` — EasyEDA PCB design files and interactive BOM
- `hardware/gerber/` — generated Gerber outputs for PCB manufacturing
- `firmware/` — PlatformIO-based firmware project

## How it works

### Power supply

The board accepts a **+24 V DC** supply. An onboard **TPS560430XDBVR** synchronous buck converter steps the 24 V down to **5.3 V** (nominally 5 V), which feeds the RP2040-Tiny module and related 5 V logic. A 3.3 V LDO on the RP2040-Tiny module provides the 3.3 V rail used by the CAN controller and sensors.

A 4-pin connector (CANH / CANL / 24 V / GND) carries the CAN bus and main power onto the board from the installation wiring.

### Microcontroller

The **Waveshare RP2040-Tiny** module (dual-core Arm Cortex-M0+ at up to 133 MHz) is the central processing unit. It communicates with all onboard peripherals via GPIO, SPI, and time-based pulse measurement.

### CAN bus communication

A **MCP25625-EML** (SPI CAN controller + transceiver in a single QFN-28 package) provides CAN 2.0B connectivity. It is clocked by an onboard **8 MHz crystal** and connected to the RP2040 over an SPI bus. The transceiver side connects directly to the CANH/CANL lines of the 4-pin power+CAN connector. The CAN bus allows this node to exchange status and commands with other nodes (e.g. a central controller or other smart garage boards).

### Outputs — relay channels

Two **G5NB-1A-E-24VDC** single-pole relays switch 24 V lighting circuits. Each relay coil is driven by a **BSS138** N-channel MOSFET (gate resistor 1 kΩ, drain clamp diode SMAJ33A) whose gate is controlled by an RP2040 GPIO pin. The relay contacts are brought to **WJ500V-5.08-3P** screw-terminal connectors (P1, P2) for field wiring.

### Inputs — isolated digital inputs

Two **PC817C** optocouplers provide galvanically isolated digital inputs (INPUT1, INPUT2). The optocoupler LED is driven from the external 24 V loop (protected by a SMAJ33A TVS diode); the collector side is pulled up to 3.3 V via 10 kΩ resistors. These inputs can accept dry-contact or open-collector signals from pushbuttons, door sensors, or other 24 V logic sources.

### Sensors

| Sensor | Connector | Notes |
|--------|-----------|-------|
| **PIR** motion detector | U11 — 2541FV-3P (3-pin JST-style) | 3.3 V supply; digital output with 1 kΩ series resistor and 100 kΩ pull-down for defined boot state; 100 nF noise filter |
| **Ultrasonic** distance (HC-SR04 compatible) | CN1 — 2541FV-4P (4-pin JST-style) | VCC switched by a **TPS22918DBVR** load switch (ON/QOD controlled by RP2040) to allow power cycling; standard Trig/Echo interface |

### Configuration — DIP switch

A **5-position DIP switch** (SW1, HV-GSHP05TS) sets the CAN node address or operating mode. Each position is read by a dedicated RP2040 GPIO with a 1 kΩ pull-up to 3.3 V.

### Status LEDs

Two red SMD LEDs (LED1, LED2 — XL-2012SURC) driven directly from RP2040 GPIOs through current-limiting resistors indicate firmware status (e.g. CAN activity, relay state, error).

---

## Technical reference — RP2040 pin assignments

> **Note:** The microcontroller module is the **Waveshare RP2040-Tiny** (RP2040, *not* RP2350). Pin assignments below are derived from schematic analysis; verify against the EasyEDA source in `hardware/schematic/` before finalising firmware constants.

### GPIO table

| RP2040 GPIO | Net / Signal | Direction | Description |
|-------------|-------------|-----------|-------------|
| GP0 | INPUT1 | Input | Isolated digital input 1 — optocoupler U5 output (active-low, pulled to 3.3 V via 10 kΩ) |
| GP1 | INPUT2 | Input | Isolated digital input 2 — optocoupler U8 output (active-low, pulled to 3.3 V via 10 kΩ) |
| GP2 | SPI0_SCK | Output | SPI clock to MCP25625 |
| GP3 | SPI0_TX | Output | SPI MOSI to MCP25625 (SI) |
| GP4 | SPI0_RX | Input | SPI MISO from MCP25625 (SO) |
| GP5 | SPI0_CSn | Output | SPI chip-select for MCP25625 (active-low) |
| GP6 | MCP_INT | Input | MCP25625 interrupt output (active-low) — triggers on received CAN frame |
| GP7 | MCP_RST | Output | MCP25625 hardware reset (active-low) |
| GP8 | MCP_STBY | Output | MCP25625 standby control (high = low-power standby) |
| GP9 | DIP1 | Input | DIP switch position 1 — CAN node address bit 0 |
| GP10 | DIP2 | Input | DIP switch position 2 — CAN node address bit 1 |
| GP11 | DIP3 | Input | DIP switch position 3 — CAN node address bit 2 |
| GP12 | DIP4 | Input | DIP switch position 4 — CAN node address bit 3 |
| GP13 | DIP5 | Input | DIP switch position 5 — CAN node address bit 4 |
| GP14 | RELAY1 | Output | Relay 1 gate drive — high activates BSS138 MOSFET, energising relay coil J1 |
| GP15 | RELAY2 | Output | Relay 2 gate drive — high activates BSS138 MOSFET, energising relay coil J2 |
| GP26 | PIR | Input | PIR motion sensor digital output (active-high on motion detection) |
| GP27 | ULTRA_ON | Output | Ultrasonic sensor VCC enable via TPS22918 load switch (high = powered) |
| GP28 | ULTRA_TRIG | Output | Ultrasonic sensor trigger pulse (≥10 µs high pulse initiates measurement) |
| GP29 | ULTRA_ECHO | Input | Ultrasonic sensor echo pulse (pulse width ∝ measured distance) |
| LED1 pin | LED1 | Output | Status LED 1 — red indicator (active-high through current-limiting resistor) |
| LED2 pin | LED2 | Output | Status LED 2 — red indicator (active-high through current-limiting resistor) |

> **SPI bus:** GP2–GP5 form the SPI0 peripheral interface to the MCP25625. The MCP25625 uses an external 8 MHz crystal (X1) as its CAN bit-rate clock source; the RP2040 does not share this crystal.

### Detailed electrical connectivity to RP2040

| Subsystem | RP2040 signal(s) | Interface conditioning between peripheral and RP2040 | Electrical behavior at RP2040 side |
|-----------|------------------|-------------------------------------------------------|------------------------------------|
| CAN controller/transceiver (U2 MCP25625) | GP2 (SCK), GP3 (MOSI), GP4 (MISO), GP5 (CSn), GP6 (INT), GP7 (RST), GP8 (STBY) | Direct 3.3 V digital SPI wiring, dedicated active-low interrupt line, reset line, and standby control line | SPI and control lines are 3.3 V CMOS; INT is active-low; CSn and RST are active-low outputs from RP2040 |
| Isolated input channel 1 (INPUT1 via U5 PC817C) | GP0 | 24 V field input drives optocoupler LED; transistor output side referenced to 3.3 V with 10 kΩ pull-up | RP2040 reads active-low logic (line pulled high when input is idle, pulled low when optocoupler conducts) |
| Isolated input channel 2 (INPUT2 via U8 PC817C) | GP1 | Same front-end as INPUT1 (optocoupler isolation + 3.3 V pull-up) | RP2040 reads active-low logic |
| DIP switch address selector (SW1, 5 positions) | GP9–GP13 | Each switch line uses 1 kΩ pull-up to 3.3 V and switch-to-GND when closed | Open switch reads high; closed switch reads low (inverted logic bits) |
| Relay driver 1 (J1 coil path) | GP14 | RP2040 drives BSS138 gate through 1 kΩ resistor; MOSFET sinks 24 V relay coil current | GPIO high turns MOSFET on and energizes relay; GPIO low releases relay |
| Relay driver 2 (J2 coil path) | GP15 | Same gate-drive topology as relay 1 | GPIO high turns MOSFET on and energizes relay; GPIO low releases relay |
| PIR motion sensor connector (U11) | GP26 | Sensor digital output routed through 1 kΩ series resistor, with 100 kΩ pull-down and 100 nF filtering | Motion event presented as active-high pulse to RP2040 |
| Ultrasonic power control (CN1 VCC path) | GP27 | GP27 controls TPS22918 load switch ON pin, which switches sensor supply rail | GPIO high powers ultrasonic module; GPIO low removes power |
| Ultrasonic trigger (CN1 TRIG) | GP28 | Direct digital output from RP2040 to sensor trigger pin | Firmware emits ≥10 µs high pulse to start measurement |
| Ultrasonic echo (CN1 ECHO) | GP29 | Echo line routed as digital input to RP2040 timing capture path | Pulse width at RP2040 input is proportional to measured distance |
| Status LED1 | LED1 GPIO (module pin) | RP2040 output drives LED through series resistor | Active-high LED drive |
| Status LED2 | LED2 GPIO (module pin) | RP2040 output drives LED through series resistor | Active-high LED drive |

---

## Control logic overview

The following logic is intended as a baseline for firmware development:

1. **Initialisation**
   - Read DIP switch (GP9–GP13) to determine the 5-bit CAN node address.
   - Assert MCP25625 reset (GP7 low), then release and configure the CAN controller via SPI (set bit-rate, acceptance filters, normal operating mode).
   - Power up the ultrasonic sensor (GP27 high).
   - Both relays start de-energised (GP14, GP15 low).

2. **Sensor polling / interrupt handling**
   - **PIR (GP26):** On rising edge, flag a motion event. Send a CAN frame reporting motion detected.
   - **Ultrasonic (GP28/GP29):** Periodically issue a trigger pulse and measure echo duration to compute distance. Send a CAN frame with the distance value. Optionally use distance threshold to detect vehicle presence.
   - **Isolated inputs (GP0, GP1):** On edge, report state change via CAN and/or directly toggle a relay (application-defined behaviour).
   - **CAN interrupt (GP6):** On falling edge, read received frame from MCP25625 via SPI and decode command.

3. **Relay control**
   - A received CAN command addressed to this node's ID sets or clears GP14 / GP15 to energise or de-energise the corresponding relay channel, switching the 24 V lighting load.
   - Relay state changes are echoed back on the CAN bus as status frames.

4. **Status LEDs**
   - LED1: blink on CAN frame activity (TX or RX).
   - LED2: solid when at least one relay is energised; off when both are open.

5. **Watchdog / error handling**
   - If no valid CAN frame is received within a configurable timeout, de-energise both relays (fail-safe off) and signal the error on LED2 (fast blink).
