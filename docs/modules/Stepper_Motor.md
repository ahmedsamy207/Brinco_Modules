# Stepper motor module

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose

Drive a stepper motor through a dual H-bridge and receive motion requests over
UART. Speed, position, step mode, acceleration and current-control requirements
are not yet specified; this page records wiring, not an implemented motion API.

## Confirmed hardware

PY32F040C1BT6 on 5 V with a 24 MHz crystal. Driver U2 is labelled
`AT8833CT_C575404`; use that exact part/BOM, not an assumed substitute.
AISEN/BISEN have 0.2 ohm sense resistors. Feedback resistor networks connect
to the PY32 OPA-related nets. CN1 pins 1/2/3/4 are BOUT2/BOUT1/AOUT1/AOUT2;
pins 5/6 are marked unconnected on the schematic.

| Signal | MCU GPIO |
|---|---|
| Module TX / Main_RX | PB10 |
| Module RX / Main_TX | PB11 |
| AIN1 / AIN2 | PB4 / PB5 |
| BIN1 / BIN2 | PB0 / PB1 |
| Motor_nSLEEP / Motor_nFAULT | PB6 / PB7 |
| AISEN | PA1 and PA9 |
| BISEN | PA3 and PA7 |
| OPA_A_OUT | PA2 and PA8 |
| OPA_A_INN | PA10 |
| OPA_B_OUT | PA4 and PA5 |
| OPA_B_INN | PA6 |

Repeated nets are physical connections; do not configure two connected pins
as conflicting outputs. Source:
[schematic](../../Modules%20Schematics/Schematic_Stepper_Motor_UART.pdf).

## Firmware and protocol status

Not implemented. Start from the F040 template; proposed project name
`py32f040_Stepper_Motor_Module` (not created). PB10/PB11 correspond to USART3;
register configuration and alternate functions need implementation review.
No motor commands or wire-format units have been agreed.

## Open decisions and next work

Confirm motor type, rated coil current, supply, coil mapping, step modes and
desired motion features. Read the exact driver and MCU OPA/timer datasheets.
Define startup/sleep behavior, drive sequence, current sensing/control,
acceleration limits, stop behavior and fault recovery. Establish a disabled
output state before enabling motion. Decide how a host learns busy/completed/
fault state and how interrupted commands are handled.

## Validation

No firmware build or hardware test. First check inactive outputs and fault/
sleep states; then verify coil sequencing, current feedback and timing using
the intended motor. Test direction, speed, stopping and fault recovery.
