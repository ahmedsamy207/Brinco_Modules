# Stepper motor module

Last reviewed: 2026-10-28. [All modules](README.md)

## Purpose

Drive a stepper motor through a dual H-bridge and receive motion requests over
UART. The first firmware targets the NFP-15BY-A motor with full-step relative
positioning, continuous rotation, adjustable speed/acceleration and stop/release.

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

Implemented in [py32f040_Stepper_Motor_Module](../../py32f040_Stepper_Motor_Module/README.md),
created from the F040 template. Owner confirmed the NFP-15BY-A,1.5 mm shaft,
10 ohm,5 V bipolar two-phase motor. It has18 degree full steps (20/revolution).
USART3 uses PB10/PB11 AF4 at 115200 8N1. Core/APB runs72 MHz from 24 MHz HSE x3.

Supports signed relative MOVE, continuous RUN, adjustable SPEED/ACCEL, decelerated
STOP with holding torque, immediate RELEASE, logical ZERO, STATUS and explicit
fault CLEAR. Timer-driven full-step sequencing runs independently of UART replies.
No half-step/microstep, physical homing, encoder, stall detection, motion queue,
ADC/OPA monitoring or software current regulation is implemented.

Startup is released. Driver fault/UART loss/position overflow latch a released
fault; automatic driver retry never resumes motion. A valid host command at
least once per second is required while moving or holding: approximately 2 s
without keepalive releases coils and latches a timeout. Full protocol, units,
error/retry semantics and NFP-15BY-A connector wiring are in the project README.

## Current and motor assumptions

The 0.2 ohm shunts set a nominal 1 A hardware chopping threshold, not a 500 mA
software current setting. The 5 V/10 ohm winding estimate is approximately 500 mA
per phase before bridge/sense drops; actual current/temperature require measurement.
OPA-related pins remain analog high impedance, avoiding conflicting outputs on
joined nets. Full-step HOLD uses full excitation until RELEASE or host timeout.

## Validation and remaining work

Firmware builds;24 ARM-emulated tests cover motion, GPIO/timer, UART/protocol,
fault handling and timeout behavior. See project README for rerun instructions.
No physical board/motor validation yet. Verify wiring, disabled outputs, wake,
current/temperature, direction/counts, loaded speed/acceleration, stop/release,
IRQ timing and fault/recovery before selecting production limits. Position is
only an open-loop count; initial phase alignment and release can move the shaft.
