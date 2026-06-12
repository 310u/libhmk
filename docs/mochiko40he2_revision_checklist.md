# mochiko40he2 Revision Checklist

Scope: next hardware revision review checklist for
`/home/satoyu/ドキュメント/mochiko40HE/mochiko38he/mochiko38he/mochiko40he2.kicad_sch`

Use this before ordering the next PCB revision.

Observed on current assembled boards:

- Bench check found `+3V3_A` missing.
- Bench check found `+3V8_PRE` at about `0.8V`.
- That makes the first bring-up priority the analog power chain, before ADC
  protocol debugging:
  - `+5V -> U2 (MPM3610AGQV-P) -> +3V8_PRE -> U8 (TLV75533PDBV) -> FB1 -> +3V3_A`
- The measured `0.8V` strongly suggests the `U2` feedback divider is configured
  near the regulator's FB reference instead of the intended `3.8V` output.

## 1. Blocking fixes

- [ ] Fix the `U2 (MPM3610AGQV-P)` output divider for `+3V8_PRE`.
  - Current schematic uses:
    - `R6 = 100k` from `+3V8_PRE` to `FB`
    - `R7 = 374k` from `FB` to `GND`
  - That sets the output to about `1.01V`, not `3.8V`.
  - For a `0.8V` FB reference, the divider should satisfy:
    - `Vout = 0.8 * (1 + Rtop / Rbottom)`
  - Example fix options:
    - keep `R6 = 100k`, change `R7` to about `26.7k`
    - keep `R7 = 374k`, change `R6` to about `1.40M`
  - Prefer the lower-impedance option unless there is a clear reason not to.
- [ ] Fix `ADS7953` reference wiring.
  - `REFP` must not be tied directly to `+3V3_A`.
  - Drive `REFP` from a valid external reference within the datasheet range.
  - Current best candidate: `2.5V` reference.
- [ ] Add the required reference capacitor for every `ADS7953`.
  - Place `10uF` directly between `REFP` and `REFM`.
  - Keep the loop short and local to each ADC.
- [ ] Re-check whether `AINP`, `AINM`, and `MXO` are wired per the intended ADS7953 usage.
  - Confirm the design really uses the internal mux path.
  - Confirm the chosen front-end matches the datasheet's `MXO -> AINP` application style.

## 2. Strongly recommended schematic fixes

- [ ] Review `GPIO3` handling on every `ADS7953`.
  - Current schematic pulls `GPIO3` down with `100k`.
  - On the TSSOP package, `GPIO3` can become the asynchronous active-low `PD` input.
  - If firmware ever enables that function, the ADC can power down immediately.
  - Safer options:
    - hard strap it to a safe state for the intended mode, or
    - leave it as a normal GPIO and document that in firmware, or
    - remove the pulldown unless there is a clear reason for it.
- [ ] Review `GPIO2` handling on every `ADS7953`.
  - `GPIO2` can become the hardware range select pin.
  - Decide explicitly whether range is controlled in hardware or only by register writes.
  - Make the strap match that decision.
- [ ] Decide whether `+VBD` should stay at `+3V3_D`.
  - This is allowed by the ADC datasheet.
  - Keep it only if the MCU I/O domain is definitely `3.3V`.
- [ ] Add a note in the schematic for the intended ADS7953 operating mode.
  - Manual mode
  - range mode
  - any GPIO repurposing
  - expected SPI clock target

## 3. Per-ADC review

Repeat for `U3`, `U9`, `U10`, and `U11`.

- [ ] `REFP` source is valid.
- [ ] `REFM` is tied to the correct analog ground.
- [ ] `+VA` and `+VBD` have local decoupling placed next to the pins.
- [ ] `CS`, `SCLK`, `SDI`, and `SDO` are routed to the intended MCU pins.
- [ ] `SDO` shared-bus behavior is acceptable for two devices on one MISO net.
- [ ] `GPIO2` and `GPIO3` default states are intentional.
- [ ] All unused analog inputs are treated intentionally.
  - Either documented as unused, or tied as recommended by the datasheet.

## 4. Hall sensor checks

The `MT9102ET` wiring looked consistent with the datasheet, but keep this pass in the next review.

- [ ] Verify the `SOT-23-3` pin order in the symbol and footprint.
  - Expected: `1=VCC`, `2=OUT`, `3=GND`.
- [ ] Verify every hall sensor still has the intended filter parts.
  - `100n` supply bypass
  - `4.7n` output capacitor
- [ ] Confirm the magnetic polarity and mechanical orientation match the intended key travel direction.
- [ ] Confirm the analog swing at the chosen magnet distance will stay inside the selected ADC reference range.

## 5. Footprint and library audit

This is a high-priority sanity pass because a correct schematic can still fail if the custom library is wrong.

- [ ] Verify the `ADS7953:TSSOP38` footprint pad numbering against the TI package drawing.
- [ ] Verify the `ADS7953` symbol pin numbering against the same TI package drawing.
- [ ] Verify the `PAW3395` custom footprint pin-1 orientation and pad numbering.
- [ ] Verify the `AT32F405RCT7` package choice matches the actually assembled part.
- [ ] Verify the `MT9102ET` footprint orientation on the PCB, not just the schematic symbol.
- [ ] Export a pin-to-net table for all custom-footprint parts and review it once manually.

## 6. Power-tree checks

- [ ] Confirm the intended rails and loads are documented:
  - `+5V`
  - `+3V8_PRE`
  - `+3V3_D`
  - `+3V3_A`
  - `+1V8_SENSOR`
- [ ] Re-check the regulator pinouts against the exact package variants used.
  - `TLV75533PDBV`
  - `TLV75518PDBV`
  - `MPM3610AGQV-P`
- [ ] Confirm ferrite bead direction and placement between digital and analog rails.
- [ ] Confirm each rail has enough local bulk capacitance near its consumers.
- [ ] Confirm the ADC reference source is not just "good enough power" but an actual suitable reference node.

## 7. USB and MCU checks

These looked broadly reasonable, but keep them in the release review.

- [ ] Keep separate `5.1k` pull-downs on `CC1` and `CC2`.
- [ ] Confirm the selected USB port mapping in firmware matches the actual routed pins.
- [ ] Confirm no later peripheral reassignment conflicts with the chosen USB pins.
- [ ] Confirm `BOOT0`, reset, and DFU entry hardware still match the bring-up plan.

## 8. Bring-up checklist for the next board

Do this before loading full keyboard firmware.

- [ ] Measure all rails first.
  - `+5V`
  - `+3V8_PRE`
  - `+3V3_D`
  - `+3V3_A`
  - `+1V8_SENSOR`
  - `ADS7953 REFP`
- [ ] Probe the analog power chain in order.
  - `+5V` at the input side
  - `+3V8_PRE` at `U2` output or `C163`
  - `U8` input and enable pins
  - `U8` output at `C173`
  - both sides of `FB1`
- [ ] Confirm `ADS7953 REFP` is inside the intended range before any SPI debugging.
- [ ] Probe one ADC's `CS`, `SCLK`, `SDI`, and `SDO` with a logic analyzer.
- [ ] Confirm `SDO` is not stuck low when `CS` is inactive.
- [ ] Test one board with a minimal ADC diagnostic firmware before enabling all peripherals.
- [ ] Confirm one held key changes raw ADC data before moving on to matrix/keymap work.
- [ ] Validate trackball separately from the key ADC path.

## 9. Suggested pass order

1. Fix `ADS7953` reference design.
2. Audit symbol and footprint pin numbering for all custom parts.
3. Re-check ADC GPIO strap strategy.
4. Re-run ERC and netlist review.
5. Build one minimal bring-up board first if possible.

## 10. Current best guess on root cause

- Immediate root cause found by measurement: `U2` feedback divider sets
  `+3V8_PRE` incorrectly, matching the observed `~0.8V to 1.0V` output range.
- Immediate bring-up blocker observed on hardware: `+3V3_A` is not present.
- Most likely schematic issue after power is restored: `ADS7953 REFP` design is out of spec.
- Secondary risk: fragile `GPIO3` pulldown strategy on all ADCs.
- Remaining open risk: custom footprint or symbol/pad numbering mismatch on a non-standard library part.
