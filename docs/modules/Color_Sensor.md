# Colour sensor module

Last reviewed: 2026-10-26. [All modules](README.md)

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

LED_EN drives a BSS138 low-side illumination switch; gate has a pull-up to
3.3 V. Firmware uses PA6 open-drain: release=on, sink=off, default on. SDA/SCL
use AF12 open-drain without internal pulls. Source:
[schematic](../../Modules%20Schematics/Schematic_Color_Sensor_UART.pdf).

**Hardware issue requiring confirmation/correction:** no separate I2C pull-ups
or level shifter appear in that PDF. The 5 V MCU requires input HIGH >=0.7*VCC
(3.5 V), while the sensor bus is limited to 3.6 V and normally pulled to 3.3 V.
A direct connection is not guaranteed; firmware cannot repair this mismatch.
Confirm PCB details or provide external pull-ups and compatible level shifting.
Do not enable the MCU's 5 V internal pulls on the sensor/gate nets.
Sources: [PY32 Table 5-19](../../PUYA%20datasheets/PY32F003_Datasheet_Rev1.7.pdf),
[VEML6040 datasheet](../../Module%20Datasheets/VEML6040_Vishay_84276.pdf).

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

Resolve the electrical issues above, then define optical calibration, reference
surfaces, distance and named-colour features if required. Raw counts are not
sRGB, colour names or calibrated surface reflectance. The driver verifies CONF
readback; the sensor provides no identity register used by this implementation.
Internal I2C address 0x10 is unrelated to host UART addressing.

## Validation

Build passes: 9,364 bytes flash, 1,952 bytes linked RAM including vendor heap/
stack reservations. All 15 ARM-emulated acquisition/bus-adapter/UART tests pass
with mocked HAL IO; actual I2C timing and electrical/optical behavior are not
simulated. No hardware validation has occurred. After electrical correction,
verify readback/data, illumination, dark/reference colours, saturation, exposure
timing, stack use and UART/error recovery on sensor disconnect or stuck lines.
