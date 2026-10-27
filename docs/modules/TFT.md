# TFT display and touch module

Last reviewed: 2026-10-27. [All modules](README.md)

## Purpose and agreed scope

Bridge ESP32 display requests to the LCDWIKI MAR2406 ILI9341 shield and return
resistive touch readings. The owner supplied the exact product link and added
touch to scope on 2026-10-27. SD remains excluded. Firmware is implemented in
[py32f040_TFT_Module](../../py32f040_TFT_Module/README.md), copied from the F040 template.

## Confirmed hardware

PY32F040C1BT6 at 5 V, fitted 24 MHz crystal with PLL x3 to 72 MHz. The owner's
[LCDWIKI product](https://www.lcdwiki.com/2.4inch_Arduino_Display) documents
ILI9341, 240x320 native pixels, an 8-bit parallel bus, onboard 5 V/3.3 V level
conversion and resistive touch. The separately linked ST7789V variant is not
supported by this build. Verify the delivered shield/controller variant.

| Signal/group | MCU GPIO |
|---|---|
| Host TFT_CS / TFT_SCK | PA15 / PB3 |
| Host TFT_MISO / TFT_MOSI | PB4 / PB5 |
| LCD_D0..LCD_D7 | PA0..PA7 respectively |
| LCD_RS / LCD_WR / LCD_RD | PB0 / PB1 / PA11 |
| LCD_CS / LCD_RST | PB8 / PB9 |
| Touch XP / YM / XM / YP | PA0 / PA1 / PB0 / PB8, shared with D0/D1/RS/CS |
| Unused SD_SS / SD_SCK / SD_MISO / SD_MOSI | PB12 / PB13 / PB14 / PB15 |

Host header: 1 MISO, 2 MOSI, 3 SCK, 4 5 V, 5 CS, 6 GND. This is the dedicated
ESP32 TFT path, bypassing the UART hub. Simultaneous TFT/LED-matrix selection
on the single CS path remains unspecified. Carrier source:
[schematic](../../Modules%20Schematics/Schematic_TFT_Module.pdf).

The manufacturer's MAR2406 touch examples use Arduino XP=8, YM=9, XM=A2,
YP=A3; generic library examples differ. PB8 has no ADC, so firmware senses the
opposite floating electrode of each resistive layer using PA0/PA1 ADC0/1.
Touch temporarily borrows four pins, keeps WR/RD inactive, switches excitation
through high-Z and restores LCD outputs even on ADC errors. SD CS is parked
HIGH; SPI2 and SD operations are not implemented. See
[implementation references/license](../../py32f040_TFT_Module/ThirdParty/README.md).

## Implemented behavior

RGB565 clear, filled rectangles, lines, row-pixel upload and four rotations.
No framebuffer; rendering is limited to 64 pixels per foreground call so touch
and host service continue. Host-rendered images/fonts use row chunks of up to
69 pixels. Native fonts, image decoders, circle primitives and autonomous UI
are not implemented. Pixel updates are progressive and not frame synchronized.

SPI mode 0, <=250 kHz, exactly 160 bytes per CS pulse, CRC-16, sequence/command
acknowledgements and FETCH. The complete framing/commands/host callback contract
is defined in the project README. Drawing ACK means queued; poll busy for
completion. INFO reports configured ILI9341 identity, not probed chip identity.

Touch is polled every 20 ms, median-filtered with two-scan contact qualification,
100 ms freshness checks and a configurable pressure proxy band. Raw values are
available before calibration; pixel coordinates stay 0xFFFF until a valid,
calibrated contact exists. CALIBRATE accepts portrait raw endpoints, reversed
axes and optional axis swap; coordinates then follow display rotation. Calibration
is RAM-only and may be persisted/resent by the ESP32. There is no touch event
queue; short taps between polls can be missed.

## Validation and remaining work

Build passes: 11,732 bytes flash, 2,400 bytes linked RAM including vendor heap/
stack reservations. All 18 ARM-emulated renderer/touch-mapping/protocol/SPI/host
helper tests pass. LCD/ADC IO is mocked; real tft_io.c, HAL, electrical timing,
GPIO restoration and optical behavior are not simulated. No physical tests have
occurred. Verify initialization, RGB bars, geometry/rotation, shared-pin touch
sampling and ADC settling under concurrent SPI, calibration accuracy, drags/
release/reset, stack headroom and first/last SPI bytes. Measure panel sheet
resistance and GPIO excitation currents at 5 V against the MCU ratings before
hardware touch tests; absolute maximum current is not a normal design target.
