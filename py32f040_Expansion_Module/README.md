# UART Expansion Module

Standalone PY32F040C1BT6 firmware, copied from `Py32f040_ProjectTemplate`.
Flash **the same image into PY1 and PY2**, each through its own SWD header.
Each MCU is an active SPI-to-four-UART bridge and forwards later-hop requests.
External 24 MHz crystal, PLL x3, 72 MHz core/APB, flash latency2; no HSI fallback.

## Connections and addressing

| Interface | MCU pins | Configuration |
|---|---|---|
| Upstream SPI1 slave | PA4 NSS, PA5 SCK, PA6 MISO, PA7 MOSI | AF0, mode0, MSB first |
| Downstream SPI2 master | PB9 GPIO CS, PB8 SCK, PB6 MISO, PB7 MOSI | SCK/MISO/MOSI AF8, mode0, 281250 Hz |
| USART1 | PA9 TX, PA10 RX | AF1, 115200 8N1 |
| USART2 | PA2 TX, PA3 RX | AF1, 115200 8N1 |
| USART3 | PB10 TX, PB11 RX | AF4, 115200 8N1 |
| USART4 | PA0 TX, PA1 RX | AF4, 115200 8N1 |

TX/RX here are the hub MCU's perspective. Logical port numbers are USART1..4.

| Board MCU | Connector -> logical port |
|---|---|
| PY1 | H1 ->2, H2 ->3, H3 ->4, H4 ->1 |
| PY2 | H5 ->1, H6 ->4, H7 ->2, H8 ->3 |

Hop0 is the MCU connected directly to the host SPI; hop1 is its downstream MCU,
and so on. Each expansion adds two hops. When accessing EXP_IN directly,
PY1=hop0, PY2=hop1. Once the future main-board PY32 implements this protocol,
main PY32=hop0, first expansion PY1=hop1 and PY2=hop2. The I2S board's passive
SPI pass-through adds **no hop**. Each link has its own CS; this is an active
chain, not one shared-CS bus. Relative target hops0..7 are supported (8 MCUs,
32 UART ports maximum from a given entrypoint); physical cable limits are untested.

See [hardware reference](../Modules%20Schematics/Expansion_Reference.md) for
six-pin EXP_IN/OUT wiring. Reverse-side four-pin JST cables deliberately map
hub pins1..4 to module pins4..1; do not apply that rule to the six-pin headers.

## Bridge protocol1

This is the first implemented bridge protocol. ESP/main-board hub integration
is still pending. It preserves all UART bytes, including NUL and binary IR
readings, without interpreting sensor commands, responses or newlines.

Every SPI transaction has **exactly64 bytes**. Upstream clock <=300 kHz, mode0,
MSB first. Keep CS high >=5 ms between transactions, allow >=1 ms after CS
assertion before clocks, and finish a selected transaction within100 ms.
SPI2 generates72 MHz/256 =281250 Hz with the same timing contract. Partial,
overlong, malformed and CRC-failing input is discarded before queue operations.

All unspecified/reserved/padding bytes must be zero. Fields are unsigned.

| Bytes | Request `BH` | Reply `HB` |
|---|---|---|
| 0..1 | ASCII BH | ASCII HB |
| 2 | Version1 | Version1 |
| 3 | Operation | Echo operation |
| 4 | Relative target hop0..7 | Echo original target hop |
| 5 | Port1..4; INFO uses0 | Status code |
| 6 | Payload length0..48 | Payload length0..48 |
| 7 | Reserved0 | Reserved0 |
| 8..11 | Transaction ID, little-endian uint32 | Echo ID |
| 12..13 | Reserved0 | Reserved0 |
| 14..61 | Payload then zero padding | Payload then zero padding |
| 62..63 | CRC, high byte first | CRC, high byte first |

CRC-16/CCITT-FALSE covers bytes0..61: polynomial0x1021, initial0xFFFF,
no reflection, no final XOR. Standard `123456789` check value0x29B1.

| Operation | Request payload | Successful reply payload |
|---|---|---|
| 0 FETCH | All bytes3..61 zero (including ID/port/hop) | Current cached receipt |
| 1 INFO | Empty; port0 | 12 bytes described below |
| 2 WRITE | 1..48 UART bytes | One byte: accepted byte count |
| 3 READ | Empty | 0..48 oldest UART RX bytes, consumed once |
| 4 STATUS | Empty | RX queued uint16, TX queued uint16, flags uint16, all little-endian |
| 5 CLEAR | Empty | Empty; clears sticky UART flags only |

INFO payload: byte0 firmware major1, byte1 minor0, byte2 ports4, byte3 maximum
relative target7, byte4 maximum payload48, byte5 reserved0, bytes6..9 baud115200
little-endian, bytes10..11 saturated local SPI framing/error count little-endian.
The error count is local to the addressed MCU; it includes CRC/framing and ID misuse.

Status codes:0 OK,1 BUSY,2 INVALID,3 TX FULL,4 LINK FAILURE/outcome unknown.
UART flags: bit0 RX overflow (newest byte dropped), bit1 UART receive/parity/
framing/overrun or rearm error, bit2 failed UART TX start. CLEAR clears flags;
it does not clear queues or abort an in-flight byte. STATUS TX includes an
in-flight byte; WRITE OK means queued, not proof of module receipt/execution.
There is no UART ACK, parity or hardware flow control.

SPI replies are **pipelined**: a transaction returns the receipt prepared before
that transaction, not the result of its simultaneously received command. Send
one command once, then FETCH until command, hop and transaction ID match and
status is not BUSY. Boot receipt has operation0, ID0, BUSY. Allow at least
`(hop+1)*200 ms +300 ms` host deadline. Each forwarding MCU has a bounded
`(remaining_hop+1)*200 ms` deadline and100 ms physical SPI timeout.

Use one outstanding command for the whole chain. IDs start at1 and increase
strictly, without wrapping, for a powered session; persist/coordinate the counter
across host-client recreation. Identical last-ID requests return the cached result;
conflicting same-ID, older-ID and ID0 requests are ignored. A different request
while forwarding is pending is ignored. Host misuse never replaces a pending
receipt. A READ duplicate returns its cached bytes without consuming more RX.

Only the most recent receipt is retained. Forwarders transmit the command once,
then FETCH only. **Never automatically retry a UART WRITE or consuming READ
following a timeout/link failure**: it may already have executed downstream.
A reset loses the volatile receipt and queues. Reconcile uncertain outcomes
explicitly before starting another operation; a receipt cannot guarantee exactly
once delivery across power loss/reset. The reference client faults on uncertain
outcomes and refuses further commands until the caller reconciles the session.

## Host integration

[Host/expansion_host.py](Host/expansion_host.py) is a dependency-free reference
codec/client, with injected SPI `exchange`, monotonic clock and wait functions.
An exchange adapter must implement the physical CS/clock timing above; it must
return exactly64 received bytes. It must raise OSError on physical failure.
The helper sends each command once, validates replies and polls safely.

```python
from expansion_host import HubClient
hub = HubClient(exchange_spi64, start_id=1)
info = hub.command(1, hop=0)                 # addressed MCU identity
hub.command(2, hop=1, port=2, data=b"PING\n")
received = hub.command(3, hop=1, port=2)     # raw bytes, may be empty/partial
```

Build discovery by issuing INFO to successive hops until the first failed link;
this probes reachable MCUs, not sensor presence/type, board identity or whether
an absent board versus a broken cable caused the failure. Stop and reconcile
that fault before resuming ordinary commands. No automatic firmware enumeration
or configurable UART baud is implemented. An ESP C/C++ adapter/main PY32
application must still be written; this project does not replace those firmwares.

## Queues, timing and source guide

Each port has256-byte RX and TX rings. WRITE admission is all-or-nothing; a
FULL result leaves the queue unchanged. RX overflow drops newest and sets a
sticky flag; host must inspect flags to detect a stream discontinuity. READ may
split a response across calls. For text sensors, host assembles lines; for IR it
preserves the raw byte. Application response timeouts belong to the host.

UART reception/transmission and both SPI byte paths use bounded interrupt work.
Foreground routes envelopes and services queues; no UART commands execute
inside SPI interrupts. SPI1 has highest IRQ priority, SPI2/NSS next, UARTs next.
Both SPI peripherals are reset/rearmed between transactions; immutable upstream
reply snapshots are refreshed while CS is high when a routed response arrives.
USART3/4 share one IRQ. Rings use monotonic16-bit indices and bounded occupancy.

SPI capacity and hop latency limit aggregate throughput: four UARTs each at full
115200 baud exceed useful bridge SPI bandwidth, especially across several hops.
A full256-byte RX queue at115200 baud represents about22 ms. Poll/drain according
to actual traffic and check overflow flags; continuous lossless audio/high-rate
streaming is outside this version's guarantee. Maximum viable cable length,
clock, cascade length and loaded throughput require physical characterization.

- `main.c`: startup, crystal clock and cooperative service loop.
- `hub.c` / `hub.h`: validation, routing, receipt cache and platform interface.
- `ports.c`: four UARTs, rings, errors and HAL callbacks.
- `spi_transport.c`: upstream SPI byte/CS handling and frame boundaries.
- `downstream.c`: nonblocking SPI2 master with finite deadlines.
- `py32f040_it.c`: exception and peripheral vectors.

## Build, tests and physical validation

From this folder, with Arm GNU tools and make on PATH:

```powershell
make -j4
python -m pip install -r Tests/requirements.txt
python Tests/run_tests.py
```

Flash `Build/py32f040_Expansion_Module.hex` into **both** MCUs using the normal
project SWD workflow (`make flash` for the connected target); power the board
correctly and program each SWD connector separately. See SETUP_WINDOWS.md.

**25 tests pass** in the current software baseline. The target build uses
9644 bytes flash and 4680 bytes static RAM; stack use is additional.
Physical hardware validation remains pending.

The suite ARM-compiles the actual routing, UART rings, upstream SPI and downstream
SPI drivers with `-Wall -Wextra -Werror`, then executes them with Unicorn2.1.4.
Separate CPU/RAM instances clock real generated frames between nodes, including
all8 supported hops. Tests cover binary isolation, duplicate/old-ID protection,
CRC and malformed frames, partial/overlong/stuck CS, ring wrap/full/overflow,
UART errors, link timeout/recovery, tick wrap, asynchronous reply refresh and
host timeout behavior. The model does not emulate actual peripheral FIFO timing,
GPIO electrical behavior, interrupt cycle budgets, crystals, wiring or real sensors.

Hardware checklist: verify crystal/core/UART baud; SPI mode/byte alignment and
NSS-driven MISO release; every connector/pin including inter-MCU link; local
and cascaded sensor commands; repeated reset/disconnect/recovery; overflow under
loaded traffic; ISR timing and cable signal integrity. Hardware remains pending.
