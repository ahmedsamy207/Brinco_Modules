# Sound sensor module

UART firmware copied from `Py32f003_ProjectTemplate` for PY32F003F18P6TU.
It reports relative sound level and adjustable-comparator detection, as agreed
with the project owner. It does not report calibrated decibels or stream audio.
See the [module reference](../docs/modules/Sound_Sensor.md) and
[schematic](../Modules%20Schematics/Schematic_Sound_Sensor_UART.pdf).

## Hardware and clock

| Function | MCU GPIO / package pin | Configuration |
|---|---|---|
| A0 microphone/bias node | PA2 / 1 | ADC channel 2, analog input |
| D0 LM393 output | PA3 / 2 | Input, external R4 pull-up, active-edge EXTI |
| Module TX / main RX | PA4 / 3 | USART2 TX, AF9 |
| Module RX / main TX | PA5 / 4 | USART2 RX, AF9 |

The UART JST is 1 GND, 2 main RX/module TX, 3 main TX/module RX, 4 5 V.
The reverse-side cable intentionally reverses connector numbering at the hub.
The fitted 32 MHz external crystal directly clocks core/APB; flash latency 1.
HAL and CMSIS use HSE_VALUE=32000000; oscillator failure halts without HSI fallback.

LM393 U2.1 has A0 on its inverting input and the R6/R7 mid-supply divider on
its non-inverting input. The default interpretation is therefore active-low
D0 when A0 exceeds that reference. R8 is wired as an adjustable bias/feed
resistance from 5 V to A0, not a firmware-controlled threshold. The second
comparator drives the indicator. Confirm quiet/active polarity, bias adjustment,
input common-mode behavior and microphone sensitivity on the physical board.
Set SOUND_DETECT_ACTIVE_LOW=0 in sound_config.h if the required detection
polarity differs. R4 provides D0 pull-up; MCU internal pulls are disabled.

## Acquisition and interpretation

TIM3 TRGO triggers ADC at 4,000 samples/s: 32 MHz / 32 / 250. The ADC clock
is 8 MHz with 239.5-cycle sampling time, 12-bit conversion and channel 2 only.
This is a relative-level acquisition rate, not full-band audio recording;
the circuit's bandwidth/noise and aliasing remain hardware validation items.

DMA1 channel 1 runs circularly over 160 halfwords. Each half contains 80
samples (20 ms). The DMA half/full callback computes last, minimum, maximum,
rounded mean and peak-to-peak, then publishes one snapshot. The foreground
UART copies snapshots under a short IRQ mask. No ADC polling or UART work
occurs in acquisition callbacks. DMA continues during foreground UART replies.

Relative level is rounded `(maximum - minimum) * 1000 / 4095`, range 0–1000.
A constant DC bias produces zero variation. The value is not perceived loudness,
not calibrated SPL, and cannot be compared across boards without calibration.
The near-rail flag means at least one sample <=5 or >=4090; it does not prove
that the microphone/comparator is free from distortion when clear.

D0 active edges latch through EXTI, so an edge can be reported even if the
pulse has ended before its ISR runs. Active levels at window boundaries are
included and carried into the next window. Detection describes the completed
window and clears in subsequent inactive windows; it is not held until READ.
Polling slower than the window rate can miss past events. EXTI pending edges
can coalesce; this is not a pulse counter. No digital debounce is imposed,
because it could discard short sounds; adjust R8 and validate false triggers.

A 100 ms engineering settling guard is followed by a complete window; the
first valid result is normally at approximately 120 ms. Validate this guard
with the actual microphone. Results older than 100 ms are invalid. ADC/DMA
errors, simultaneous pending DMA half/full boundaries (buffer overrun), or a
stalled acquisition latch a fault. New triggers stop and a reset is required
to recover. No old measurement is presented as valid after a fault.

## UART protocol, version 1

115200 baud, 8 data bits, no parity, one stop bit. Commands are uppercase ASCII
terminated by LF; CRLF also works. Each reply ends CRLF. Send one command and
wait for its reply before sending the next. There are no unsolicited events.

| Command | Successful response | Fields |
|---|---|---|
| PING | PONG | Link check |
| INFO | INFO BRINCO SOUND 1 | Firmware protocol identity |
| READ | SOUND level detected clipped | 0–1000 relative level, 0/1 window detection, 0/1 near-rail flag |
| RAW | RAW last min max mean d0 | ADC counts 0–4095, final D0 voltage 0/1 |
| STATUS | STATUS valid fault sequence age_ms | 0/1 flags, uint32 window counter and age |

RAW values and D0 are from the last completed window, not a fresh conversion
initiated by the command. READ and RAW refer to the latest snapshot independently;
they can straddle a new window. STATUS remains available during startup/faults;
sequence zero means no window has been published. Sequence and millisecond ticks
wrap naturally at 32 bits. Settling completion remains latched across tick rollover.

Example: a window alternating ADC 2000/2100, inactive D0, gives:

```text
READ
SOUND 24 0 0
RAW
RAW 2100 2000 2100 2050 1
```

Errors: ERR NOT_READY during settling, ERR STALE when the cached result is too
old, ERR ACQUISITION on latched fault, ERR COMMAND for an unknown command,
ERR LINE for a non-printable/oversized line, ERR LINE_TIMEOUT for a partial
line idle >=500 ms, and ERR RX for UART errors or receive overflow. The line
limit is 31 characters and receive queue capacity is 127 bytes. After line/RX
errors, discard continues through the next LF to avoid executing a damaged
command suffix; send a fresh command after that delimiter. Replies use bounded
integer formatting with no heap/printf dependency. Missing/truncated replies
are detected by the host timeout; they are not retransmitted automatically.

## Build, test and board bring-up

Run `make all` in this folder. Output goes to Build; generated files are ignored.
Programming/debugging follows [SETUP_WINDOWS.md](SETUP_WINDOWS.md). The complete
standalone template, vendor libraries and debug configuration are retained.

Validated build: 9,476 bytes flash and 2,392 bytes linked RAM usage, including
the template's 512-byte heap and 1,024-byte minimum-stack reservations. Runtime
stack headroom still needs board validation. The vendor linker script emits
an RWX LOAD-segment warning; the build completes and produces ELF/BIN/HEX.

Install dependencies with `py -m pip install -r Tests/requirements.txt`, then
run `py Tests/run_tests.py` with ARM GCC on PATH. The tests compile the real
sound_meter.c and module_uart.c into ARM code and run it under Unicorn with
mocked time/UART and injected ADC windows. All 13 tests pass. They check settling, DC bias rejection,
statistics, rounding, rail thresholds, window detection fields, freshness,
tick rollover, sticky faults, commands, malformed lines and UART recovery.
They do not execute sound_sensor.c, the real ADC/DMA/EXTI driver or IRQ scheduling.

Hardware is still in manufacture. Verify oscillator/core frequency, 4 kHz trigger
and DMA windows, A0 bias/quiet variation, D0 polarity and short-pulse capture,
R8 adjustment, near-rail behavior, serial traffic under acquisition load,
fault/reset recovery and runtime stack use. Record board revision, microphone
part, stimulus and scope results before declaring hardware validation complete.
