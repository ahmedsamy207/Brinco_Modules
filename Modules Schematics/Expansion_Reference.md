# Expansion chain reference

Reviewed 2026-10-22 against `Schematic_Expansion_Board_UART.pdf` (revision 1.0,
2026-09-23, added in commit `e5eed86`), `Schematic_I2S_EXP_Module.pdf`
(revision 1.0, 2026-09-11), and the main board schematic.

## Intended system topology

The designer confirms that the main board, I2S board and expansion board can
be cascaded using their six-pin expansion connectors. The main board EXP
header belongs to the main PY32's SPI2, not the ESP-to-PY32 SPI1 link.

An example connection is:

```text
ESP32
  | SPI through the main board level shifter
Main board PY32 SPI1
  | Main PY32 SPI2 -> EXP
I2S board EXP_IN -> passive signal pass-through -> EXP_OUT
  |
Expansion PY1 SPI1
  | Expansion PY1 SPI2 -> internal nets
Expansion PY2 SPI1
  | Expansion PY2 SPI2 -> EXP_OUT
Additional expansion stage, if fitted
```

The I2S board does not contain a PY32 or an SPI peripheral on the expansion
nets. Its EXP_IN/EXP_OUT carry the same CS, SCK, MISO, MOSI, 5 V and GND nets.
The MAX98357A speaker path and MSM261DDB019 PDM microphone use the separate
eight-pin audio connector to the ESP32. Audio does not travel over SPI.
The audio connector must also be connected to use the audio hardware;
the six-pin expansion cable alone does not carry audio or the 3.3 V mic supply.

The expansion board is an active two-MCU hub, not a passive shared SPI bus:
PY1's SPI2 nets connect to PY2's SPI1 pins. Its final EXP_OUT is driven by
PY2's SPI2. Firmware must receive upstream requests and forward downstream
ones, with responses relayed back upstream. Upstream slave/downstream master
roles follow the intended topology, but clock mode, rates and framing still
need to be designed. A single CS net on each segment is not a global chip
select simultaneously selecting all hubs.

## Expansion MCU mapping

Both MCUs are PY32F040C1BT6, powered at 5 V, with separate 24 MHz crystals,
NRST pull-ups, grounded BOOT0 and individual PRG/SWD headers.

Each uses the same peripheral pin groups as the main board PY32:

| Interface | Pins |
|---|---|
| Upstream SPI1 | CS PA4, SCK PA5, MISO PA6, MOSI PA7 |
| Downstream SPI2 | CS PB9, SCK PB8, MISO PB6, MOSI PB7 |
| USART1 | TX PA9, RX PA10 |
| USART2 | TX PA2, RX PA3 |
| USART3 | TX PB10, RX PB11 |
| USART4 | TX PA0, RX PA1 |
| SWD | SWDIO PA13, SWCLK PA14 |

The inter-MCU SPI nets are PY1_CS2, PY1_SCK2, PY1_MISO2 and PY1_MOSI2:
PY1 drives/reads these through SPI2; PY2 receives them on SPI1.
PY2_CS2/SCK2/MISO2/MOSI2 go to EXP_OUT.

| Connector | Owning MCU | UART |
|---|---|---|
| H1 | PY1 | USART2 |
| H2 | PY1 | USART3 |
| H3 | PY1 | USART4 |
| H4 | PY1 | USART1 |
| H5 | PY2 | USART1 |
| H6 | PY2 | USART4 |
| H7 | PY2 | USART2 |
| H8 | PY2 | USART3 |

All eight UART connectors use 1 = 5 V, 2 = hub TX, 3 = hub RX, 4 = GND,
matching the main board UART convention. Use the designer-specified reverse-side
JST module cables. One expansion board adds eight UART ports, giving twelve
including the main board's four. The I2S pass-through adds no UART ports.

## Six-pin connector correspondence

Read physical pin numbers, not left/right drawing position: the IN symbols
are mirrored relative to OUT. All reviewed six-pin OUT connectors share
one numbering scheme, and both reviewed EXP_IN connectors share the other:

| Signal | Main EXP / EXP_OUT pin | EXP_IN pin |
|---|---:|---:|
| MISO | 1 | 2 |
| CS | 2 | 1 |
| 5 V | 3 | 4 |
| SCK | 4 | 3 |
| GND | 5 | 6 |
| MOSI | 6 | 5 |

The intended cable/connector assembly must preserve these signal identities.
This six-pin correspondence differs from the four-pin JST module cable
reversal; do not apply the four-pin rule to expansion headers.

## Firmware implementation and remaining work

[Expansion firmware](../py32f040_Expansion_Module/README.md) now implements the
first SPI routing protocol: identical images for both MCUs, four115200 8N1
UARTs per MCU, relative hops0..7,64-byte CRC frames, buffered binary payloads,
receipt caching, error flags and finite forwarding deadlines. Its README is
canonical for protocol/timing details; this schematic alone does not establish
validated electrical timing, cable length or cascade performance.

Main ESP32/main-board PY32 hub integration and physical validation remain
pending. Each expansion still adds two active hops, while I2S pass-through adds
none. ESP audio drivers remain separate; the I2S board has no MCU firmware.
