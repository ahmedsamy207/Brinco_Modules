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
| OPA_A_OUT | PA2 and PA8 | Analog high impedance |
| OPA_A_INN | PA10 | Analog high impedance |
| OPA_B_OUT | PA4 and PA5 | Analog high impedance |
| OPA_B_INN | PA6 | Analog high impedance |

Connected sense/OPA pins remain analog inputs; the firmware never drives
physically joined MCU pins against one another. ADC/OPA monitoring and software
current regulation are **not implemented** in this first version.

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
| `SPEED 100` | Target full steps/s,1..500; default 50 | `OK` |
| `ACCEL 200` | Acceleration/deceleration full steps/s²,1..5000; default 100 | `OK` |
| `MOVE 20` | Move+20 full steps from current logical position | `OK` when accepted |
| `MOVE -20` | Move−20 full steps; signed nonzero count, magnitude<=1,000,000 | `OK` when accepted |
| `RUN 1` / `RUN -1` | Continuous rotation in selected direction | `OK` when accepted |
| `STOP` | Cancel target and decelerate to HOLD | `OK` |
| `RELEASE` | Immediately disable coils and cancel motion | `OK` |
| `ZERO` | Set logical position0 while IDLE/HOLD only; no movement/homing | `OK` |
| `CLEAR` | Clear fault while released and nFAULT high; never auto-restart | `OK` |

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
or fault pin still low; `ERR LINE` invalid/overlong line; `ERR LINE_TIMEOUT`
partial-line expiry; `ERR RX` lost/corrupted UART input. Invalid lines are
discarded through the next LF so suffixes cannot become motion commands.
UART errors/overflow release coils and latch fault4. Reply transmission is
bounded and is not retried automatically if it fails.

**Never automatically resend MOVE/RUN after a missing reply:** the command
may already have been accepted. This UART protocol has no transaction-ID cache.
Read STATUS and reconcile the application state. The expansion bridge protects
its own repeated WRITE packets but cannot make arbitrary application retries
of a new UART command safe.

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
- `motion_protocol.c/.h`: strict grammar and bounded heap-free status formatter.
- `module_uart.c/.h`: RX ring, line validation/recovery and bounded replies.
- `py32f040_it.c`: core exceptions and USART3 shared-vector adapter.

Motion/fault state is shared with IRQs using brief critical sections. nFAULT
EXTI has highest priority, TIM6 next, USART3 next. UART RX IRQs only enqueue
bytes; foreground parses commands. No UART waits or floating-point formatting
run in the motion IRQ. ADC/OPA inputs remain untouched analog connections.

From this project folder with Arm GNU tools and make on PATH:

```powershell
make -j4
python -m pip install -r Tests/requirements.txt
python Tests/run_tests.py
```

Use the usual standalone SWD workflow (`make flash`) with the motor/module
correctly powered. Build outputs are under `Build/py32f040_Stepper_Motor_Module.*`.
See SETUP_WINDOWS.md for probe/tool setup.

The target build links at 8644 bytes flash and 1968 bytes static RAM (stack is
additional). The 24 software tests ARM-compile the **actual motion, protocol, UART and motor
GPIO/timer code** with warnings as errors and execute them in Unicorn2.1.4.
Mocks provide HAL/register semantics and coil-output traces. Tests cover exact
counts and phase order both directions, wake delay, acceleration/deceleration,
continuous rotation/speed changes, release/hold, limits, busy commands, faults,
lease expiry/renewal, partial/malformed input, UART overflow and error recovery.
They do not validate the real oscillator, GPIO electrical behavior, interrupt
latency, motor torque/synchronism, current/temperature or cable connections.

Hardware validation remains pending: first verify disabled startup and all four
coil outputs without a motor; then nSLEEP wake and latched nFAULT shutdown;
check coil pairs/direction, measured current,20-step nominal revolution, loaded
speed/acceleration, STOP/release behavior, lease expiry and UART recovery. Measure
IRQ duration/jitter, motor/driver temperature and supply behavior before setting
production motion limits. No physical homing or position accuracy is claimed.
