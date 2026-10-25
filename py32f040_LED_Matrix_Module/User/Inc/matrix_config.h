/** @file matrix_config.h
 *  @brief Fixed board geometry and timing for XINGLIGHT C22461793 LEDs.
 *  Physical viewing orientation is not assumed: host indices follow LED1..48.
 */
#ifndef BRINCO_MATRIX_CONFIG_H
#define BRINCO_MATRIX_CONFIG_H
#define MATRIX_LEDS 48U
#define MATRIX_RGB_BYTES (MATRIX_LEDS * 3U)
#define MATRIX_TIMER_HZ 72000000U
#define MATRIX_PERIOD_TICKS 90U    /**< 1.25 us / 800 kbit/s at 72 MHz. */
#define MATRIX_ZERO_TICKS 22U      /**< High 0.306 us; low 0.944 us. */
#define MATRIX_ONE_TICKS 68U       /**< High 0.944 us; low 0.306 us. */
#define MATRIX_RESET_CELLS 240U    /**< 300 us low at each end of the waveform. */
#define MATRIX_WAVEFORM_WORDS (2U * MATRIX_RESET_CELLS + MATRIX_LEDS * 24U)
#define MATRIX_DEFAULT_BRIGHTNESS 32U /**< Scale 0..255; not a current limiter. */
#define MATRIX_DMA_TIMEOUT_MS 10U
#define MATRIX_SPI_FRAME_BYTES 160U
#define MATRIX_SPI_TIMEOUT_MS 100U
#endif
