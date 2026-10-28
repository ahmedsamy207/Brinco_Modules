# Stepper Motor Module — NFP-15BY-A

Standalone PY32F040C1BT6 project copied from `Py32f040_ProjectTemplate`.
Target motor confirmed by the owner: **NFP-15BY-A**, 1.5 mm shaft, 10 ohm coils,
5 V, bipolar two-phase, 18 degrees per full step, 20 full steps/revolution.
The firmware supports relative moves, continuous rotation, speed/acceleration
changes, decelerated stop with holding torque, and immediate coil release.

Sources: [motor manufacturer](https://dcmotorpro.com/product/15mm-micro-dc-stepper-motor-11mm-type-model-nfp-15by),
[board schematic](../Modules%20Schematics/Schematic_Stepper_Motor_UART.pdf),
[AT8833 document](../Module%20Datasheets/AT8833CT_C575404.pdf),
and the repository PY32F040 datasheet. This project is configured for the A motor,
not the B motor's different winding resistance and wire colours.

## Hardware and clock

| Signal | GPIO / connection | Configuration |
|---|---|---|
| Module TX, schematic Main_RX | PB10 | USART3 AF4 |
| Module RX, schematic Main_TX | PB11 | USART3 AF4 |
| AIN1 / AIN2 | PB4 / PB5 | GPIO outputs |
| BIN1 / BIN2 | PB0 / PB1 | GPIO outputs |
| Driver nSLEEP / nFAULT | PB6 / PB7 | Output / active-low input with falling EXTI |
| AISEN | PA1 and PA9 | Analog high impedance |
| BISEN | PA3 and PA7 | Analog high impedance |
| OPA_A_OUT | PA8 output tied to PA2 | OPA1 output switch / ADC2 input |
| OPA_A_INN | PA10 | Analog high impedance |
| OPA_B_OUT | PA5 output tied to PA4 | OPA2 output switch / ADC4 input |
| OPA_B_INN | PA6 | Analog high impedance |

All connected sense/OPA pins use GPIO analog mode. OPA1 drives only PA8 and
OPA2 only PA5 through their analog output switches; tied PA2/PA4 are ADC inputs.
No digital GPIO outputs contend on these nets. Both amplifiers and current
monitoring are implemented; closed-loop software current regulation is not.

The fitted 24 MHz HSE feeds PLL x3 for 72 MHz core/APB, flash latency2.
`HSE_VALUE=24000000U` agrees in Makefile and HAL config. Crystal/PLL failure
halts with bridges disabled, without HSI fallback. The driver inputs and nSLEEP
are driven low before increasing the clock. PA13/14 remain available for SWD.

The board powers both MCU and driver from **5 V**; the driver's wider standalone
VM rating does not make this complete module a12 V board.

### Motor connector CN1, NFP-15BY-A

| CN1 pin | Driver output | Manufacturer wire |
|---|---|---|
| 1 | BOUT2 / B− | White |
| 2 | BOUT1 / B+ | Red |
| 3 | AOUT1 / A+ | Yellow |
| 4 | AOUT2 / A− | Black |
| 5,6 | Not connected | Leave unused |

Check actual motor lead colours/coil pairs against the supplied motor before
powering. Positive/negative command direction is a coil-sequence convention;
physical clockwise/counterclockwise depends on shaft viewpoint and wiring.
The module UART connector uses1 GND,2 module TX,3 module RX,4 5 V. Use the
specified reverse-side JST cable to the main/expansion hub's reversed numbering.

### Coil control and current

AT8833 accepts four bridge inputs, not STEP/DIR. Forward full-step sequence is
`A+B+ -> A−B+ -> A−B− -> A+B−`, with the reverse order for negative motion.
Each update changes all four inputs in one GPIO BSRR write. Full steps use two
energized phases. This version has no half-step, microstep or reduced-current
holding mode. RELEASE sets all inputs and nSLEEP low; STOP retains the final
full-step phase after decelerating.

R1/R2 are0.2 ohm sense resistors. The driver's nominal 200 mV current trip gives
approximately 1 A chopping threshold; its160..240 mV range implies roughly
0.8..1.2 A before resistor tolerances. This is not a programmed500 mA setting.
At5 V/10 ohm, winding current is approximately 500 mA per phase before bridge,
sense-resistor and supply drops; winding tolerance and temperature affect it.
The maker does not state a separate A-version current rating on the linked page.
Measure actual current and temperature under the intended load/duty cycle.

## UART commands

115200 baud, 8N1, uppercase ASCII, one command per LF or CRLF line. No extra
spaces, trailing arguments, embedded CR/NUL or binary command bytes. Maximum
line length63 printable characters, partial-line timeout500 ms. Send one
command and consume its reply before the next. No unsolicited events.

| Command | Behavior / bounds | Reply |
|---|---|---|
| `PING` | Link check and keepalive | `PONG` |
| `INFO` | Firmware/motor/step identification | `INFO BRINCO STEPPER 1 NFP-15BY-A FULL 20` |
| `STATUS` | Coherent motion snapshot and keepalive | See below |
| `CURRENT` | Sampled shunt current/diagnostics and keepalive | See current monitoring below |
| `ILIMIT 750` | Supervisory threshold,100..1000 mA per winding; IDLE only, default750 | `OK` |
| `SPEED 100` | Target full steps/s,1..500; default 50 | `OK` |
| `ACCEL 200` | Acceleration/deceleration full steps/s²,1..5000; default 100 | `OK` |
| `MOVE 20` | Move+20 full steps from current logical position | `OK` when accepted |
| `MOVE -20` | Move−20 full steps; signed nonzero count, magnitude<=1,000,000 | `OK` when accepted |
| `RUN 1` / `RUN -1` | Continuous rotation in selected direction | `OK` when accepted |
| `STOP` | Cancel target and decelerate to HOLD | `OK` |
| `RELEASE` | Immediately disable coils and cancel motion | `OK` |
| `ZERO` | Set logical position0 while IDLE/HOLD only; no movement/homing | `OK` |
| `CLEAR` | Clear released fault with nFAULT high and healthy current acquisition; see recovery below | `OK` |

All replies end CRLF. `OK` acknowledges acceptance, not move completion. Poll
STATUS for HOLD after a completed MOVE/STOP. SPEED and ACCEL can change during
motion. A new MOVE/RUN is rejected while WAKING/MOVING/RUNNING/STOPPING; there
is no motion queue or automatic reversal while moving. Stop first, then restart.
STOP during WAKING releases without energizing; STOP when already idle/holding
is a no-op. RELEASE retains any fault latch.

Example status:

```text
STATUS RUNNING POS 42 REM 0 VEL 100000 SPEED 100 ACCEL 200 FAULT 0
```

POS is signed full steps relative to startup/ZERO. REM is unsigned steps left
in a finite move; it is0 for continuous rotation, STOPPING and cancelled moves.
VEL is speed magnitude in **milli-steps/s** (100000 means100 full steps/s).
SPEED and ACCEL use the integer units above. Position counts commanded electrical
transitions, not measured shaft movement. At20 steps/revolution,100 steps/s
corresponds to300 RPM nominally; real achievable speed/torque must be measured.

States: IDLE (released), WAKING, MOVING, RUNNING, STOPPING, HOLD (energized),
FAULT (released, latched). Fault codes:0 none,1 driver nFAULT,2 host lease timeout,
3 signed 32-bit position limit,4 UART receive error/overflow. The fault pin does
not distinguish overcurrent from overtemperature. Hardware driver recovery
never resumes firmware motion automatically.

Errors: `ERR COMMAND` unknown verb; `ERR ARG` invalid number/range/grammar;
`ERR BUSY` operation incompatible with current state; `ERR FAULT` latched fault
or acquisition cannot be cleared; `ERR NOT_READY` startup current calibration
incomplete; `ERR LINE` invalid/overlong line; `ERR LINE_TIMEOUT`
partial-line expiry; `ERR RX` lost/corrupted UART input. Invalid lines are
discarded through the next LF so suffixes cannot become motion commands.
UART errors/overflow release coils and latch fault4. Reply transmission is
bounded and is not retried automatically if it fails.

**Never automatically resend MOVE/RUN after a missing reply:** the command
may already have been accepted. This UART protocol has no transaction-ID cache.
Read STATUS and reconcile the application state. The expansion bridge protects
its own repeated WRITE packets but cannot make arbitrary application retries
of a new UART command safe.

## Current monitoring and supervision

OPA1/OPA2 use the external15 kΩ /1 kΩ feedback networks, gain16. With0.2 Ω
shunts, nominal gain is3.2 V/A. ADC1 continuously scans **PA2/ADC2 (A), PA4/ADC4
(B), internal VREFINT** into a circular DMA buffer. The core clock remains
72 MHz from the24 MHz crystal. ADC clock is PCLK/8=9 MHz, with134.5-cycle
external samples and239.5-cycle internal reference acquisition (>20 μs).
Unequal acquisition times avoid an intentionally phase-locked scan at the
nominal driver chopping rate; real aliasing and waveform coverage must be
checked with a scope. The channels are sequential, not simultaneous.

Each DMA half contains128 triplets, approximately an8 ms observation window.
The firmware verifies DMA ownership before/after analysis and publishes only
a validated half. Two initial settling windows are followed by eight released
zero-offset windows. MOVE/RUN return `ERR NOT_READY` until calibration completes
(roughly80 ms after acquisition starts). Poll CURRENT until READY1 before motion.
No motor is energized during calibration. An implausible released baseline
(mean>300 mV or peak>500 mV) faults instead of hiding a stuck signal as zero.

VCCA is estimated each window from nominal1.200 V VREFINT. Average and peak
amplifier voltages are offset-corrected and converted to positive milliamps:
`mA ≈ max(output_mV − released_offset_mV,0) /3.2`. Internal reference tolerance,
OPA offset/output floor, shunt/gain tolerances and board layout affect accuracy.
Startup zeroing is a relative baseline correction, not a calibrated ammeter;
OPA clipping near zero can bias larger readings too. No precision-current claim.

Example reply, values in mA except VCCA in mV:

```text
CURRENT A 420 B 415 PEAKA 495 PEAKB 490 VCCA 5000 LIMIT 750 READY 1 FAULT 0
```

A/B are **sampled positive low-side shunt means over the latest window**;
PEAKA/PEAKB are the largest captured samples from that window. They are not
true winding-current RMS, signed phase current, guaranteed physical peaks or
an all-time maximum. Recirculation/chopping can hide current from the shunt,
and narrow switching pulses can be missed. A low reading does not prove no
coil current or a disconnected motor. Read READY/FAULT before trusting values.

`ILIMIT` changes the **fault threshold**, not the driven/current-regulated
setpoint. It is RAM-only, shared by both windings and can change only while
released in IDLE. Default750 mA leaves margin above the NFP-15BY-A's approximate
500 mA winding-current estimate. It is an initial engineering setting requiring
bench validation, not a certified motor thermal limit. The AT8833's shunt-set
hardware chopping/OCP still operates independently and is not reprogrammed.

An excessive-current window requires at least16 of128 samples above the
threshold in either winding. Three consecutive excessive windows release both
bridges and latch **fault5** (roughly24 ms after sustained evidence; a condition
starting partway through a window can take another window). Brief spikes do
not trip or accumulate across clean windows. Near-rail samples (ADC>=3800/4095)
with the same16-of128 /three-window rule instead latch **fault6**, because OPA/
ADC saturation invalidates the measurement. This is supervisory detection,
not cycle-by-cycle limiting or a guarantee against every waveform/fault.

DMA transfer errors, missed/coalesced boundaries, overwrite during analysis,
ADC overrun, invalid VREFINT/VCCA (outside4.2..5.5 V) or invalid raw codes also
latch fault6. Acquisition silence for800 motor ticks (40 ms) faults even while
released. A late result cannot undo that failure. The motor fault code preserves
the first cause; CURRENT's own fault field can show a later acquisition failure.

For fault5, release has already occurred: wait for three consecutive safe
windows (below the qualification criteria), then CLEAR with nFAULT high and
issue a fresh motion command. Fault6 requires correcting the cause and a module
reset/power cycle to restart/calibrate acquisition; CLEAR cannot bypass it.
There is no automatic motion restart or runtime automatic recalibration.

## Timing, stop behavior and position limits

TIM6 runs at 20 kHz:72 MHz/(71+1)/(49+1), a50 us scheduling tick. Acceleration
updates every 1 ms using Q16 fixed-point speed. A phase accumulator schedules
at most one full-step transition per tick; there is no backlog catch-up burst.
Finite moves use a square-root braking envelope and terminate at the exact
requested commanded step count. Step intervals and the endpoint have whole-step
and50 us resolution; this is not a continuous trajectory/servo controller.
The 500 steps/s cap is a software bound, not a measured loaded motor rating.

On wake, wait40 timer edges (at least 1.95 ms) before applying the stored phase,
exceeding the driver's1 ms specified wake delay. Initial energization aligns an
unknown rotor before the first counted step; that alignment can move the shaft.
There is no home switch, encoder, stall detection or absolute position feedback.
Startup/ZERO establishes only a logical origin. Releasing, faults, reset or loss
of synchronism can invalidate the relationship between count and physical angle.
After a fault, re-establish the application reference before relying on position.

### Host keepalive

While moving **or holding**, send a valid command such as `STATUS` or `PING`
**at least once per second**. The firmware releases coils and latches timeout
fault2 after 40,000 timer ticks (approximately 2 s) without a valid command.
Idle/released operation does not require a heartbeat. Invalid/rejected commands
do not renew it. For recovery, inspect STATUS, send CLEAR when the fault pin is
high, then issue an explicit new motion command. Motion never auto-resumes.
The timeout depends on the timer IRQ running; it is not an independent hardware
watchdog and does not prove shutdown if interrupts/CPU execution stop.

Typical host sequence (each line consumes its reply):

```text
ACCEL 200
SPEED 100
MOVE 20
STATUS                 <- poll every 100..500 ms until HOLD
MOVE -20
STATUS                 <- continue polling until HOLD
RUN 1
STATUS                 <- continue polling during continuous rotation
STOP
STATUS                 <- poll until HOLD
RELEASE
```

The comments/arrows above are explanatory and must not be sent over UART.
HOLD preserves full coil excitation and needs keepalive too; use RELEASE when
holding torque is not needed. All settings, counters and fault state are RAM
only and reset on power/reset. Restart begins released at default speed/accel.

## Source, build and verification

- `main.c`: disabled startup,24 MHz HSE/PLL, foreground UART service.
- `motion.c/.h`: timer-driven state machine, profiles, limits, faults and lease.
- `motor_io.c`: exact board pins, atomic coil writes, TIM6 and nFAULT IRQs.
- `current_adc.c`: OPA startup, ADC scan, DMA ownership/error handling.
- `current_monitor.c/.h`: startup baseline, supply compensation, telemetry and faults.
- `motion_protocol.c/.h`: strict grammar and bounded heap-free status formatter.
- `module_uart.c/.h`: RX ring, line validation/recovery and bounded replies.
- `py32f040_it.c`: core exceptions and USART3 shared-vector adapter.

Motion/fault state is shared with IRQs using brief critical sections. nFAULT
EXTI has highest priority, TIM6 next, USART3/ADC error next, DMA analysis last. UART RX IRQs only enqueue
bytes; foreground parses commands. No UART waits or floating-point formatting
run in the motion IRQ. The motion tick also checks acquisition freshness.
DMA analysis can be preempted; only brief publication/state updates mask IRQs.

From this project folder with Arm GNU tools and make on PATH:

```powershell
make -j4
python -m pip install -r Tests/requirements.txt
python Tests/run_tests.py
```

Use the usual standalone SWD workflow (`make flash`) with the motor/module
correctly powered. Build outputs are under `Build/py32f040_Stepper_Motor_Module.*`.
See SETUP_WINDOWS.md for probe/tool setup.

The target build links at 13764 bytes flash and 3696 bytes linked RAM (stack is
additional). The 38 software tests ARM-compile the **actual motion, protocol, UART, motor
GPIO/timer, current processing and OPA/ADC/DMA adapter code** with warnings as errors and execute them in Unicorn2.1.4.
Mocks provide HAL/register semantics and coil-output traces. Tests cover exact
counts and phase order both directions, wake delay, acceleration/deceleration,
continuous rotation/speed changes, release/hold, limits, busy commands, faults,
lease expiry/renewal, partial/malformed input, UART overflow and error recovery.
Current tests include startup gating/offset checks, channel scaling and supply
variation, chopped mean/peak, spike rejection, sustained high current, threshold
bounds, saturation, stale samples, DMA overwrite/errors and ADC error paths.
They do not validate the real oscillator, GPIO electrical behavior, interrupt
latency, motor torque/synchronism, current/temperature or cable connections.

Hardware validation remains pending: first verify disabled startup and all four
coil outputs without a motor; then nSLEEP wake and latched nFAULT shutdown;
validate both OPA outputs and ADC scaling with known currents and supply levels;
measure chopped/recirculation waveforms, offset drift and false-trip behavior;
exercise injected overcurrent/sensor faults, DMA timing and acquisition loss;
check coil pairs/direction, measured current,20-step nominal revolution, loaded
speed/acceleration, STOP/release behavior, lease expiry and UART recovery. Measure
IRQ duration/jitter, motor/driver temperature and supply behavior before setting
production motion limits. No physical homing or position accuracy is claimed.
