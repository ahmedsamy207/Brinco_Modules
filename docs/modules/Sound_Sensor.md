# Sound sensor module

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose

Provide microphone-related analog and comparator state information to the UART
hub. This is distinct from the I2S board's digital audio microphone. Whether
the product reports an event, relative level, waveform or another metric is open.

## Confirmed hardware

PY32F003F18P6TU, 5 V, 32 MHz crystal. MIC1 feeds an LM393 comparator circuit
with adjustable R8. Analog net A0 and digital net D0 reach the MCU.

| Signal | GPIO | MCU package pin |
|---|---|---:|
| A0 | PA2 | 1 |
| D0 | PA3 | 2 |
| Module TX / main RX | PA4 | 3 |
| Module RX / main TX | PA5 | 4 |

Source: [schematic](../../Modules%20Schematics/Schematic_Sound_Sensor_UART.pdf).
The schematic alone does not establish calibrated sound-pressure units or
the analog node's useful transfer function.

## Firmware and protocol status

Not implemented. Start from the F003 template; proposed project name
`py32f003_Sound_Sensor_Module` (not created). No sampling rate, filtering,
event polarity, UART commands or payload has been agreed.

## Open decisions and next work

Agree output semantics before selecting ADC rate and processing. Verify LM393
circuit behavior and D0 polarity. Define baseline removal, peak/RMS/windowing
only if needed by the agreed feature. Do not label an arbitrary ADC count as
decibels without a measurement/calibration model. Define adjustable-threshold
behavior, event persistence and no-signal/saturation reporting.

## Validation

No firmware build or hardware test. Later inspect A0/D0 on a scope, measure
quiet/noisy baselines and R8 effect, then test timing, false triggers,
saturation and serial response under the agreed processing load.
