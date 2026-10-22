# LED matrix module

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose

Display patterns on a chain of 48 addressable LEDs using a local PY32 controller.
Image/animation features, brightness range and host commands remain to be defined.

## Confirmed hardware

Two-page schematic: PY32F040C1BT6, 5 V, 24 MHz crystal; LED1..LED48 connected
DI-to-DO in a chain. DIN reaches LED1 through 330 ohms. Per-LED decoupling and
a 1000 uF bulk capacitor are shown. The LEDs' exact model and serial timing/
colour order are not identified by the generic LED symbol text.

| Signal | GPIO | MCU package pin |
|---|---|---:|
| Host TFT_CS | PA15 | 38 |
| Host TFT_SCK | PB3 | 39 |
| Host TFT_MISO | PB4 | 40 |
| Host TFT_MOSI | PB5 | 41 |
| LED DIN | PB15 | 28 |

The six-pin host SPI header is 1 MISO, 2 MOSI, 3 SCK, 4 5 V, 5 CS, 6 GND.
It uses TFT_* net names even though this is the LED matrix board.
Source: [schematic, pages 1 and 2](../../Modules%20Schematics/Schematic_LED_MATRIX_UART.pdf).

## Firmware and protocol status

Not implemented. The owner confirmed that the module communicates directly
with the main board ESP32 through the TFT SPI header, bypassing the UART hub.
Start from the F040 template; proposed project name
`py32f040_LED_Matrix_Module` (not created). The owner will rename the historical
`Schematic_LED_MATRIX_UART.pdf` later; the UART suffix is a naming error,
not an interface requirement. Keep the existing source link until it is renamed.

## Open decisions and next work

Confirm exact LED part, bit timing, colour order, physical matrix
orientation and pixel-index mapping. Define brightness/power limits, frames,
buffering, animation features and incomplete-frame handling. Choose a waveform
generation method that meets the exact LED timing without disrupting host IO.
This display connector is not the expansion-chain SPI connector pinout.
The main board shows one TFT header/chip-select path. Using both display boards
simultaneously would need an agreed connection/selection scheme; shared-bus
operation has not been specified.

## Validation

No firmware build or hardware test. Later verify first/last pixel, chain order,
all channels/colours, frame timing, brightness/power behavior and host-load
interaction. The schematic grid is not proof of the final viewing orientation.
