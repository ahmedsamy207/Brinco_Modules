# LED matrix module

Last reviewed: 2026-10-25. [All modules](README.md)

## Purpose

Display patterns on a chain of 48 addressable LEDs using a local PY32 controller.
Implemented commands include RGB frames, fill, pixel, clear, brightness and status.

## Confirmed hardware

Two-page schematic: PY32F040C1BT6, 5 V, 24 MHz crystal; LED1..LED48 connected
DI-to-DO in a chain. DIN reaches LED1 through 330 ohms. Per-LED decoupling and
a 1000 uF bulk capacitor are shown. The LEDs' exact model and serial timing/
colour order were subsequently confirmed by the owner-provided C22461793
datasheet: XINGLIGHT XL-5050RGBC-WS2812B-S, GRB/MSB-first at 800 kbit/s.
See the [exact LED datasheet](../../Module%20Datasheets/XL-5050RGBC-2812B-S_C22461793.pdf).

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

Implemented. The owner confirmed that the module communicates directly
with the main board ESP32 through the TFT SPI header, bypassing the UART hub.
Copied from the F040 template into
[py32f040_LED_Matrix_Module](../../py32f040_LED_Matrix_Module/README.md).
The project README defines the fixed 160-byte CRC-protected SPI protocol,
acknowledgement pipeline and portable host helper. SPI1 uses AF0, while
PB15 uses AF1 TIM15_CH2 with update DMA1 channel 1. The fitted 24 MHz crystal
feeds PLL x3 for 72 MHz. Default brightness is 32/255; source RGB is retained.
Invalid/incomplete packets leave pixels unchanged. The owner will rename the historical
`Schematic_LED_MATRIX_UART.pdf` later; the UART suffix is a naming error,
not an interface requirement. Keep the existing source link until it is renamed.

## Open decisions and next work

Verify physical matrix orientation and XY mapping; indices 0–47 currently
follow schematic LED1–LED48. Define production brightness/power limits and any
future animation features. TIM15/DMA generates the waveform with SPI IRQs enabled.
This display connector is not the expansion-chain SPI connector pinout.
The main board shows one TFT header/chip-select path. Using both display boards
simultaneously would need an agreed connection/selection scheme; shared-bus
operation has not been specified.

## Validation

Build passes: 8,768 bytes flash and 5,568 bytes static RAM. All 13 ARM-emulated
software tests pass with mocked HAL registers and LED output. No hardware test;
FIFO/IRQ/DMA electrical timing remains unverified. Later verify first/last pixel, chain order,
all channels/colours, frame timing, brightness/power behavior and host-load
interaction. The schematic grid is not proof of the final viewing orientation.
