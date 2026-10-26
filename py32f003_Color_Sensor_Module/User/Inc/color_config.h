/** @file color_config.h @brief Internal sensor bus and acquisition policy. */
#ifndef BRINCO_COLOR_CONFIG_H
#define BRINCO_COLOR_CONFIG_H
#define COLOR_I2C_ADDRESS 0x10U /* Seven-bit sensor address, not host UART address. */
#define COLOR_I2C_HZ 100000U
#define COLOR_I2C_TIMEOUT_MS 5U /* Vendor HAL BUSY wait can separately take 25 ms. */
#define COLOR_DEFAULT_IT_MS 160U
#define COLOR_DEFAULT_LED 1U /* On = PA6 released to external 3.3 V gate pull-up. */
#define COLOR_SETTLE_MS 10U /* Engineering power/illumination settling allowance. */
#define COLOR_RETRY_MS 500U
#define COLOR_CONF_REG 0x00U
#define COLOR_RED_REG 0x08U
/* Near full-scale warning, not a calibrated optical saturation boundary. */
#define COLOR_SATURATION_COUNT 65520U
#endif
