/** @file tft_board.h
 *  @brief Pin map from Schematic_TFT_Module.pdf; no GPIO is driven here.
 *  Target LCDWIKI MAR2406 ILI9341 shield with onboard level conversion.
 *  Touch XP=D0=PA0, YM=D1=PA1, XM=RS=PB0, YP=CS=PB8. Both ends of
 *  each resistive layer are equivalent sense points when floating. PA0/PA1
 *  ADC channels 0/1 avoid needing an ADC on PB8. SD remains excluded.
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
#define TFT_RS_PIN GPIO_PIN_0
#define TFT_WR_PORT GPIOB
#define TFT_WR_PIN GPIO_PIN_1
#define TFT_RD_PORT GPIOA
#define TFT_RD_PIN GPIO_PIN_11
#define TFT_CS_PORT GPIOB
#define TFT_CS_PIN GPIO_PIN_8
#define TFT_RESET_PORT GPIOB
#define TFT_RESET_PIN GPIO_PIN_9

/* Schematic reference only: no SD driver or SD pin initialization in v1. */
#define TFT_SD_PORT GPIOB
#define TFT_SD_CS_PIN GPIO_PIN_12
#define TFT_SD_SCK_PIN GPIO_PIN_13
#define TFT_SD_MISO_PIN GPIO_PIN_14
#define TFT_SD_MOSI_PIN GPIO_PIN_15
#endif
