# Colour sensor module

Last reviewed: 2026-10-27. [All modules](README.md)

## Purpose

Read a VEML6040 colour sensor and report results through the module UART.
The board also provides controlled illumination. Version 1 reports raw RGBW,
near-saturation flags and status, with configurable integration and illumination.
Named-colour classification and calibration remain future work.

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

The [corrected schematic](../../Modules%20Schematics/Schematic_Color_Sensor_UART.pdf)
was updated locally by the owner and reviewed on 2026-10-27. It includes Q1/Q3 BSS138
level shifters: gates to 3.3 V, sources to sensor-side SDA_3.3V/SCL_3.3V,
drains to MCU-side SDA_5V/SCL_5V. Sensor U3 uses the 3.3 V nets; U1 PA2/PA3
use the 5 V nets. R3/R4 pull the sensor bus to 3.3 V and R5/R6 pull the MCU
bus to 5 V. All four are now 4.7 kohm. The two sides are not directly tied.
This topology addresses the earlier missing pull-ups and logic-level mismatch.

Q2 is now AO3400A: drain to LED1 cathode, source to GND, gate to PA6/LED_EN
with R2=10 kohm to 3.3 V. U2=68 ohm is the LED series resistor. PA6 remains
open-drain: release=on, sink=off, default on. Check actual LED current against
its part/forward voltage. Verify physical MOSFET pin mappings against exact BOM
parts; the PDF symbols do not establish PCB footprint correctness.

R3-R6 now implement the recommended 4.7 kohm pull-ups. R2 remains 10 kohm
and the LED series resistor remains 68 ohm. Standard-mode rise time must still
be verified <=1 us on both sides of the translators; resistor values alone
are not proof of physical timing. Reference:
[TI pull-up calculation note](https://www.ti.com/lit/an/slva689/slva689.pdf).

The earlier missing-pull-up/level-shifter version and intermediate 10 kohm
version are retained in Git history. The corrected file is now the canonical
Schematic_Color_Sensor_UART.pdf; the duplicate _New.pdf has been removed by
the owner. Physical rework/new-board completion remains unconfirmed.
MCU pins, 32 MHz crystal and UART mapping are unchanged; no firmware behavior
change is needed. Retain external pull-ups and disabled internal pulls.

## Firmware and protocol status

Implemented in [py32f003_Color_Sensor_Module](../../py32f003_Color_Sensor_Module/README.md),
copied from the F003 template, using fitted 32 MHz HSE. USART2 is 115200 8N1;
PING, INFO, READ/RAW, STATUS, LED 0/1 and IT commands are documented there.
The internal bus is 100 kHz I2C at seven-bit address 0x10. Active-force mode
acquires one exposure before reading all four words; partial failures never
publish mixed readings. Default IT is 160 ms, with supported settings 40–1280 ms.
An engineering conversion-time margin and settling guard need real-board checks.
Failures invalidate data and trigger reset/reconfiguration retries after 500 ms.

## Open decisions and next work

Validate the corrected hardware and pull-up timing, then define optical calibration, reference
surfaces, distance and named-colour features if required. Raw counts are not
sRGB, colour names or calibrated surface reflectance. The driver verifies CONF
readback; the sensor provides no identity register used by this implementation.
Internal I2C address 0x10 is unrelated to host UART addressing.

## Validation

Build passes: 9,364 bytes flash, 1,952 bytes linked RAM including vendor heap/
stack reservations. All 15 ARM-emulated acquisition/bus-adapter/UART tests pass
with mocked HAL IO; actual I2C timing and electrical/optical behavior are not
simulated. No hardware validation has occurred. On the corrected physical board,
verify readback/data, illumination, dark/reference colours, saturation, exposure
timing, stack use and UART/error recovery on sensor disconnect or stuck lines.
