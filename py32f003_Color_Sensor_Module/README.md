# Color sensor module

Standalone copy of `Py32f003_ProjectTemplate` for PY32F003F18P6TU and
VEML6040A3OG. Reports raw RGBW through UART and controls illumination/exposure.
Application code is commented; [module reference](../docs/modules/Color_Sensor.md)
tracks board decisions. Named-colour recognition, white balance, sRGB output,
lux/CCT and surface calibration are not implemented in version 1.

## Hardware status: I2C electrical changes required

The [schematic](../Modules%20Schematics/Schematic_Color_Sensor_UART.pdf) powers
the PY32 at 5 V and the sensor at 3.3 V. It shows no separate SDA/SCL pull-ups
or level shifter. The [PY32 datasheet](../PUYA%20datasheets/PY32F003_Datasheet_Rev1.7.pdf),
Table 5-19, specifies input HIGH >=0.7*VCC: 3.5 V at a 5 V supply. A direct
3.3 V HIGH is therefore not guaranteed. The sensor bus must stay within its
specified voltage limits; pulling it directly to 5 V is not a solution.

Before physical operation, provide external I2C pull-ups and a compatible
voltage arrangement, such as appropriately pulled-up, bidirectional level
shifting. Any MCU supply change also needs review of UART and other connections.
Confirm whether the manufactured PCB includes corrections absent from the PDF.
Firmware and simulated tests cannot resolve this electrical mismatch.

| Function | GPIO / package pin | Configuration |
|---|---|---|
| Sensor SDA | PA2 / 1 | I2C1 SDA, AF12, open-drain, no internal pull |
| Sensor SCL | PA3 / 2 | I2C1 SCL, AF12, open-drain, no internal pull |
| Module TX / main RX | PA4 / 3 | USART2 TX, AF9 |
| Module RX / main TX | PA5 / 4 | USART2 RX, AF9 |
| LED_EN | PA6 / 5 | Open-drain: release for on, sink for off |

PA6 never drives a push-pull 5 V HIGH. Q1's gate is released to the schematic's
3.3 V R2 pull-up when illumination is on. It defaults on, including the reset
interval before firmware controls it; LED 0 sinks the gate to turn it off.
No brightness PWM is implemented. The UART JST is 1 GND, 2 module TX/main RX,
3 module RX/main TX, 4 5 V. Reverse-side cable numbering is intentional.

The fitted 32 MHz crystal directly clocks core/APB; flash latency is 1.
HAL and CMSIS HSE_VALUE both equal 32000000. Startup failure halts without HSI
fallback. Internal I2C uses 100 kHz and seven-bit address 0x10 (HAL address
argument 0x20). This address is not a host UART module address.

## Sensor acquisition

Reference: [Vishay datasheet 84276, Rev. 1.7](../Module%20Datasheets/VEML6040_Vishay_84276.pdf),
pages 7–9, and [Vishay application note 84331](https://www.vishay.com/docs/84331/designingveml6040.pdf),
active-force operation. CONF=0x00; R/G/B/W registers=0x08/09/0A/0B.
Words are 16-bit, low byte first, with an 8-bit register pointer and repeated
START on reads. The driver uses HAL memory-read, not separate STOP-delimited
pointer-write/data-read operations. Reserved bits remain zero.

Acquisition is a foreground state machine:

1. Allow 10 ms engineering power/illumination settling time; reset I2C software
   and hardware state, then write shutdown/force configuration.
2. Enable force mode and verify CONF readback. Default integration is 160 ms.
3. Write TRIG, wait without blocking UART, then read four individual words.
4. Publish only after all four reads succeed; trigger the next exposure afterward.

Force mode holds a completed exposure while the four channels are read.
The wait is `IT + IT/4 + 10 ms`: 60/110/210/410/810/1610 ms for the six
supported integration settings. This margin and settling delay are engineering
choices to verify on hardware, not manufacturer-guaranteed maximum timings.
There is no separate data-ready or identity register used by this driver.
CONF readback is a communication/configuration check, not unique device identity.

One service call performs at most one sensor register transaction. HAL transfers
use a 5 ms timeout; the bundled HAL has a separate approximately 25 ms BUSY wait.
UART RX interrupts remain available except for the HAL's short two-byte read
critical sections. No foreground integration delay or heap allocation is used.

An I2C/readback failure invalidates the cached sample and sets error. After
500 ms, the driver resets/reconfigures I2C and retries; error clears only after
a complete fresh result. Peripheral reset is not nine-clock physical bus recovery;
a stuck line, absent pull-up or electrical mismatch may require board repair/reset.

Readings contain raw R/G/B/W counts 0–65535, not 8-bit display RGB or percentages.
All-zero data is a valid dark reading. The near-saturation mask uses bits 0–3
for R/G/B/W, set when that channel is >=65520. It is an engineering warning,
not a calibrated optical limit. Changing exposure or illumination immediately
invalidates the old sample, abandons its in-flight exposure and restarts settling.
Unchanged settings preserve the reading. Settings are RAM-only and reset to defaults.

## UART protocol, version 1

115200 baud, 8N1, uppercase ASCII, LF or CRLF command terminator. Replies end
CRLF. Send one command and wait for its response before the next. No unsolicited
stream is sent. Setting acknowledgements mean accepted locally; check READ or
STATUS for subsequent sensor success. UART remains useful if the sensor is absent.

| Command | Response | Meaning |
|---|---|---|
| PING | PONG | Link check |
| INFO | INFO BRINCO COLOR 1 | Protocol identity |
| READ | COLOR r g b w mask | Latest complete raw counts and near-saturation mask |
| RAW | Same as READ | Explicit raw-data alias |
| STATUS | STATUS valid error it_ms led sequence age_ms mask | Cached state and requested settings |
| LED 0 / LED 1 | OK LED 0 / OK LED 1 | Illumination off/on |
| IT 40 / 80 / 160 / 320 / 640 / 1280 | OK IT value | Integration milliseconds; include IT before the value |

Example commands and replies (values are illustrative):

```text
IT 160
OK IT 160
READ
ERR NOT_READY
READ
COLOR 1200 800 500 2600 0
LED 0
OK LED 0
```

READ/RAW return ERR SENSOR following sensor failures, otherwise ERR NOT_READY
until a fresh valid sample exists. Getters invalidate samples older than the
exposure wait plus 100 ms. STATUS is available throughout startup/retries;
valid/error/led are 0/1. Sequence counts complete published exposures and wraps
at uint32; age is milliseconds since last publication, also with wrap-safe tick
arithmetic. Sequence zero means no publication. Metadata shows requested settings;
old channel/mask fields can remain cached while valid=0 and must not be used.

Invalid setting arguments return ERR ARGUMENT without changing state. Only the
exact six IT values and LED 0/1 are accepted. Unknown commands return ERR COMMAND.
The receive ring holds 127 bytes; a line may contain 31 printable characters.
ERR LINE indicates an oversized/non-printable line; ERR LINE_TIMEOUT indicates
a partial line idle >=500 ms; ERR RX indicates UART errors/overflow. Following
these errors, discard continues through the next LF, preventing a damaged suffix
from becoming a setting command. Send a fresh command after that delimiter.
Host timeouts handle missing/truncated replies; replies are not automatically retried.

## Build and tests

Run `make all` in this folder. ELF/BIN/HEX go to Build; generated output is
ignored. [SETUP_WINDOWS.md](SETUP_WINDOWS.md) covers programming/debugging.
Build uses 9,364 bytes flash and 1,952 bytes linked RAM, including the vendor
512-byte heap and 1,024-byte minimum-stack reservations. Real stack use still
needs validation. The inherited linker script emits an RWX LOAD-segment warning.

With ARM GCC on PATH, install `py -m pip install -r Tests/requirements.txt`,
then run `py Tests/run_tests.py`. All 15 tests pass. They compile the actual
color_sensor.c, color_bus.c and module_uart.c for ARM and execute under Unicorn,
with simulated time, GPIO, UART and I2C HAL calls. Tests check word/address format,
GPIO configuration requests, register sequence, all exposure settings, waiting,
atomic publication, partial failures/retry, illumination changes, saturation,
dark data, stale/tick rollover behavior and command/error handling.

The tests do not execute the real I2C HAL or sensor silicon and cannot prove
repeated-START wire timing, input thresholds, pull-ups, lamp power or optical
accuracy. No physical-board validation has occurred.

## Hardware bring-up

Resolve pull-ups/logic compatibility first. Scope MCU clock, both I2C bus
voltage levels, rise times, ACKs and repeated START. Verify every word's byte
order, force-mode conversion wait at each IT, LED_EN release/sink behavior,
UART latency under disconnect/stuck-line conditions and recovery. Record dark,
white and reference-colour measurements at fixed distance with illumination
on/off; establish calibration and any classification requirements afterward.
