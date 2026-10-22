# Ultrasonic module

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose

Measure distance and supply the latest result to the main/expansion UART hub.
The old I2C project is historical and is not the current module protocol.

## Confirmed hardware

PY32F003F18P6TU, 5 V, 32 MHz external crystal shown. Current firmware uses
24 MHz internal HSI instead. PB6/package pin 13 drives TRIG. ECHO connects
to both PA3/pin 2 and PA1/pin 20 for rising/falling TIM1 capture. Module TX
is PA4/pin 3; RX is PA5/pin 4, USART2 AF9.

Source: [schematic](../../Modules%20Schematics/Schematic_Ultrasonic_UART.pdf).

## Firmware status and implemented protocol

Project: [py32f003_Ultrasonic_Module](../../py32f003_Ultrasonic_Module/README.md).
Its README is the full protocol/build reference; application code is in `User/`.

- Builds successfully; code is commented. No physical-board validation yet.
- TIM1 at 1 MHz, 10 us trigger, 60 ms trigger interval, 30 ms echo deadline.
- Handles timer rollover, missing echoes, invalid pulse sequences and stuck-high input.
- Returns unsmoothed integer millimetres using 343 m/s sound speed.
- USART2 at 115200, 8N1. `PING`, `INFO`, `DIST` are ASCII LF/CRLF commands.
- `DIST` returns cached `DIST <mm> <age_ms>` or an explicit ASCII error, with CRLF.

## Open decisions and next work

Confirm actual ultrasonic transducer/module specification, supported range and
measurement interval. Decide whether later requirements call for filtering,
temperature compensation or a system-wide protocol revision.

## Validation

Build passed (7104 bytes flash, 1928 bytes RAM at last check). Hardware pending:
scope trigger/echo and UART, compare known distances, test absent/stuck echo,
check rollover and verify module connector orientation. Precision in millimetres
does not establish physical measurement accuracy.
