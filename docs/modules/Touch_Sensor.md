# Touch sensor module

Last reviewed: 2026-10-24. [All modules](README.md)

## Purpose

Report capacitive touch state to a main/expansion UART port. A dedicated touch
controller handles the sensing electrode; the PY32 reads its digital output.

## Confirmed hardware

PY32F003F18P6TU, 5 V, fitted 32 MHz crystal used directly as SYSCLK.
The owner supplied the exact U2 datasheet: **XMXW 223, LCSC C1693666**, Rev 1.1,
stored in [Module Datasheets](../../Module%20Datasheets/223_C1693666.pdf).
OUT connects to the MCU and indicator LED. Grounded TOG/AHLB select direct,
active-high output. KEY connects to the sensing electrode network.

| Signal | GPIO | MCU package pin |
|---|---|---:|
| Touch OUT | PA2 | 1 |
| Module TX / main RX | PA4 | 3 |
| Module RX / main TX | PA5 | 4 |

Source: [schematic](../../Modules%20Schematics/Schematic_Touch_Sensor_UART.pdf).

## Firmware and protocol status

Implemented from the updated F003 template:
[py32f003_Touch_Sensor_Module](../../py32f003_Touch_Sensor_Module/README.md).
The project README is the current protocol and build reference.

- USART2, 115200 baud, 8N1; LF/CRLF ASCII commands and CRLF responses.
- `PING`, `INFO`, `READ`, `RAW`; READ replies `TOUCH 0` or `TOUCH 1`.
- 600 ms startup guard, 10 ms state qualification, best-effort 1 ms sampling.
- A service gap over 20 ms invalidates state and requires fresh qualification.
- Explicit startup/stale/parser/RX errors; no unsolicited events or toggle latch.
- Source comments, mock-HAL ARM execution tests and workspace entry included.

## Open decisions and next work

The supplied controller datasheet lists 400 ms startup settling, 80 ms response
and an **8-second maximum output activation in direct mode**. Software follows
the actual OUT signal; it cannot guarantee indefinite held-touch detection.
Confirm physical behavior during bring-up. The software guard/debounce defaults
are engineering choices, not guarantees about every board's power-up behavior.
Sensitivity is set by the hardware electrode/cover/R-C network, not an MCU ADC.
Define any future event/gesture features separately from the current state API.

## Validation

Build passed: 6004 bytes flash, 1856 bytes RAM. All nine ARM-emulated software
tests passed, including both polarity configurations. These tests do not
validate the electrical controller or oscillator. The build retains the
template's linker warning about an RWX LOAD segment.
No physical board has been flashed or tested. Bring-up must cover startup,
touch/release, rapid touches, long holds and the IC's 8-second reset behavior,
sensitivity, noise, connector mapping and UART/core clock timing.
