# TFT display module

Last reviewed: 2026-10-27. [All modules](README.md)

## Purpose

Bridge host display requests to a 2.4-inch parallel TFT, with a separate SD
interface available on the display headers. The owner confirmed on 2026-10-27
that the first version is display drawing only; SD support is excluded.
Specific drawing features await the exact display module/controller details.

## Confirmed hardware

PY32F040C1BT6 at 5 V, 24 MHz crystal. The host header is SPI; local LCD data
is an eight-bit parallel bus. Exact TFT controller and SD hardware details
are not identified by the header-only display drawing.

| Signal/group | MCU GPIO |
|---|---|
| Host TFT_CS / TFT_SCK | PA15 / PB3 |
| Host TFT_MISO / TFT_MOSI | PB4 / PB5 |
| LCD_D0..LCD_D7 | PA0..PA7 respectively |
| LCD_RS / LCD_WR / LCD_RD | PB0 / PB1 / PA11 |
| LCD_CS / LCD_RST | PB8 / PB9 |
| SD_SS / SD_SCK / SD_MISO / SD_MOSI | PB12 / PB13 / PB14 / PB15 |

Host header numbering: 1 MISO, 2 MOSI, 3 SCK, 4 5 V, 5 CS, 6 GND.
This is the main board's dedicated TFT SPI path, not automatically the PY32
expansion-chain connection. Source:
[schematic](../../Modules%20Schematics/Schematic_TFT_Module.pdf).

## Firmware and protocol status

Controller/graphics/host protocol not implemented. The owner confirmed the host is the main board ESP32,
connected through the dedicated TFT SPI header, bypassing the PY32 UART hub.
The F040 template has been copied into
[py32f040_TFT_Module](../../py32f040_TFT_Module/README.md). It is a clock-only
foundation with commented carrier pin definitions, using the fitted 24 MHz
external crystal and PLL x3 to 72 MHz. It does not initialize or drive LCD,
host SPI or SD pins. Exact display purchase/datasheet link is awaited from
the owner before controller initialization or bus timing can be implemented.
No drawing command set, screen dimensions in pixels, pixel format, SPI mode/
rate or frame/error protocol has been approved.

## Open decisions and next work

Obtain the actual TFT controller datasheet/module pinout and define initialization,
parallel timing, orientation, colour format, voltage compatibility and graphics
scope. A 5 V supply header alone is not proof of 5 V signal tolerance. SD is
excluded from version 1. Define streaming/local rendering and memory budgets before
allocating buffers. A full framebuffer must not be assumed to fit MCU RAM.
The schematic does not define a touch-panel feature.
The main board schematic shows one TFT host header/chip-select path; simultaneous
TFT and LED matrix attachment would need an agreed connection/selection scheme.
The owner has confirmed the SPI interface, not simultaneous shared-bus operation.

## Validation

The clock-only project foundation builds (3,600 bytes flash, 1,584 bytes linked
RAM including template reservations); no controller logic tests
or physical hardware tests have occurred. Later validate controller initialization,
parallel read/write timing, colour bars, dimensions/orientation, host command
parsing, throughput, incomplete updates and SD operations if included.
