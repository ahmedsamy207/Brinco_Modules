# Touch sensor module

PY32F003F18P6TU firmware copied from `Py32f003_ProjectTemplate`.
Last reviewed: 2026-10-24. Hardware is still in manufacturing.

## Hardware and clock

Source: [touch schematic](../Modules%20Schematics/Schematic_Touch_Sensor_UART.pdf).
The controller is **XMXW 223, LCSC C1693666**, confirmed using the project
owner's [datasheet](../Module%20Datasheets/223_C1693666.pdf), Revision 1.1.

| Function | GPIO | MCU package pin | Configuration |
|---|---|---:|---|
| Touch-controller OUT | PA2 | 1 | Digital input, no internal pull |
| Module TX / main RX | PA4 | 3 | USART2_TX, AF9 |
| Module RX / main TX | PA5 | 4 | USART2_RX, AF9 |
| Crystal input/output | PF0 / PF1 | 16 / 17 | 32 MHz HSE |
| SWDIO / SWCLK | PA13 / PA14 | 10 / 11 | Debug port retained |

The MCU uses the fitted **32 MHz crystal** directly: SYSCLK/HCLK/PCLK are
32 MHz, flash has one wait state, and HSE_VALUE is defined consistently for
HAL and CMSIS. Startup checks SystemCoreClock; HSE failure halts rather than
silently substituting the internal oscillator. The board operates from 5 V.

The mode pins TOG and AHLB are grounded. In the supplied datasheet this
selects direct output and active-high touch: OUT is low when inactive and
high while detection is active. The indicator LED is driven by OUT hardware;
the MCU does not drive it or the sensing electrode. Module connector pins
1/2/3/4 are GND/module TX/module RX/5 V; use the specified reverse-side JST
cable to the hub.

## Controller behavior and firmware timing

The supplied datasheet lists 400 ms startup settling, 80 ms response time,
and an **8-second maximum active output in direct mode** (pages 4-5).
After that limit the controller resets its detection/output state. Firmware
reports the actual controller output, so indefinite held-touch detection is
not guaranteed. It does not latch a press to hide that hardware release.
The datasheet values are not all specified as worst-case guarantees.

The MCU adds:

- A conservative 600 ms startup guard, followed by 10 ms of observed stability.
  `READ` reports not ready until qualification succeeds (normally around 610 ms).
- Best-effort 1 ms foreground sampling; both touch and release need 10 ms
  stability before changing the logical state. Short observed glitches do not
  change a previously qualified state.
- A 20 ms freshness limit. Longer service gaps invalidate the state and require
  new qualification instead of assuming the unobserved interval was stable.
- Wrap-safe tick arithmetic; completed startup stays completed after tick wrap.

These values are in `User/Inc/touch_sensor_config.h`. `TOUCH_ACTIVE_HIGH=1`
matches this board; polarity can be changed for another board revision.
Keep the main loop responsive. Debouncing cannot observe transitions occurring
entirely between samples, and host polling can miss a short qualified activation.
There is no event queue, event stream, long-press classification or software toggle.
Validity describes observation freshness/qualification, not chip-presence detection.
The MCU cannot diagnose a missing/damaged controller solely from a fixed input level.

## UART protocol version 1

115200 baud, 8 data bits, no parity, one stop bit, no flow control. Commands
are case-sensitive ASCII ending with LF (`\n`); CRLF is also accepted. Replies
are ASCII ending with CRLF. No startup banner or unsolicited stream is sent.
Send one command and wait for its response before sending another.

| Request | Response |
|---|---|
| `PING\n` | `PONG\r\n` |
| `INFO\n` | `INFO BRINCO TOUCH 1\r\n` |
| `READ\n` | `TOUCH 0\r\n` (inactive) or `TOUCH 1\r\n` (active) |
| `RAW\n` | `RAW 0\r\n` or `RAW 1\r\n`: latest sampled voltage, no debounce/inversion |

`READ` uses the latest qualified state. `RAW` is available during the startup
guard for diagnosis and is not a substitute for `READ` readiness. Both reject
stale snapshots. A host response timeout of 100 ms is suitable for the normal
foreground service, but does not eliminate the controller's own response delay.

| Error | Meaning |
|---|---|
| `ERR NOT_READY` | Startup guard or fresh input qualification still in progress |
| `ERR STALE` | No input observation within the freshness limit |
| `ERR COMMAND` | Unrecognized command |
| `ERR LINE` | Control/non-ASCII byte or more than 31 printable command characters |
| `ERR LINE_TIMEOUT` | Partial command idle in foreground processing for 500 ms |
| `ERR RX` | UART receive error, receive re-arm failure, or software queue overflow |

Errors also end in CRLF. Blank lines are ignored. The RX queue has 127 usable
bytes. After LINE, LINE_TIMEOUT or RX errors, discard through the next LF before
starting a new command. Send an extra LF to resynchronize, then retry. No CRC is
included in this initial line protocol. Failed transmissions are not retried
automatically; the host must time out and resynchronize/retry.

## Build, tests, and code

Open this folder in VS Code and use Build/F5, or run `make all`. The template
derives output names automatically: `Build/py32f003_Touch_Sensor_Module.elf`,
`.bin`, `.hex`, `.map` and `.lst`. Generated Build/Temp files are ignored.

- `User/Src/touch_sensor.c`: GPIO sampling, startup, qualification and freshness.
- `User/Src/module_uart.c`: interrupt RX queue, foreground commands and recovery.
- `User/Src/main.c`: external-crystal clock and cooperative service loop.
- `User/Src/py32f0xx_it.c`: SysTick and USART2 interrupt adapters.
- `Tests/`: actual ARM-compiled driver/parser tests with a mocked HAL in Unicorn.

Run software tests with Arm GNU tools on PATH and Python:

```powershell
py -m pip install -r Tests/requirements.txt
py Tests/run_tests.py
```

The tests exercise startup, both polarities, touch/release, glitch filtering,
stale recovery, tick rollover, long input hold/hardware-release reporting,
commands, partial lines, malformed lines, overflow and UART error recovery.
They do not model the real controller, GPIO electrical behavior, RCC/USART
registers, oscillator startup or interrupt timing. They require no hardware probe.

## Hardware validation still required

Current validation: production build passed (6004 bytes flash, 1856 bytes RAM)
and all nine emulated software tests passed on 2026-10-24. The existing template
linker warning about an RWX LOAD segment remains.

Verify crystal/core clock and UART timing, idle/active polarity, startup and
rapid/repeated/held touches. Check the physical 8-second behavior, recovery,
the cover/electrode sensitivity, noise and shared-system load. The MCU's 10 ms
qualification cannot improve the sensing IC's intrinsic response time.
No physical board has been flashed or tested yet.
