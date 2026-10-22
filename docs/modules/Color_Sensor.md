# Colour sensor module

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose

Read a VEML6040 colour sensor and report results through the module UART.
The board also provides controlled illumination. Exact reported quantities
and colour-classification features are not yet agreed.

## Confirmed hardware

PY32F003F18P6TU powered from 5 V, 32 MHz crystal shown. GR6206 regulator
provides 3.3 V for VEML6040A3OG and illumination. Schematic labels sensor
I2C address as `0x10`; this is an internal sensor bus, not the host transport.

| Signal | GPIO | MCU package pin |
|---|---|---:|
| Internal SDA | PA2 | 1 |
| Internal SCL | PA3 | 2 |
| Module TX / main RX | PA4 | 3 |
| Module RX / main TX | PA5 | 4 |
| LED_EN | PA6 | 5 |

LED_EN drives a BSS138 low-side illumination switch; gate has a pull-up to
3.3 V. Source: [schematic](../../Modules%20Schematics/Schematic_Color_Sensor_UART.pdf).

## Firmware and protocol status

Not implemented; no UART commands, sampling interval, units or response frame
have been defined. Start from the F003 template. Proposed project name:
`py32f003_Color_Sensor_Module` (not created).

## Open decisions and next work

Obtain the exact sensor datasheet and implement configuration/integration-time
handling and register reads. Define raw versus processed colour output,
illumination control/default, calibration and saturation/error reporting.
Verify internal I2C pull-ups and 3.3 V logic compatibility against the actual
board/BOM; the drawing does not show separate SDA/SCL pull-up resistors.
Choose the host UART protocol deliberately; do not treat the internal I2C
address as a module address.

## Validation

No firmware build or hardware test. Later verify sensor identity/data reads,
illumination control, reference colours under defined light, dark offset,
saturation, integration timing and UART behavior on sensor disconnect.
