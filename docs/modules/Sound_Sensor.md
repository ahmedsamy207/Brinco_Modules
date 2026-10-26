# Sound sensor module

Last reviewed: 2026-10-26. [All modules](README.md)

## Purpose

Provide microphone-related analog and comparator state information to the UART
hub. This is distinct from the I2S board's digital audio microphone. The owner
selected relative sound level plus adjustable-threshold detection. Decibels and
audio streaming are outside this implementation.

## Confirmed hardware

PY32F003F18P6TU, 5 V, 32 MHz crystal. MIC1 feeds an LM393 comparator circuit
with adjustable R8. Analog net A0 and digital net D0 reach the MCU. A0 is
U2.1's inverting input; its non-inverting reference is the R6/R7 mid-supply
divider. Default detection is active-low. R8 adjusts microphone bias/feed
resistance from 5 V to A0; verify its usable sensitivity range on hardware.

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

Implemented in [py32f003_Sound_Sensor_Module](../../py32f003_Sound_Sensor_Module/README.md),
copied from the F003 template. Uses fitted 32 MHz HSE directly, TIM3-triggered
4 ksample/s ADC and circular DMA, with one 80-sample/20 ms result per window.
Level is peak-to-peak scaled to 0–1000, removing DC bias from amplitude.
EXTI latches D0 active edges and boundary levels into a window detection flag;
near-ADC-rail samples set a separate flag. Polarity is configurable.
Startup settling, freshness checks and sticky acquisition faults prevent use
of invalid readings. USART2 is 115200 8N1 with PING, INFO, READ, RAW and STATUS.
See the project README for exact fields, timings, recovery and test commands.

## Open decisions and next work

Verify LM393 circuit behavior, common-mode range, quiet/active D0 polarity and
R8 sensitivity adjustment. The current statistic is peak-to-peak, not RMS or dB.
Constant quiet ADC values and a disconnected microphone cannot be reliably
distinguished in software alone; no microphone-presence claim is made.
Detection covers only the latest window, not all events since the last READ.

## Validation

Build passes and 13 ARM-emulated statistics/UART tests pass. The tests inject
sample windows and mock HAL IO; they do not execute the ADC/DMA/EXTI driver or
establish hardware timing. No physical-board validation has occurred. Inspect
A0/D0 on a scope, verify 4 kHz/20 ms timing, quiet/noisy baselines, R8 effect,
false triggers, short pulses, rail flags, concurrent UART load and fault/reset
recovery. Record real measurements and runtime stack headroom when boards arrive.
