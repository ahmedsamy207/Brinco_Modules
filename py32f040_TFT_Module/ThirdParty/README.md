# TFT implementation references

Reviewed 2026-10-27. Target is LCDWIKI MAR2406 with ILI9341, as linked by the
project owner. The page also links an ST7789V package; that controller is not
supported by this build. Verify the supplied module variant.

- [Product page](https://www.lcdwiki.com/2.4inch_Arduino_Display)
- [MAR2406 manual](https://www.lcdwiki.com/res/MAR2406/2.4inch_Arduino_8BIT_Module_MAR2406_User_Manual_EN.pdf)
- [ILI9341 datasheet](https://www.lcdwiki.com/res/MAR2406/ILI9341_Datasheet.pdf)
- [Manufacturer examples](https://www.lcdwiki.com/res/Program/Arduino/2.4inch/UNO_8BIT_ILI9341_MAR2406_V1.1/2.4inch_Arduino_8BIT_Module_ILI9341_MAR2406_V1.1.zip)
- [LCDWIKI_KBV initialization source](https://github.com/lcdwiki/LCDWIKI_kbv/blob/master/LCDWIKI_KBV.cpp)

The initialization settings in tft_io.c are adapted from LCDWIKI_KBV, case
0x9341. Its MIT notice is preserved in LCDWIKI_LICENSE.txt. Graphics, host
protocol and PY32 glue here are separate C implementations.

The manufacturer's touch_pen and TouchScreen_Calibr sketches use XP=8,
YM=9, XM=A2, YP=A3, equivalent to LCD D0/D1/RS/CS. The TouchScreen library
credits ladyada/Adafruit under MIT. Its four-wire excitation method was
consulted; our implementation uses complementary sensing/pressure connections
to avoid non-ADC PB8, with separate filtering/calibration. Generic examples
use different pins and must not override the MAR2406-specific wiring.

SHA-256 of reviewed downloads (not copies distributed by this project):

| Reference | SHA-256 |
|---|---|
| MAR2406 manual | 49cc6124cff691d5a593c2b8a8f58431d7635f6facd06e72c40352012c8e4635 |
| ILI9341 datasheet | 24b992a96997313a5a516e000b676f0e55326e06c2baff9c720056d0b8da05ed |
| Manufacturer example ZIP | 3fe62c3f6300c4bb274d5d1a3542bbc6a6ea69955b64709eba8ef6a7ea194f96 |
| LCDWIKI_KBV.cpp | e8181a4f9415c99e1a4b848076a03c2a8df84d2e70ba0d851754988c138f3c78 |
