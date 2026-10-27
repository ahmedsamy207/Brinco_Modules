/** @file tft_config.h @brief Display/host/touch engineering defaults and units. */
#ifndef BRINCO_TFT_CONFIG_H
#define BRINCO_TFT_CONFIG_H
#define TFT_SPI_FRAME_BYTES 160U
#define TFT_MAX_PAYLOAD 144U
#define TFT_SPI_TIMEOUT_MS 100U
#define TFT_NATIVE_WIDTH 240U
#define TFT_NATIVE_HEIGHT 320U
#define TFT_DRAW_BUDGET 64U /* Maximum pixels rendered per foreground service. */
#define TFT_MAX_ROW_PIXELS 69U /* 6-byte header + 69 RGB565 words = 144 bytes. */
#define TFT_TOUCH_PERIOD_MS 20U
#define TFT_TOUCH_STALE_MS 100U
#define TFT_TOUCH_Z_MIN 80U
#define TFT_TOUCH_Z_MAX 3900U /* Pressure proxy, not force/ohms. Verify per panel. */
#define TFT_TOUCH_SPREAD 80U /* Max ADC-count spread of three samples per signal. */
#endif
