# IR Array module

Line-following sensor firmware for the PY32F003F18P6TU, copied from
`Py32f003_ProjectTemplate`. Supports either all five TCRT5000L sensors or
the three central sensors after removing the two outer board sections.

## Hardware mapping

Source: `Modules Schematics/Schematic_IR_Array_UART.pdf`, revision 1.0.
The five outputs are analog phototransistor collectors with 10 kohm pull-ups
to 5 V. There are no hardware comparators or sensor-count straps shown.

| Physical sensor | Net | GPIO | MCU package pin | ADC channel |
|---|---|---|---:|---:|
| S1 (outer) | A1 | PA5 | 4 | 5 |
| S2 | A2 | PA6 | 5 | 6 |
| S3 (centre) | A3 | PA7 | 6 | 7 |
| S4 | A4 | PA1 | 20 | 1 |
| S5 (outer) | A5 | PA0 | 19 | 0 |

Module TX is PA2/package pin 1, USART1_TX AF1 (schematic net `RX`). Module RX
is PA3/package pin 2, USART1_RX AF1 (schematic net `TX`). The UART net labels
describe the main board's perspective. SWDIO/SWCLK remain on PA13/PA14.

The firmware retains the template's 24 MHz internal HSI clock. The board's
32 MHz crystal is not used. ADC clock is PCLK/4 = 6 MHz, with 239.5-cycle
sampling to allow settling through the pull-up networks.

## Three versus five sensors

Select `IR_SENSOR_COUNT=5` (default) for the full board or `3` for the cut board.
In 3-sensor mode only S2, S3 and S4 are sampled; removed S1/S5 pads are ignored.
Sensor count cannot be reliably inferred from reflectance: a missing sensor
could resemble a present sensor over a dark surface. Flash the matching build.

From this project folder:

```powershell
# Full board, also used by the default VS Code Build/F5 tasks
make all

# Cut board: independent outputs and objects avoid reusing 5-sensor objects
make IR_SENSOR_COUNT=3 all
```

Images:

- Five sensors: `Build/py32f003_IR_Array_Module.bin` / `.elf` / `.hex`.
- Three sensors: `Build3/py32f003_IR_Array_Module.bin` / `.elf` / `.hex`.

The Makefile automatically separates the two sensor-count variants, so
switching between them does not reuse objects compiled for the other count.
Use `make IR_SENSOR_COUNT=3 flash` or the VS Code **Flash (3 sensors, pyOCD)**
task for the cut board. Select **Debug (3 sensors, pyOCD)** to build/debug it.
The default tasks and launch configuration target the 5-sensor build.

## Sensor classification

The ADC is calibrated at startup. A full sensor scan is scheduled every 1 ms
(best effort, subject to foreground UART servicing). Results publish only
after every active channel converts successfully. ADC failure or results
older than 20 ms produce an invalid data byte.

Each sensor has a configurable 12-bit threshold in `User/Inc/ir_array_config.h`.
The provisional default is 2048, with hysteresis of 64 ADC counts:

- On the first complete scan: values >= 2048 are high, lower values are low.
- Subsequently: >= 2112 switches high; <= 1984 switches low.
- Inside that band, the previous state is retained.

A bit value of 1 means **high sensor voltage**, not a calibrated colour label.
With the shown collector pull-ups, stronger reflected IR tends to pull the
voltage low. Determine the black/white mapping and per-sensor thresholds on
the real surface at the robot's operating height. `RAW` exposes the ADC counts
for this calibration. No line-position or steering calculation is performed;
the main board receives individual classified states for its control loop.

## One-byte data format

| Bit | Meaning |
|---|---|
| 7 | 1 = five sensors, 0 = three sensors |
| 6 | 1 = valid, fresh scan; 0 = unavailable/ADC error/stale |
| 5 | Reserved, always 0 |
| 4..0, five-sensor build | bit 0 = S1, 1 = S2, 2 = S3, 3 = S4, 4 = S5 |
| 2..0, three-sensor build | bit 0 = S2, 1 = S3, 2 = S4; bits 3 and 4 are 0 |

Invalid bytes clear all sensor-state bits, preserving the count flag:
`0x80` for five sensors or `0x00` for three. Check bit 6 before interpreting
the lower bits; invalid is not an all-low reading.

Examples:

- Five sensors, only S1/S3/S5 high: `11010101` = `0xD5`.
- Three sensors, S2/S4 high and S3 low: `01000101` = `0x45`.
- All low, valid: `0xC0` for five or `0x40` for three.
- All high, valid: `0xDF` for five or `0x47` for three.

## UART interface

115200 baud, 8N1, no flow control. Commands are case-sensitive ASCII lines
ending in LF (`\n`); CRLF is also accepted. No unsolicited streaming occurs.
Send one command and consume its complete response before sending another.

| Command | Response |
|---|---|
| `READ\n` | Exactly **one raw binary byte** described above; no CR/LF suffix |
| `PING\n` | ASCII `PONG\r\n` |
| `INFO\n` | ASCII `INFO BRINCO IR_ARRAY 1 5\r\n` or `... 3\r\n` |
| `RAW\n` | ASCII `RAW <S1> <S2> <S3> <S4> <S5>\r\n` or `<S2> <S3> <S4>` |

`RAW` returns the single invalid byte if no fresh complete scan is available. The
`READ` reply always occupies one byte, even when the scan is invalid.
Binary data is not printed as decimal or hexadecimal ASCII. For example,
`0x45` is one byte, not the two characters `4` and `5`.

Malformed/unknown commands and UART receive errors return the single invalid
byte (`0x80` or `0x00`). ASCII error lines are deliberately avoided: the first
character of `ERR` is `0x45`, which could look like valid three-sensor data.
The receive queue holds
127 bytes; command lines allow 31 printable characters. An incomplete line
times out after 500 ms of foreground inactivity. After a line/receive error,
send LF to resynchronize before a new command. Blank lines are ignored.
Use a 100 ms host timeout. Diagnostic commands normally return text, but the
host should handle the invalid byte before waiting for their text terminator.
Valid five-sensor data is `0xC0..0xDF`; valid three-sensor data
is `0x40..0x47`. There is no CRC in this initial protocol.

## Validation status and hardware bring-up

Hardware is still in manufacturing. Firmware builds do not validate optical
performance, cut-board trace routing, ADC accuracy, or electrical compatibility
with the main board.

When boards arrive, verify sensor ordering with `RAW`, tune thresholds over
both surfaces, check transitions near the threshold, exercise all sensor
patterns, and verify count/valid bits in both firmware variants. Verify that
the PCB cutouts preserve power and signal routing for S2..S4. Check USART1
timing, UART error recovery, and actual scan rate under the intended polling
load. A UART adapter must use signal levels appropriate to the powered board.
