/** @file ir_array.h
 *  @brief Foreground ADC acquisition and UART-byte encoding API.
 */
#ifndef BRINCO_IR_ARRAY_H
#define BRINCO_IR_ARRAY_H
#include "main.h"
#include "ir_array_config.h"

/* Bit 5 remains reserved; active high/low states occupy bits 0..4 or 0..2. */
#define IR_PACKET_FIVE_SENSORS 0x80U /**< Bit 7: full board; clear for cut board. */
#define IR_PACKET_VALID        0x40U /**< Bit 6: complete scan within freshness limit. */

typedef struct {
  uint16_t raw[IR_SENSOR_COUNT]; /* Active sensors in S1..S5 or S2..S4 order. */
  uint8_t states;       /**< Compact sensor bits: 1 means high analog voltage. */
  uint8_t valid;        /**< Zero means raw/state data must not guide steering. */
  uint32_t updated_ms;  /**< HAL tick of the last fully successful scan. */
} IRArrayReading;

/** @brief Configure active pins and calibrate ADC after HAL and clock setup. */
void IRArray_Init(void);
/** @brief Run frequently in main(); acquire a complete scan when due. */
void IRArray_Process(void);
/** @brief Return cached results with freshness checked; foreground-only. */
IRArrayReading IRArray_GetReading(void);
/* Exactly one byte: [five-sensor][valid][reserved=0][sensor bits].
 * Invalid packets retain the count flag and clear every sensor bit. */
uint8_t IRArray_GetPacket(void);
#endif
