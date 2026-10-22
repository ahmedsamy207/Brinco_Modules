/** @file ir_array_config.h
 *  @brief Board variant and optical classification settings.
 *  @note Values are provisional until tested at the robot's working height.
 *        Threshold indices always refer to physical S1..S5, even in 3 mode.
 */
#ifndef BRINCO_IR_ARRAY_CONFIG_H
#define BRINCO_IR_ARRAY_CONFIG_H

/* The PCB has no sensor-count strap. Program the correct build for the board. */
#ifndef IR_SENSOR_COUNT
#define IR_SENSOR_COUNT 5
#endif
#if IR_SENSOR_COUNT != 3 && IR_SENSOR_COUNT != 5
#error "IR_SENSOR_COUNT must be 3 or 5"
#endif

/* 12-bit ADC thresholds, indexed by physical S1..S5. Tune over the actual
 * line/background at the operating height. These are provisional defaults. */
#define IR_THRESHOLD_S1 2048U
#define IR_THRESHOLD_S2 2048U
#define IR_THRESHOLD_S3 2048U
#define IR_THRESHOLD_S4 2048U
#define IR_THRESHOLD_S5 2048U
#define IR_HYSTERESIS   64U /**< Half-width: switch at threshold +/- this value. */
#define IR_SAMPLE_PERIOD_MS 1U /**< Minimum interval between scan starts. */
#define IR_STALE_MS 20U /**< Host data becomes invalid if sampling falls behind. */

#if IR_HYSTERESIS == 0 || IR_HYSTERESIS > 2047
#error "IR_HYSTERESIS must be between 1 and 2047 ADC counts"
#endif
#if IR_THRESHOLD_S1 < IR_HYSTERESIS || IR_THRESHOLD_S1 > 4095U - IR_HYSTERESIS || \
    IR_THRESHOLD_S2 < IR_HYSTERESIS || IR_THRESHOLD_S2 > 4095U - IR_HYSTERESIS || \
    IR_THRESHOLD_S3 < IR_HYSTERESIS || IR_THRESHOLD_S3 > 4095U - IR_HYSTERESIS || \
    IR_THRESHOLD_S4 < IR_HYSTERESIS || IR_THRESHOLD_S4 > 4095U - IR_HYSTERESIS || \
    IR_THRESHOLD_S5 < IR_HYSTERESIS || IR_THRESHOLD_S5 > 4095U - IR_HYSTERESIS
#error "Thresholds and hysteresis must fit in the 12-bit ADC range"
#endif
#endif
