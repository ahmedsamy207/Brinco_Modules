# Module development index

Last reviewed: 2026-10-27. Start here when joining or resuming the project.

These pages describe the agreed purpose, schematic wiring, implemented behavior,
open decisions, and next steps for every board. Hardware is in manufacturing:
**no module has been validated on physical boards yet**. A successful build is
not a hardware test. Update the relevant page whenever requirements, code,
protocols, schematics, or validation results change.

| Board/module | Current state | Reference |
|---|---|---|
| Main board | Hardware reviewed; ESP/PY32 hub firmware not implemented | [Main Board](Main_Board.md) |
| UART expansion board | Hardware reviewed; two-hub forwarding firmware not implemented | [Expansion Board](Expansion_Board.md) |
| Ultrasonic | UART firmware implemented and built; hardware validation pending | [Ultrasonic](Ultrasonic.md) |
| IR array | 3/5-sensor firmware implemented and both builds pass; calibration pending | [IR Array](IR_Array.md) |
| Colour sensor | RGBW firmware builds; 15 software tests pass; corrected schematic with 4.7 kohm pull-ups reviewed; physical validation pending | [Colour Sensor](Color_Sensor.md) |
| Sound sensor | Relative level/detection firmware builds; 13 software tests pass; hardware validation pending | [Sound Sensor](Sound_Sensor.md) |
| Touch sensor | UART firmware builds; nine software tests pass; hardware validation pending | [Touch Sensor](Touch_Sensor.md) |
| Stepper motor | Hardware reviewed; control requirements and firmware pending | [Stepper Motor](Stepper_Motor.md) |
| LED matrix | SPI/DMA firmware builds; 13 software tests pass; hardware validation pending | [LED Matrix](LED_Matrix.md) |
| TFT display | ESP32 TFT-header SPI interface confirmed; firmware pending | [TFT](TFT.md) |
| I2S/audio board | Purpose agreed; ESP audio drivers/features pending; no onboard MCU | [I2S Audio](I2S_Audio.md) |

## Shared decisions

- After completing and validating a coherent set of project changes, commit
  and push it to the active GitHub development branch automatically, as
  requested by the project owner. Report the commit and any push failure;
  this does not authorize merging or rewriting remote history.

- Always use the fitted external crystal: F003 at 32 MHz HSE directly;
  F040 at 72 MHz from 24 MHz HSE and PLL x3. Keep HAL/CMSIS HSE_VALUE,
  flash latency and peripheral dividers consistent. No silent HSI fallback.

- Start each new MCU module by copying the matching PY32F003 or PY32F040
  template; name the resulting project `py32f0xx_<ModuleName>_Module`.
- Preserve the legacy `Py32f003_Ultrasonic` as historical I2C firmware. The
  current ultrasonic UART implementation is a separate project.
- Document application code: purpose, pin mapping, APIs, units, timing,
  interrupt ownership, protocol fields, errors, and calibration assumptions.
- Main-board UART net labels describe the main board's perspective. A module
  net labelled main RX is the module's TX, and vice versa.
- Reverse-side JST cables intentionally connect hub pins 1/2/3/4 to module
  pins 4/3/2/1. See the main board reference for power and signal mapping.
- Six-pin SPI IN/OUT numbering uses its own mapping; do not reuse the JST rule.
- Main and expansion PY32 hubs forward between SPI and individual UART links.
  SPI routing, discovery, framing and timing are not implemented or agreed yet.
- Existing ultrasonic/IR/touch/sound/colour UART implementations use 115200 baud, 8N1. This is
  not an automatically approved protocol specification for unimplemented boards.
- The owner confirmed that TFT and LED matrix communicate directly with the
  main board ESP32 through the TFT SPI header, bypassing the UART hubs.
  `Schematic_LED_MATRIX_UART.pdf` is a historical filename; the owner plans to
  rename it. Its UART suffix does not describe the interface.

## Evidence and maintenance

Hardware mappings come from the PDFs linked on each page; product intent comes
from the project owner's decisions. Proposed folder names and future work are
labelled explicitly. Component register sequences, timing limits and electrical
details must be checked against exact component datasheets before implementation.

For implemented modules, project READMEs and source define the current protocol;
these overview pages link to them instead of maintaining competing copies.
Record real test results, board revisions and calibration values when hardware
arrives. Do not mark a module complete solely because its project compiles.
