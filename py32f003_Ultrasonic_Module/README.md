# Ultrasonic module (UART hardware)

Created by copying `Py32f003_ProjectTemplate`. Target: PY32F003F18P6TU.
The legacy `Py32f003_Ultrasonic` folder contains the old I2C implementation.

## Implementation status

The distance-measurement driver and an initial UART command interface are
implemented. The main loop samples continuously. This protocol is newly defined
for this module because the main board firmware does not exist yet.

## Hardware

Source: `Modules Schematics/Schematic_Ultrasonic_UART.pdf`, revision 1.0.

| Function | MCU pin | Package pin | Configuration |
|---|---|---:|---|
| Trigger | PB6 | 13 | GPIO output, idle low |
| Echo rising | PA3 | 2 | TIM1_CH1, AF13 |
| Echo falling | PA1 | 20 | TIM1_CH4, AF13 |
| Module TX / main board RX | PA4 | 3 | USART2_TX, AF9 |
| Module RX / main board TX | PA5 | 4 | USART2_RX, AF9 |
| SWDIO / SWCLK | PA13 / PA14 | 10 / 11 | Debug port |

Both echo inputs are physically connected to the same sensor echo signal.
The board uses a 5 V supply and its fitted 32 MHz external crystal as SYSCLK.
TIM1's prescaler is calculated from the peripheral clock (32 / 1 MHz - 1 = 31),
preserving the 1 us measurement tick. Flash uses one wait state. Crystal startup
failure halts in APP_ErrorHandler(); there is no silent internal-clock fallback.

## Measurement behavior

- 10 us trigger pulse, with 60 ms between measurement starts.
- TIM1 runs freely at 1 MHz with a 16-bit counter.
- Unsigned subtraction handles a counter rollover between echo edges.
- A 30 ms deadline handles absent echoes; overcapture, repeated rising edges,
  zero-length pulses, and a stuck-high echo report an invalid result.
- Distance is an unsmoothed integer in millimetres, using 343 m/s sound speed.
- Check the reading status before using its distance. Failed readings clear
  the distance; timestamps identify when the last measurement completed.
- Interrupts are masked for the short 10 us trigger pulse. Capture processing
  runs in the timer interrupt; the main loop handles scheduling and conversion.

## UART protocol version 1

115200 baud, 8 data bits, no parity, 1 stop bit, no flow control. Commands are
case-sensitive ASCII terminated by LF (`\n`); CRLF is also accepted. Replies
end with CRLF. There is no unsolicited streaming or startup banner. Send one
command and wait for its complete reply before sending another (100 ms host
response timeout is sufficient for normal operation).

| Request | Reply |
|---|---|
| `PING\n` | `PONG\r\n` |
| `INFO\n` | `INFO BRINCO ULTRASONIC 1\r\n` |
| `DIST\n` | `DIST <distance_mm> <age_ms>\r\n` |

`DIST` reads the latest completed measurement; it does not trigger or wait for
a new one. For example, `DIST 1234 12` means 1234 mm measured 12 ms ago. There
is no filtering, temperature compensation, or asserted sensor-specific range
limit. Precision in millimetres is the wire representation, not an accuracy claim.

Error replies:

| Reply | Meaning |
|---|---|
| `ERR NOT_READY` | No measurement has completed since startup |
| `ERR NO_ECHO` | The latest measurement exceeded the echo deadline |
| `ERR INVALID_ECHO` | Stuck-high input, overcapture, or invalid pulse sequence |
| `ERR STALE` | The latest measurement is more than 200 ms old |
| `ERR COMMAND` | Unknown command |
| `ERR LINE` | Non-ASCII/control character or more than 31 command characters |
| `ERR LINE_TIMEOUT` | An incomplete command was idle for at least 500 ms |
| `ERR RX` | Receive overflow or UART framing/parity/overrun error |

Blank lines are ignored. After a line or receive error, send an LF to
resynchronize before issuing a new command. The module discards bytes up to
that delimiter so a partial corrupted command cannot be executed.

This is an initial interface for the future main board firmware, not a legacy
protocol. It has no packet CRC; add framing/checksums if system requirements
later call for stronger link integrity.

## Build and validation

Run `make all` in this folder. Outputs use the folder name automatically.
Hardware validation is still required: scope the trigger, verify echo capture
against known distances, and test no echo and stuck-high echo conditions.
The measurement timing and useful range must also be checked against the
actual ultrasonic sensor specification.
Check `PING`, `INFO`, valid and unknown commands, startup/no-echo `DIST`
responses, and recovery after overlong/partial lines using a UART adapter
with signal levels appropriate for the board. Main board electrical
compatibility remains to be checked against its schematic.
