# IR array module

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose

Line-following robot input. Classify each TCRT5000L output high/low and pack the
active sensor states plus sensor-count information into one UART data byte.
Use all five sensors or cut off outer S1/S5 and retain S2/S3/S4.

## Confirmed hardware

PY32F003F18P6TU at 5 V; firmware uses the fitted 32 MHz external crystal.
Sensor outputs are analog collectors with 10 kohm pull-ups; no count strap is shown.

| Physical sensor | GPIO | Package pin | ADC channel |
|---|---|---:|---:|
| S1 | PA5 | 4 | 5 |
| S2 | PA6 | 5 | 6 |
| S3 | PA7 | 6 | 7 |
| S4 | PA1 | 20 | 1 |
| S5 | PA0 | 19 | 0 |

Module TX PA2/pin 1, RX PA3/pin 2, USART1 AF1.
Source: [schematic](../../Modules%20Schematics/Schematic_IR_Array_UART.pdf).

## Firmware status and implemented protocol

Project: [py32f003_IR_Array_Module](../../py32f003_IR_Array_Module/README.md).
Its README and `User/Inc/ir_array_config.h` define the current behavior.

- Separate 5-sensor/default and 3-sensor `Build3` variants; no auto-detection.
- ADC calibration at startup; complete scans scheduled at best-effort 1 ms intervals.
- Provisional threshold 2048, hysteresis +/-64 counts; tune separately per sensor.
- USART1 115200, 8N1. `READ\n` returns exactly one binary byte, without CRLF.
- Bit 7: 1 = five sensors, 0 = three. Bit 6: fresh/valid. Bit 5: reserved zero.
- Five mode bits 0..4 = S1..S5; three mode bits 0..2 = S2..S4, bits 3/4 zero.
- Invalid data clears sensor bits but retains count. High means voltage high,
  not an unconditionally calibrated black/white label.
- `PING`/`INFO`/`RAW` diagnostics; protocol errors return the single invalid byte.

## Open decisions and next work

Tune optical thresholds at actual mounting height/surfaces. Confirm physical
left/right orientation; do not assume sensor numbering equals robot left/right.
Main-board steering and line-position estimation are outside this module's
current scope. Keep the count build matched to the full/cut board.

## Validation

Both 32 MHz HSE builds passed: five mode 7452 bytes flash, three mode 7460; both 1920 bytes
RAM at last check. Hardware pending: all patterns, cut-board continuity, ordering,
black/white response, threshold chatter, freshness/error bytes and polling load.
