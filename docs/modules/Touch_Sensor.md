# Touch sensor module

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose

Report capacitive touch state to a main/expansion UART port. A dedicated touch
controller handles the sensing electrode; the PY32 reads its digital output.

## Confirmed hardware

PY32F003F18P6TU, 5 V, 32 MHz crystal. Touch controller U2 is labelled `223`
in the schematic; confirm the exact part from the BOM before assuming its mode
truth table. OUT is connected to the MCU and an indicator LED. TOG and AHLB
are drawn connected to GND; KEY connects to the electrode network.

| Signal | GPIO | MCU package pin |
|---|---|---:|
| Touch OUT | PA2 | 1 |
| Module TX / main RX | PA4 | 3 |
| Module RX / main TX | PA5 | 4 |

Source: [schematic](../../Modules%20Schematics/Schematic_Touch_Sensor_UART.pdf).

## Firmware and protocol status

Not implemented. Start from the F003 template; proposed project name
`py32f003_Touch_Sensor_Module` (not created). Touch output polarity, reporting
style and UART command/payload format have not been agreed.

## Open decisions and next work

Confirm the exact controller and mode pins against its datasheet. Agree state
polling versus edge events, any software debounce/stability interval, startup
settling and held-touch behavior. Distinguish hardware toggle mode from
software event handling; do not infer mode from the abbreviated part label.

## Validation

No firmware build or hardware test. Later test startup, touch/release/hold,
rapid repeated touches, idle noise, actual polarity and UART reporting.
