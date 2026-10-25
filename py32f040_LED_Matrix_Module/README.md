# LED matrix firmware

Standalone copy of `Py32f040_ProjectTemplate`, for PY32F040C1BT6 and 48
XINGLIGHT XL-5050RGBC-WS2812B-S LEDs (LCSC C22461793). See the
[module reference](../docs/modules/LED_Matrix.md) and
[exact LED datasheet](../Module%20Datasheets/XL-5050RGBC-2812B-S_C22461793.pdf).

## Hardware and output

| Function | MCU pin | Peripheral |
|---|---|---|
| CS | PA15 (38) | SPI1 NSS, AF0 |
| SCK | PB3 (39) | SPI1 SCK, AF0 |
| MISO | PB4 (40) | SPI1 MISO, AF0 |
| MOSI | PB5 (41) | SPI1 MOSI, AF0 |
| LED DIN | PB15 (28) | TIM15 CH2, AF1 |

Header: 1 MISO, 2 MOSI, 3 SCK, 4 5 V, 5 CS, 6 GND. The fitted 24 MHz
external crystal feeds PLL x3: core/APB/timer run at 72 MHz, flash latency 2.
Clock failure halts without HSI fallback.

TIM15 update DMA1 channel 1 feeds CCR2. The 90-tick period is 1.25 us
(800 kbit/s); zero uses 22 high ticks (0.306 us), one uses 68 (0.944 us).
GRB, MSB first, follows datasheet pages 6–7. Each frame has 300 us low before
and after its 48 pixels, approximately 2.04 ms total. DIN is actively low
while idle. DMA error or a >10 ms watchdog latches a fault until MCU reset.
SPI interrupts remain enabled during output.

Startup sends black. Host pixels are RGB bytes 0–255; linear index 0–47 means
schematic LED1–LED48. Physical orientation/XY mapping awaits the PCB. No
animations or fonts are implemented. Brightness defaults to 32/255, using
`(channel * brightness + 127) / 255`; original RGB is retained. Brightness 0
blanks output without losing pixels. This scale is not a current limiter;
validate the supply/connector/thermal budget before setting a production
maximum (the datasheet specifies 12 mA per colour channel).

## SPI protocol, version 1

Mode 0, MSB first, 8-bit, at most 250 kHz. Transfer exactly 160 bytes per
active-low CS pulse. Allow >=10 us CS setup before the first clock and >=1 ms
CS high between transactions. Wait >=20 ms after module power/reset.

| Request bytes | Meaning |
|---|---|
| 0–2 | `42 4D 01` hex: BM, version 1 |
| 3 | Sequence 0–255 |
| 4 | Command |
| 5 | Payload length 0–144 |
| 6–7 | Reserved zero |
| 8 onward | Payload; unused bytes through 157 must be zero |
| 158–159 | CRC high byte, low byte |

CRC-16/CCITT-FALSE covers bytes 0–157 including padding: polynomial 0x1021,
initial 0xFFFF, no reflection/final XOR; `123456789` gives 0x29B1.

| Command | Code | Payload |
|---|---|---|
| FETCH | 0x00 | Empty; preserve previous command acknowledgement |
| INFO | 0x01 | Empty |
| STATUS | 0x02 | Empty |
| FRAME | 0x10 | 144 bytes: R0,G0,B0 through R47,G47,B47 |
| FILL | 0x11 | R,G,B for all pixels |
| PIXEL | 0x12 | Index 0–47, R,G,B |
| CLEAR | 0x13 | Empty; black pixels, preserve brightness |
| BRIGHTNESS | 0x14 | One byte 0–255; retransmit retained RGB |

Pixel/brightness commands start a complete frame immediately when idle.
No queue is kept; BUSY or rejected commands leave pixels/brightness unchanged.

| Reply bytes | Meaning |
|---|---|
| 0–2 | `4D 42 01` hex: MB, version 1 |
| 3–4 | Acknowledged sequence, command |
| 5 | Status |
| 6 | Flags: bit 0 busy, bit 1 output fault, bit 2 transport error |
| 7 | Payload length |
| 8 onward | Payload, zero padding through 157 |
| 158–159 | Same CRC convention |

Statuses: 0 OK, 1 bad CRC, 2 bad format/padding, 3 unknown command, 4 bad
length, 5 out-of-range pixel, 6 busy, 7 output error. INFO/STATUS return
`48, 3, brightness, 1` (count, bytes per pixel, brightness, GRB identifier).
Startup sequence/command are 0xFF/0xFF.

SPI returns the reply prepared **before** the current transaction. Send a
command, wait with CS high, then send a valid FETCH to read its acknowledgement.
Check CRC, sequence and command. FETCH preserves that acknowledgement and
refreshes flags for subsequent transactions. OK means output started; poll
STATUS/FETCH until busy clears if completion is needed. No optical feedback
or sequence deduplication exists. Avoid sequence reuse while an ACK is pending.

Short/overlong transactions and receive errors discard the request and set
the transport flag. CS held low >100 ms aborts; release CS and honor the idle
gap to recover. Full-size packets with bad format/CRC return an error ACK.
A valid non-FETCH packet clears the transport flag. A lost reply does not
prove non-execution: retry FETCH and resolve the outcome before another command.

## Host integration

[Host/matrix_host.c](Host/matrix_host.c) provides portable C callbacks for SPI
exchange and microsecond delay. It uses 2 ms idle gaps, sends each command
once, then retries FETCH up to eight times. True means a matching CRC-valid
ACK arrived: inspect status/flags. False means outcome unknown. Compile the
helper alongside the main board application; peripheral setup is external.

```c
uint8_t reply[160];
uint8_t red[] = {255, 0, 0};
MatrixHost host = {context, board_spi_exchange, board_wait_us, 0};
bool acknowledged = MatrixHost_Command(&host, MATRIX_FILL, red, 3, reply);
/* Check acknowledged, reply[5] status and reply[6] flags. */
```

Callbacks must meet the SPI contract, including CS setup and exactly one
160-byte transfer per CS pulse. Main ESP32 TFT pins: SCK GPIO21, MOSI GPIO48,
MISO GPIO47, CS GPIO14. Set the level-shifter enable according to its verified
circuit/component. Simultaneous TFT/matrix use on the single TFT CS path needs
a separate hardware-selection decision.

## Build and verification

Run `make all`; flash/debug instructions are in [SETUP_WINDOWS.md](SETUP_WINDOWS.md).
Final build uses 8,768 bytes flash and 5,568 bytes statically allocated RAM.
Runtime stack usage still needs hardware verification within the 16 KB RAM.

Install [Tests/requirements.txt](Tests/requirements.txt), then run
`python Tests/run_tests.py` with ARM GCC on PATH. All 13 tests pass. They compile
the real protocol, waveform, SPI transport and host helper for Cortex-M0+ and
execute with Unicorn. Tests cover colour/order/scaling, CRC, commands, atomic
rejection, corrupt/malformed packets, ACK pipeline, CS/error recovery and host
retries. HAL registers and LED output are mocked; FIFO, real interrupt latency,
timer/DMA and electrical timing are not simulated.

No physical-board tests have occurred. On arrival, scope clock, LED pulses,
reset/idle intervals, SPI first/last bytes and MISO release under concurrent
traffic. Verify RGB, first/last pixels, orientation, reset/fault recovery and
power/temperature at intended brightness before raising its limit.
