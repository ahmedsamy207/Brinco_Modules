# TFT display and resistive touch module

Copied from Py32f040_ProjectTemplate. Target is the owner's
[LCDWIKI MAR2406 ILI9341 module](https://www.lcdwiki.com/2.4inch_Arduino_Display),
with eight-bit parallel display and four-wire resistive touch. Scope now includes
display drawing **and touch**; SD remains excluded. The linked manufacturer
manual documents onboard 5 V/3.3 V level conversion. Other similarly named
shields, including the ST7789V variant linked on that page, are not interchangeable.

The owner's intended purchase is [Ampere product 25678](https://ampere-electronics.com/product/2-4-inch-tft-lcd-shield-touch-display-module-for-arduino/).
Its indexed listing names MAR2406/ILI9341 but does not establish the supplied
board's touch wiring. The current firmware uses the LCDWIKI-specific CS/RS
profile below. Some shields instead use WR/RS with D6/D7; that profile would
map to PA6/PA7/PB0/PB1 and requires a firmware change, not an assumed match.
WR=PB1/ADC9 and RS=PB0/ADC8 are already ADC-capable on this carrier. Confirm
the actual module pin map before changing firmware or reworking the PCB.

PY32F040C1BT6 uses fitted 24 MHz HSE, PLL x3 to 72 MHz, flash latency 2 and
consistent HAL/CMSIS HSE_VALUE. Clock failure halts without HSI fallback.
Default view is native 240x320 portrait; rotations 1/3 provide 320x240 landscape.
Pixels use RGB565. No full framebuffer is allocated (one would need 153,600 bytes).
Initialization is write-only: configured controller identity is not electrically
probed. Verify the actual delivered panel is the documented ILI9341 model.

## Confirmed carrier wiring

[Schematic](../Modules%20Schematics/Schematic_TFT_Module.pdf),
[module reference](../docs/modules/TFT.md),
[commented pin definitions](User/Inc/tft_board.h).

| Signal | MCU GPIO / package pin |
|---|---|
| Host SPI1 NSS | PA15 / 38 |
| Host SPI1 SCK | PB3 / 39 |
| Host SPI1 MISO | PB4 / 40 |
| Host SPI1 MOSI | PB5 / 41 |
| LCD D0..D7 | PA0..PA7 / 10..17 |
| LCD RS | PB0 / 18 |
| LCD WR | PB1 / 19 |
| LCD RD | PA11 / 32 |
| LCD CS | PB8 / 45 |
| LCD RESET | PB9 / 46 |

SPI header: 1 MISO, 2 MOSI, 3 SCK, 4 5 V, 5 CS, 6 GND. The main ESP32
connects directly through this TFT header, bypassing the UART hub. Shared
simultaneous TFT/LED-matrix selection has not been specified.

Display connector wiring: H1 pins 3..8 = D2..D7; H2 pins 1/2 = D0/D1;
H3 pins 2..6 = RESET/CS/RS/WR/RD; H4 pins 2/4 = GND/5 V. The unused-in-v1
SD SPI2 pins are PB12 CS, PB13 SCK, PB14 MISO, PB15 MOSI. PB12 is parked HIGH
to deselect the unused card; SPI2 and SD operations are not implemented.

The manufacturer's MAR2406-specific touch examples define XP=Arduino D8,
YM=D9, XM=A2, YP=A3. These translate to:

| Touch electrode | Shared LCD net | Carrier GPIO |
|---|---|---|
| XP | D0 | PA0, ADC0 |
| YM | D1 | PA1, ADC1 |
| XM | RS | PB0 |
| YP | CS | PB8 (no ADC input) |

Do not copy the different pins from generic TouchScreen library examples.
The firmware samples floating YM for X and floating XP for Y: opposite ends
of an undriven resistive layer have the same ideal sense voltage. For pressure,
XM is driven LOW and YP HIGH, while XP/YM are sampled. This avoids needing ADC
on PB8. The method and actual settling/loading need board validation.

WR and RD stay HIGH while touch borrows pins. Every electrode becomes high-Z
before excitation changes, and every path restores the LCD outputs, including
ADC errors. Between operations all four electrodes are parked HIGH to avoid
continuous sheet current at idle. No LCD transaction is made during sensing. TIM3 supplies wrap-safe
microsecond delays; ADC runs at 9 MHz (72/8) with 239.5-cycle sample time.

## Drawing and touch behavior

Startup performs hardware/software reset and ILI9341 initialization, then queues
a black clear. Allow at least 1 second after power/reset before host traffic and
poll busy before a drawing command. Startup timing must be checked on hardware.
The manufacturer-derived initialization and license are recorded in
[ThirdParty](ThirdParty/README.md).

One drawing job is accepted at a time. Each service renders at most 64 pixels,
then allows touch/SPI foreground work. Rectangles and row pixels are written
in segments; lines use Bresenham rasterization. All coordinates must fit the
current orientation; invalid requests are rejected, not clipped. Drawing is
progressive, without frame-synchronous presentation or tear prevention.
Images and text can be rendered by the host as RGB565 row chunks. There is no
on-module font, image decoder, circle primitive, SD loader or autonomous UI.

Touch is sampled every 20 ms, best effort. Each ADC signal discards one sample
then takes the median of three. A spread >80 counts marks an unstable scan.
The pressure proxy is 4095-(z2-z1), or zero if z2<z1; it is not force or calibrated
resistance. Default contact band is 80..3900. Two successive stable contacts
within 160 raw counts per axis qualify down. Release/failure invalidates down
immediately; a service gap >100 ms requires requalification. Thresholds are
engineering defaults in tft_config.h and need real-panel adjustment.

Raw coordinates are available without calibration. Pixel coordinates remain
0xFFFF until a valid calibrated contact exists. Calibration endpoints are
RAM-only and defined in native portrait coordinates; mapping follows all four
display rotations. An optional swap flag handles exchanged axes. There are no
guessed factory calibration values or touch-event queue: short taps between
polls can be missed, and the host must poll TOUCH for interaction.

## SPI protocol, version 1

Mode 0, MSB first, eight-bit transfers, <=250 kHz. Exactly 160 bytes under one
active-low CS pulse; >=10 us CS setup and >=5 ms CS HIGH between transactions.
The portable host helper uses 5 ms idle gaps to allow foreground ADC, rendering
and CRC processing before re-arming. Host requests and replies use
different TFT signatures from the LED matrix; they are not interchangeable.

| Request bytes | Meaning |
|---|---|
| 0..2 | 42 54 01 hex: BT, version 1 |
| 3 | Host sequence, 0..255 |
| 4 | Command |
| 5 | Payload length, 0..144 |
| 6..7 | Zero reserved |
| 8 onward | Payload; all unused bytes through 157 must be zero |
| 158..159 | CRC high byte, low byte |

CRC-16/CCITT-FALSE over bytes 0..157: polynomial 0x1021, initial 0xFFFF,
no reflection/final XOR; check vector 123456789 = 0x29B1. Payload words are
little-endian. RGB565 is R bits 15..11, G 10..5, B 4..0: red=0xF800,
green=0x07E0, blue=0x001F. The driver sends each pixel HIGH byte first to LCD.

| Command | Code | Payload |
|---|---|---|
| FETCH | 0x00 | Empty; preserve previous acknowledgement |
| INFO / STATUS | 0x01 / 0x02 | Empty |
| TOUCH | 0x03 | Empty |
| CLEAR | 0x10 | color u16 |
| RECT | 0x11 | x,y,width,height,color: five u16 words |
| PIXELS | 0x12 | x,y,count: three u16 words, then count RGB565 words; count 1..69, one row |
| ROTATE | 0x13 | One byte 0..3; does not clear/remap existing display RAM |
| LINE | 0x14 | x0,y0,x1,y1,color: five u16 words |
| CALIBRATE | 0x20 | x-left,x-right,y-top,y-bottom: four u16 endpoints, then one flags byte |

Calibration endpoints must be <=4095, with each span >=128. Descending endpoints
invert an axis. Flags bit0 swaps raw X/Y before mapping; all other bits must be
zero. Invalid calibration leaves the previous settings unchanged. Calibration,
INFO/STATUS and TOUCH are available while drawing; drawing/rotation commands
return BUSY when a job is pending. Bounds/length validation precedes BUSY.

| Reply bytes | Meaning |
|---|---|
| 0..2 | 54 42 01 hex: TB, version 1 |
| 3..4 | Acknowledged sequence and command |
| 5 | Status |
| 6 | Live flags: bit0 drawing busy, bit1 touch down, bit2 transport error, bit3 touch valid, bit4 calibrated |
| 7 | Reply payload length |
| 8 onward | Payload, zero padding through 157 |
| 158..159 | Same CRC convention |

Statuses: 0 OK, 1 bad CRC, 2 bad format/padding, 3 unknown command, 4 bad length,
5 bad range, 6 busy. Startup sequence/command are 0xFF/0xFF. INFO/STATUS payload
is ten bytes: width u16, height u16, rotation u8, pixel-bits=16 u8,
configured-controller=0x9341 u16, max-row-pixels=69 u8, touch-supported=1 u8.

TOUCH payload is a 20-byte snapshot: byte0 flags (bit0 valid, bit1 down, bit2
calibrated); byte1 rotation; bytes2..3 pixel X; 4..5 pixel Y; 6..7 raw X;
8..9 raw Y; 10..11 pressure proxy; 12..15 age in ms u32; 16..19 sequence u32.
Pixel coordinates are 0xFFFF without valid calibrated contact. Live reply-header
flags may be newer than this saved TOUCH snapshot. A failed ADC scan clears
valid/down, while raw fields may retain older values; check the flags.

SPI returns the reply prepared **before** the current request. Send a command,
wait CS HIGH, then FETCH to obtain its ACK. Verify CRC and match sequence/command.
Drawing OK means queued; poll busy to wait for completion. No sequence deduplication
exists. If an ACK is lost, retry FETCH rather than blindly repeating the drawing
command. Short/overlong transfers discard input and set transport error; CS held
LOW >100 ms aborts. Release CS and respect the idle gap to recover. A valid
non-FETCH packet clears transport error. Full-size bad packets produce an error ACK.

## Host helper and calibration workflow

[Host/tft_host.c](Host/tft_host.c) supplies TFTHost_Command with exchange/delay
callbacks; it is not a complete ESP32 application. Callbacks must implement the
SPI contract, including CS setup and all 160 bytes (1,280 clock cycles) per pulse. The helper
sends each command once, then retries FETCH up to eight times. True means a
matching ACK arrived: inspect its status/flags. False means outcome unknown.
Serialize callers and avoid sequence reuse while a reply remains outstanding.
Main ESP TFT pins: SCK21, MOSI48, MISO47, CS14. Configure the main-board level
shifter according to its verified hardware before integration.

For example, a red RECT at (10,20), size 100x50, uses payload bytes
0A 00 14 00 64 00 32 00 00 F8. A pixel row uses little-endian RGB565 words.

To calibrate on arrival: select rotation 0 and use drawing commands to show
reference targets near the native portrait edges. Read stable TOUCH raw X/Y
while touching known targets; derive/extrapolate the raw endpoints corresponding
to x=0/239 and y=0/319, accounting for target insets. Send CALIBRATE with those
values and swap flag if required. Check corners/centre, then verify all rotations.
Host storage can resend the measured calibration after every module reset.

## Build, software tests and hardware validation

Run make all; [SETUP_WINDOWS.md](SETUP_WINDOWS.md) covers programming/debugging.
Build uses 11,732 bytes flash and 2,400 bytes linked RAM, including the template's
512-byte heap and 1,024-byte minimum-stack reservations. Runtime stack headroom
still needs measurement. The active application uses no heap or framebuffer.

With ARM GCC on PATH: install `py -m pip install -r Tests/requirements.txt`,
then run `py Tests/run_tests.py`. All 18 ARM-emulated tests pass. They compile
the real renderer, touch filtering/mapping, packet protocol, SPI transport and
host helper; mocked LCD IO writes a test-only framebuffer. Tests cover bounds,
chunk budgets, RGB565 copying, line directions, rotation, corruption, busy/ACK
handling, touch filtering, calibration validation/clamping, rollover and SPI recovery.

Tests do not execute tft_io.c or the real HAL. They cannot establish GPIO/ADC
sequencing, electrical drive current, first/last SPI-byte timing, FIFO behavior,
LCD initialization, actual colours, optical orientation or touch accuracy.
No physical-board validation has occurred. Before touch excitation at 5 V,
measure panel sheet resistance and verify GPIO source/sink currents against the
PY32 ratings; 20 mA per pin is an absolute maximum, not a design target.
Scope LCD setup/WR/RD timing, shared-pin restoration, ADC settling, both touch
axes/pressure and concurrent SPI. Validate colour bars, endpoints, rotations,
fast drags/taps, release, reset and calibration persistence in the host.
