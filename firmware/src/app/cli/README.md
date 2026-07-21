# CLI module (`src/app/cli`)

This folder contains the presentation layer of the BIOS-style serial console.
Its goal is to separate:
- UI state (model)
- theme and rendering primitives
- declarative page layout

Application logic and hardware access stay in `src/app/serial_cli.cpp`.

## File overview

- `model.h`
  - Pure snapshot of the data shown in the UI (`Model`).
  - Defines tabs, selections, and displayed values (input/output, distance, ultrasonic mode).
  - Has no Arduino/FreeRTOS dependencies.

- `theme.h`
  - Color palette and styles (`Style`) used by the entire renderer.
  - Single place to change look and feel.

- `screen.h` / `screen.cpp`
  - ANSI positioned drawing primitives (`put`, `field`, `frame`, `fillRow`, `fillRows`).
  - `field()` writes fixed-width cells to prevent flicker during partial refreshes.

- `pages.h` / `pages.cpp`
  - Declarative definition of pages (`Page`) and dynamic fields (`Field`).
  - Each field declares position, width, formatter (`FormatFn`), and style (`StyleFn`).
  - Exposes `kPages` with 6 tabs:
    - STATUS
    - INPUTS (adds the last ultrasonic reading; shows "disabled" when the sensor mode is OFF)
    - OUTPUTS (adds the ultrasonic activation mode as a cyclable OFF/ON/AUTO row)
    - RTOS (read-only FreeRTOS task table: state, priority, free stack, CPU share, plus heap/uptime)
    - CAN (read-only MCP25625 transceiver diagnostics: link state from the boot-time loopback self-test, RX frame count, error flags)
    - CANOPEN (read-only CANopenNode protocol diagnostics: NMT state, node-ID, heartbeat producer activity)

## How rendering works

Rendering is orchestrated by `SerialCli` (outside this folder):

1. terminal connected: full redraw
2. tab change: redraw tabs + content area
3. same tab: redraw only changed fields (text/style diff)

To support this behavior, this folder provides:
- `pages.cpp` for text formatting functions and style rules
- `screen.cpp` for atomic draw operations and style reset after each draw

## `Field` and `Page` contract

`Field`:
- `x, y`: 1-based terminal position
- `w`: maximum cell width
- `format(model, buf, len)`: generates text
- `style(model)`: field style (`nullptr` => default style `theme::kValue`)

`Page`:
- `tabLabel`: tab label
- `drawStatic(screen)`: draws static elements
- `fields`: dynamic fields list
- `fieldCount`: number of fields
- `selectableCount`: number of selectable elements (for Up/Down + Enter)

## Supported user input (handled by `SerialCli`)

- Tab / Left-Right arrows: switch page
- Up-Down arrows: select element in active page
- Enter / Space: run action on selected element
- R (or Ctrl+L): full redraw

## How to add or modify a page

1. Add formatter/style functions in `pages.cpp` (if needed).
2. Define the static block (`draw...Static`).
3. Define the `Field` array with correct coordinates and widths.
4. Register the page in `kPages`.
5. Set `selectableCount` to match interactive elements.

Practical notes:
- Keep text short: each field is truncated/padded to `w`.
- Avoid overlap between static frames and dynamic fields.
- If behavior is visual-only, work in this folder; if hardware actions are needed, update `serial_cli.cpp`.

## Dependencies

- ANSI library (`ansi.h`) for terminal rendering.
- No direct RTOS task dependencies in this folder.

## Quick references

- Runtime integration: `src/app/serial_cli.h` and `src/app/serial_cli.cpp`
- Application events: `src/app/events.h`
