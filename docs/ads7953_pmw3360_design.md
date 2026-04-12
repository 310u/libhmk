# ADS7953 / Trackball Design Note

This note captures the proposed architecture for a future keyboard that uses:

- `ADS7953` external SPI ADCs for the Hall-effect key matrix
- a central SPI optical sensor for the trackball

The goal is to turn the current direction into an implementation plan that fits
the existing `libhmk` runtime boundaries.

The intended production sensor is `PAW3395`.
Today, the trackball module includes both a `PMW3360` baseline path and an
initial `PAW3395` path so the common SPI plumbing does not need to be rewritten
when the production keyboard switches over.
`PAW3950` remains a future candidate, but needs validated vendor
documentation before firmware support should be claimed.

## Goals

- Replace the current MCU-ADC-plus-MUX matrix scan with an external SPI ADC
  backend.
- Use `4 x ADS7953`, arranged as `2 devices per side` and `2 SPI buses total`.
- Keep matrix processing, layout handling, profile handling, and HID plumbing as
  unchanged as possible.
- Use DMA for the ADC path to reduce scan jitter and CPU overhead.
- Add a central trackball sensor without coupling it to the analog matrix path.

## Non-Goals

- Do not generalize the public `spi_api` around DMA for the first
  implementation.
- Do not require the trackball sensor to use DMA in v1.
- Do not add a generic GPIO interrupt abstraction in v1.
- Do not try to solve arbitrary external ADC topologies in the first pass.
  The first target is specifically `2 SPI buses x 2 ADS7953 devices`.

## Hardware Topology

Recommended bus split:

```text
SPI0 (3.3V domain): ADS7953 right-0, ADS7953 right-1
SPI1 (3.3V domain): ADS7953 left-0,  ADS7953 left-1
SPI2 (trackball domain): central trackball sensor
```

Practical rules:

- Each ADS7953 gets a dedicated `CS`.
- The two ADS7953 buses share `SCLK/MISO/MOSI` only within each bus.
- The trackball sensor should live on its own SPI bus.
- The trackball sensor should not share the ADS7953 SPI buses because the ADC side is
  intended to run as a continuous DMA scan engine.
- The trackball sensor should expose a `MOTION` pin on the PCB even if v1 uses
  polling.
- Trackball click / side buttons should be wired as normal keys through
  `digital` GPIO inputs or other existing key paths, not through the PMW3360
  driver.

## Voltage / Board Assumptions

- `ADS7953` is expected to live in the keyboard's normal `3.3V` logic domain.
- The trackball sensor should be treated as its own sensor power domain. Board
  design must confirm the correct IO voltage, power sequencing, and any
  required level shifting from the chosen sensor design documents before
  implementation.
- The firmware design assumes that those electrical details are solved at the
  board level and presents the sensor as a dedicated SPI device plus optional
  `MOTION` GPIO.

## Trackball Sensor Candidates

### `PAW3395`

- Preferred production target for the new keyboard.
- Good architectural fit for the same dedicated trackball bus.
- PixArt's public product page lists `1.8-2.1V`, `4-wire SPI`, `26,000 cpi`,
  and `650 ips`, so the board-level topology remains compatible with the same
  overall split as `PMW3360`.
- The public `badjeff/paw3395-pcb` breakout is a useful concrete reference:
  it expects `3.3V` host-side wiring, regulates the sensor rail locally,
  exposes `MOTION` plus the SPI pins, and keeps `NRESET` on-board with a
  pull-up instead of routing it back to the MCU.
- A `TRACKBALL_SENSOR_PAW3395` path now exists in firmware, based on the
  vendor datasheet and cross-checks against a public STM32 bring-up example.
- The `trackball` module now defaults the `MOTION` GPIO to an internal pull-up
  for `PAW3395`, matching that breakout's active-low interrupt wiring model.
- Hardware validation is still required before treating that path as production
  ready.

### `PMW3360`

- Current implementation baseline.
- Good fit for a dedicated wired trackball bus.
- Useful as a known SPI bring-up reference while `PAW3395` support is pending.

### `PAW3950`

- Keep this as a possible future sensor, not an active firmware target.
- As of `2026-04-11`, it did not appear in PixArt's public optical mouse sensor
  product listing that also includes `PMW3360` and `PAW3395`.
- That means board-level planning can stay flexible, but firmware support
  should not be claimed until vendor documentation is available.

## Runtime Boundaries

The design should preserve the current separation of responsibilities:

- The SPI ADC backend only acquires samples.
- `analog_scan.c` continues to translate sampled values into key-facing storage.
- `matrix.c`, `layout.c`, `profile_runtime.c`, and the HID layer stay unaware
  of ADS7953 framing details.
- The trackball module bypasses the matrix path and emits pointer deltas through
  `hid_mouse_move()` / `hid_mouse_scroll()`.
- Trackball buttons stay outside the sensor driver and keep using normal key or
  HID button routing.

That means:

- External ADC work belongs under the existing analog backend boundary.
- Trackball work belongs in a separate input module, similar in spirit to
  joystick support, but without pretending to be an analog matrix input.

## Configuration Model

### Analog Backend Selection

The existing `analog.backend = "spi_adc"` hook should be promoted from
placeholder to real support.

### Proposed `keyboard.json` Shape

The current `analog.raw` / `analog.mux` schema is MCU-ADC-shaped. For ADS7953,
add a backend-specific section under `analog.spi`.

Example:

```json
"analog": {
  "backend": "spi_adc",
  "adc_resolution": 12,
  "invert_adc": false,
  "spi": {
    "driver": "ads7953",
    "buses": [
      {
        "bus": 0,
        "devices": [
          { "cs": "B0", "map": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0, 0] },
          { "cs": "B1", "map": [15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 0, 0] }
        ]
      },
      {
        "bus": 1,
        "devices": [
          { "cs": "B2", "map": [29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 0, 0] },
          { "cs": "B3", "map": [43, 44, 45, 46, 47, 48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] }
        ]
      }
    ]
  }
}
```

Notes:

- `map` is a `16`-entry vector per ADS7953 device.
- The values should follow the same physical-key-number convention already used
  by `analog.raw.vector`: `1` means key index `0`, and `0` means unused.
- Values above `NUM_KEYS` may be reserved for raw-only channels if future
  analog side inputs need to remain addressable through `analog_read_raw()`.

### Proposed Trackball Board Configuration

For the first implementation, the trackball sensor should be configured through
`board_def.h`, not `keyboard.json`, to keep the scope small.

Suggested macros:

```c
#define TRACKBALL_ENABLED 1
#define TRACKBALL_SENSOR_PMW3360 1
#define TRACKBALL_SPI_BUS 2
#define TRACKBALL_SPI_MODE SPI_BUS_MODE_3
#define TRACKBALL_CS_PORT GPIOA
#define TRACKBALL_CS_PIN GPIO_PINS_4
#define TRACKBALL_MOTION_PORT GPIOA
#define TRACKBALL_MOTION_PIN GPIO_PINS_5
#define TRACKBALL_SWAP_XY 0
#define TRACKBALL_INVERT_X 0
#define TRACKBALL_INVERT_Y 0
#define TRACKBALL_CPI_DEFAULT 1600
```

If later bring-up needs extra pins such as reset or shutdown, they can be added
to `board_def.h` without disturbing the higher-level design.

In other words:

- use `TRACKBALL_SENSOR_PMW3360` as the conservative bring-up baseline
- use `TRACKBALL_SENSOR_PAW3395` for the intended production build once the
  target board has been validated on hardware

## Build-Time Macro Design

The current generic analog header assumes that every sampled input fits within
the MCU's built-in ADC channel count. That is no longer true for external SPI
ADCs.

Recommended refactor:

- Keep `ADC_NUM_CHANNELS` as an MCU-ADC-only concept.
- Introduce a backend-neutral sampled-input count, for example:
  `ANALOG_NUM_SAMPLED_INPUTS`.
- Derive `ANALOG_NUM_SAMPLED_INPUTS` from
  `ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS`.
- Move MCU-specific channel-range assertions out of the generic header and into
  the MCU ADC backend implementation.

For `spi_adc`, `scripts/make.py` should flatten `analog.spi.buses[].devices[].map`
into the existing raw-input representation:

- `ADC_NUM_MUX_INPUTS = 0`
- `ADC_NUM_RAW_INPUTS = 64` for `4 x ADS7953`
- `ADC_RAW_INPUT_VECTOR = flattened map`

This keeps `analog_scan.c` unchanged and lets the SPI ADC backend feed a single
flat sample vector into the existing generic storage path.

Additional backend-specific macros will still be needed, for example:

- `SPI_ADC_DRIVER_ADS7953`
- `SPI_ADC_NUM_DEVICES`
- `SPI_ADC_DEVICE_BUS`
- `SPI_ADC_DEVICE_CS_PORTS`
- `SPI_ADC_DEVICE_CS_PINS`

## ADS7953 Runtime Design

### Design Choice

The first backend should target `AT32F405xx` only.

Reasons:

- The current project already uses DMA heavily on AT32.
- The AT32 side already has working patterns for non-blocking ADC DMA and
  double-buffered RGB DMA.
- Keeping the first implementation to one MCU family reduces the amount of
  SPI-DMA and IRQ plumbing that has to be validated at once.

### Scan Strategy

The first implementation can use a deterministic `manual-mode` sweep per
device instead of `Auto-1`.

Why:

- The physical key order does not need to match scan order because build-time
  mapping can reorder channels freely.
- A fixed `18`-frame burst per device (`16` channels plus the ADS7953 pipeline
  flush) keeps every DMA burst self-contained.
- Resetting the sweep at the start of every DMA burst makes channel alignment
  deterministic even when buses run independently.
- The backend hot path stays simple: start DMA, parse returned channel tags,
  publish the flattened sample vector, repeat.

This still leaves `Auto-1` as a valid future optimization if hardware testing
shows a clear benefit.

### Private SPI-DMA Implementation

The public `spi_api` should stay blocking and generic for now.

The ADS7953 backend should instead configure SPI directly inside the analog
backend implementation because it needs:

- DMA-capable SPI transfers
- device-private state machines
- likely `16-bit` frame handling
- tight CS timing between back-to-back device sweeps

This keeps DMA complexity local to the ADC backend rather than forcing every
SPI user to adopt a more complicated public abstraction.

### Per-Bus State Machine

Each ADC bus owns:

- one SPI instance
- one TX DMA channel/stream
- one RX DMA channel/stream
- two chip selects
- a small bus state machine

Suggested state model:

- `IDLE`
- `DEVICE0_ACTIVE`
- `DEVICE1_ACTIVE`
- `WAITING_FOR_PEER`
- `ERROR`

### Buffer Model

Recommended buffers:

- `working_samples[64]`
- one RX DMA buffer per bus for one device sweep
- one static TX command buffer for one device sweep

A minimal approach is enough:

- RX DMA captures `16` words from device 0
- ISR copies parsed results into the correct slice of `working_samples`
- ISR starts device 1 on the same bus
- RX DMA captures `16` words from device 1
- ISR copies parsed results into the next slice
- bus is marked complete

Once both buses complete the current round:

- call `analog_scan_store_samples(working_samples, 0)`
- mark initialization complete after the first full round
- immediately start the next round

This preserves the existing "analog backend continuously refreshes samples"
behavior.

### Concurrency Model

The two ADC buses should run in parallel.

One full round is therefore bounded by one side:

- `16 channels`
- `2 devices`
- `16 clocks per conversion frame`

At `20 MHz` SPI, the ideal transfer time per side is roughly:

```text
16 channels x 2 devices x 16 clocks / 20 MHz = 25.6 us
```

Real scan time will be slightly larger because of:

- CS toggling
- DMA re-arming
- ISR overhead

But the important point is that the design budget is comfortably below typical
USB polling intervals.

### `analog_init()` / `analog_task()` Semantics

For the SPI ADC backend:

- `analog_init()` should initialize SPI, DMA, GPIO CS pins, configure all four
  ADS7953 devices, start the first scan round, and wait until at least one full
  round has completed.
- `analog_task()` should remain lightweight.

Preferred `analog_task()` behavior:

- do nothing in the steady state
- optionally detect and recover from DMA stall / error conditions

That keeps the hot path interrupt-driven, similar to the current MCU ADC
backend.

## Trackball Design

### Module Boundaries

Add a new module:

- `include/trackball.h`
- `src/trackball.c`

Public API:

- `trackball_init()`
- `trackball_task()`

And call it from `main.c`.

### Data Path

The trackball should not enter the matrix path.

Instead:

- read the active trackball sensor's motion data
- apply axis transform / sensitivity / optional scroll mode logic
- emit deltas through `hid_mouse_move()` or `hid_mouse_scroll()`

This is compatible with the current HID implementation because mouse movement
is already accumulated in `hid.c` until the host is ready.

### SPI Usage

The trackball sensor should use the existing public blocking `spi_api`.

Reasons:

- It is a single device on its own bus.
- Motion reads are small.
- The current concern is not bandwidth, but keeping the ADC DMA engine
  isolated.
- Avoiding DMA here keeps the first implementation much smaller.

### Polling Model

The first implementation should use polling.

Recommended behavior:

- if a `MOTION` pin is defined, poll the GPIO first and skip SPI reads when no
  motion is pending
- if no `MOTION` pin is defined, poll the sensor status path at a modest fixed
  cadence
- when motion exists, perform the sensor-specific motion read sequence and
  accumulate deltas

This avoids adding a generic EXTI layer before the rest of the feature is
proven.

### Pointer Buttons

The trackball sensor driver should not own mouse buttons.

Instead:

- trackball click / thumb buttons should map to existing key or mouse-button
  keycodes
- `trackball_task()` should ideally pass `buttons = 0` to `hid_mouse_move()`
  and `hid_mouse_scroll()`

That avoids conflicts with any other pointer-capable module that might also use
the shared HID mouse interface.

### Main Loop Placement

The cleanest placement is before `layout_task()`, so the same loop iteration
can still flush HID reports.

Preferred future order:

```text
analog_task()
matrix_scan()
encoder_task()
joystick_task()   // optional improvement: move here as well
trackball_task()
layout_task()
...
```

Rationale:

- `layout_task()` already calls `hid_send_reports()`
- pointer deltas produced before `layout_task()` can ride the same HID send pass
- this reduces one-loop pointer latency compared with running the trackball task
  afterward

## Phased Implementation Plan

### Phase 1: Generic Plumbing

- Enable `analog.backend = "spi_adc"` in `scripts/make.py`
- add `analog.spi` schema validation
- refactor generic analog count checks so external ADCs are representable
- flatten ADS7953 channel maps into the existing raw-input vector model

### Phase 2: AT32 ADS7953 Backend

- add AT32-only SPI ADC backend implementation
- implement `2 buses x 2 devices`
- implement DMA scan engine
- feed the flat `64`-sample vector into `analog_scan_store_samples()`
- validate the path with a reference keyboard target that enables the backend

### Phase 3: Basic Trackball Support

- add `trackball.c`
- add board-level pin macros
- implement `PMW3360` first using blocking-SPI motion polling and mouse output
- keep the module internally split into common trackball plumbing and
  sensor-specific operations so `PAW3395` can be added later
- defer SROM upload and advanced tuning until hardware validation
- keep configuration compile-time only

### Phase 4: `PAW3395` Driver

- add a real `TRACKBALL_SENSOR_PAW3395` path
- validate SPI mode, reset sequence, burst format, CPI control, and any
  required firmware upload flow from official documentation
- verify that the `PMW3360`-based common layer was split at the right boundary
- migrate the production keyboard definition to `PAW3395`

Status:

- implemented in firmware with a dedicated init sequence, identity check, CPI
  programming, and 12-byte motion burst reads
- still waiting on board-level hardware validation and production keyboard
  migration

### Phase 5: Tuning and User-Facing Controls

- add persistent trackball settings if needed
- add optional scroll mode / CPI / axis transform controls
- add diagnostics for ADC round timing and trackball motion activity

## Testing Plan

### Unit / Native Tests

- build-flag flattening for `analog.spi`
- mapping validation for `4 x 16` channel vectors
- pure logic tests for any trackball axis transform helper
- any state-machine helpers that can be tested without hardware

### On-Device Validation

For ADS7953:

- confirm all `64` channels are read in the expected order
- confirm left/right halves both update while the board is under load
- confirm scan jitter remains stable with RGB and USB active
- confirm recalibration and bottom-out persistence still behave normally

For the first implemented sensor path:

- confirm no motion is lost during fast flicks
- confirm no pointer stalls while the ADC DMA engine is active
- confirm USB suspend/resume does not wedge the sensor path

## Open Questions

- Exact AT32 DMA / DMAMUX request assignments for the chosen SPI peripherals
  still need to be matched to the final board pinout.
- The chosen trackball sensor board design still needs final confirmation for
  power domain, IO levels, and any required sensor-specific bring-up sequence.
- `PAW3395` is now a supported firmware target for bring-up, but it still needs
  board-level validation before it should be treated as production ready.
- `PAW3950` still needs vendor documentation before it should be treated as a
  firmware target.
- If STM32F446 support is required later, it should be treated as a follow-up
  port after AT32 is stable.

## External References

- TI ADS7953 product page: <https://www.ti.com/product/ADS7953>
- TI ADS7953 datasheet: <https://www.ti.com/lit/gpn/ads7953>
- PixArt optical sensor comparison: <https://www.pixart.com/products-comparison/7/Optical_Mouse_Sensor>
- PixArt PMW3360 product page: <https://www.pixart.com/products-detail/tw/10/PMW3360DM-T2QU>
- PixArt PAW3395 product page: <https://www.pixart.com/products-detail/129/PAW3395DM-T6QU>
