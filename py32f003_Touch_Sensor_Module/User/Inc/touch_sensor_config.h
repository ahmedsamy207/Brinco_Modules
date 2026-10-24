/** @file touch_sensor_config.h
 *  @brief Touch input polarity and foreground timing, all times in milliseconds.
 *  @note Defaults match XMXW 223 (LCSC C1693666), Rev 1.1, with TOG/AHLB
 *        grounded: direct-mode, active-high OUT. See Module Datasheets/.
 */
#ifndef BRINCO_TOUCH_SENSOR_CONFIG_H
#define BRINCO_TOUCH_SENSOR_CONFIG_H

#ifndef TOUCH_ACTIVE_HIGH
#define TOUCH_ACTIVE_HIGH 1U
#endif
#define TOUCH_STARTUP_MS 600U  /**< Conservative guard; datasheet lists 400 ms settling. */
#define TOUCH_DEBOUNCE_MS 10U  /**< Require observed stability before accepting a state. */
#define TOUCH_SAMPLE_MS 1U     /**< Minimum foreground polling interval. */
#define TOUCH_STALE_MS 20U     /**< Longer service gaps require fresh qualification. */

#if TOUCH_ACTIVE_HIGH != 0 && TOUCH_ACTIVE_HIGH != 1
#error "TOUCH_ACTIVE_HIGH must be 0 or 1"
#endif
#if TOUCH_SAMPLE_MS == 0 || TOUCH_DEBOUNCE_MS < TOUCH_SAMPLE_MS || TOUCH_STALE_MS <= TOUCH_SAMPLE_MS
#error "Invalid touch sampling/debounce/freshness intervals"
#endif
#endif
