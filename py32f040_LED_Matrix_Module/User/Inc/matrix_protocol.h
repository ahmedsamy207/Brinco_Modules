/** @file matrix_protocol.h @brief Fixed 160-byte SPI application packets, v1.
 *  All functions are foreground-only. Transport freezes replies during CS low.
 */
#ifndef BRINCO_MATRIX_PROTOCOL_H
#define BRINCO_MATRIX_PROTOCOL_H
#include <stdint.h>
#include "matrix_config.h"
enum {
  MATRIX_FETCH=0x00, MATRIX_INFO=0x01, MATRIX_STATUS=0x02,
  MATRIX_FRAME=0x10, MATRIX_FILL=0x11, MATRIX_PIXEL=0x12,
  MATRIX_CLEAR=0x13, MATRIX_BRIGHTNESS=0x14
};
enum {
  MATRIX_OK=0, MATRIX_BAD_CRC=1, MATRIX_BAD_FORMAT=2, MATRIX_BAD_COMMAND=3,
  MATRIX_BAD_LENGTH=4, MATRIX_BAD_RANGE=5, MATRIX_BUSY=6, MATRIX_OUTPUT_ERROR=7
};
void MatrixProtocol_Init(void);
/** @brief Validate a complete packet before changing any pixel/brightness state. */
void MatrixProtocol_Handle(const uint8_t *request);
/** @brief Build a frozen reply for the next transaction, with live busy/error flags. */
void MatrixProtocol_BuildReply(uint8_t *reply);
void MatrixProtocol_TransportError(void);
/** @brief CRC-16/CCITT-FALSE: polynomial 0x1021, initial 0xFFFF, no reflection. */
uint16_t MatrixProtocol_CRC(const uint8_t *data, uint32_t length);
#endif
