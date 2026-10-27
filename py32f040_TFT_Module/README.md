# TFT module: project foundation

Copied from Py32f040_ProjectTemplate as required by the module workflow.
**This is not an implemented TFT driver.** It currently configures only the
PY32F040C1BT6 clock: fitted 24 MHz HSE, PLL x3 to 72 MHz, flash latency 2,
and consistent HAL/CMSIS HSE_VALUE. Clock failure halts without HSI fallback.
No LCD, SPI host or SD GPIO/peripheral is initialized or driven.

The owner confirmed on 2026-10-27 that the initial scope is **display drawing
only**. SD support is excluded. The exact TFT module purchase/datasheet link
is still awaited; controller type, resolution, initialization, parallel timing,
orientation, pixel order and 5 V logic compatibility must be established from
that module before implementing the driver. A 5 V power pin alone does not
establish 5 V signal compatibility.

## Confirmed carrier wiring

[Schematic](../Modules%20Schematics/Schematic_TFT_Module.pdf),
[module reference](../docs/modules/TFT.md),
[commented pin definitions](User/Inc/tft_board.h).

| Signal | MCU GPIO / package pin |
|---|---|
| Host SPI1 NSS | PA15 / 38 |
| Host SPI1 SCK | PB3 / 39 |
| Host SPI1 MISO | PB4 / 40 |
| Host SPI1 MOSI | PB5 / 41 |
| LCD D0..D7 | PA0..PA7 / 10..17 |
| LCD RS | PB0 / 18 |
| LCD WR | PB1 / 19 |
| LCD RD | PA11 / 32 |
| LCD CS | PB8 / 45 |
| LCD RESET | PB9 / 46 |

SPI header: 1 MISO, 2 MOSI, 3 SCK, 4 5 V, 5 CS, 6 GND. The main ESP32
connects directly through this TFT header, bypassing the UART hub. Shared
simultaneous TFT/LED-matrix selection has not been specified.

Display connector wiring: H1 pins 3..8 = D2..D7; H2 pins 1/2 = D0/D1;
H3 pins 2..6 = RESET/CS/RS/WR/RD; H4 pins 2/4 = GND/5 V. The unused-in-v1
SD SPI2 pins are PB12 CS, PB13 SCK, PB14 MISO, PB15 MOSI. They are recorded
in tft_board.h for reference only. No touch input is specified by this carrier.

## Next implementation work

After the owner supplies the exact TFT link, verify the module's electrical
interface and controller documentation. Then implement controller initialization,
parallel GPIO timing, bounded-memory drawing and the ESP32 SPI command protocol.
Do not assume a full framebuffer fits the 16 KB RAM or choose a resolution from
the 2.4-inch diagonal alone. Add ARM-emulated tests for actual command/drawing
logic once it exists; no tests are claimed for an unimplemented driver.

Run make all in this directory to build the clock-only foundation. The build
passes: 3,600 bytes flash and 1,584 bytes linked RAM including template
reservations. This validates only compilation/linkage, not display operation. Inherited
programming/debug instructions are in [SETUP_WINDOWS.md](SETUP_WINDOWS.md).
No physical display or board validation has occurred.
