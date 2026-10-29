/** @file tft_board.h
 *  @brief Pin map verified against Schematic_TFT_Module.pdf on 2026-10-29.
 *  LCD CS=PB0/ADC8, RS=PB1/ADC9, WR=PB8. The revised PDF now matches
 *  the owner's confirmed carrier revision and this firmware.
 *  LCDWIKI MAR2406 touch profile: XP=D0=PA0, YM=D1=PA1, XM=RS, YP=CS.
 *  Supplier stock's electrode wiring still needs confirmation; alternate
 *  WR/RS shields need a different profile. SD remains excluded.
 */
#ifndef BRINCO_TFT_BOARD_H
#define BRINCO_TFT_BOARD_H
#include "main.h"

/* Host SPI1: same dedicated ESP32 TFT header used by the LED matrix option. */
#define TFT_HOST_NSS_PORT GPIOA
#define TFT_HOST_NSS_PIN GPIO_PIN_15
#define TFT_HOST_SPI_PORT GPIOB
#define TFT_HOST_SCK_PIN GPIO_PIN_3
#define TFT_HOST_MISO_PIN GPIO_PIN_4
#define TFT_HOST_MOSI_PIN GPIO_PIN_5
#define TFT_HOST_SPI_AF GPIO_AF0_SPI1

/* Parallel 8080-style data wiring: byte bit n maps directly to PAn, n=0..7.
 * Preserve all other GPIOA bits when writing data; PA11 is LCD_RD, and the
 * remaining pins include host NSS and SWD. No framebuffer is allocated. */
#define TFT_DATA_PORT GPIOA
#define TFT_DATA_MASK 0x00FFU
#define TFT_RS_PORT GPIOB
#define TFT_RS_PIN GPIO_PIN_1
#define TFT_WR_PORT GPIOB
#define TFT_WR_PIN GPIO_PIN_8
#define TFT_RD_PORT GPIOA
#define TFT_RD_PIN GPIO_PIN_11
#define TFT_CS_PORT GPIOB
#define TFT_CS_PIN GPIO_PIN_0
#define TFT_RESET_PORT GPIOB
#define TFT_RESET_PIN GPIO_PIN_9

/* Four shared electrodes. Conventional reads use YP/CS ADC8 for X,
 * XM/RS ADC9 for Y; pressure reads XM then YP. WR is never an electrode. */
#define TFT_TOUCH_XP_PORT GPIOA
#define TFT_TOUCH_XP_PIN GPIO_PIN_0
#define TFT_TOUCH_YM_PORT GPIOA
#define TFT_TOUCH_YM_PIN GPIO_PIN_1
#define TFT_TOUCH_XM_PORT TFT_RS_PORT
#define TFT_TOUCH_XM_PIN TFT_RS_PIN
#define TFT_TOUCH_YP_PORT TFT_CS_PORT
#define TFT_TOUCH_YP_PIN TFT_CS_PIN
#define TFT_TOUCH_XM_ADC ADC_CHANNEL_9
#define TFT_TOUCH_YP_ADC ADC_CHANNEL_8

/* SD reference: CS is parked HIGH; no SD driver or SPI2 operations. */
#define TFT_SD_PORT GPIOB
#define TFT_SD_CS_PIN GPIO_PIN_12
#define TFT_SD_SCK_PIN GPIO_PIN_13
#define TFT_SD_MISO_PIN GPIO_PIN_14
#define TFT_SD_MOSI_PIN GPIO_PIN_15
#endif
