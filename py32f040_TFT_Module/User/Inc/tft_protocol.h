/** @file tft_protocol.h @brief CRC-protected pipelined SPI commands, version 1. */
#ifndef BRINCO_TFT_PROTOCOL_H
#define BRINCO_TFT_PROTOCOL_H
#include <stdint.h>
#include "tft_config.h"
enum { TFT_FETCH=0,TFT_INFO=1,TFT_STATUS=2,TFT_TOUCH=3,
       TFT_CLEAR=0x10,TFT_RECT=0x11,TFT_PIXELS=0x12,TFT_ROTATE=0x13,
       TFT_LINE=0x14,TFT_CALIBRATE=0x20 };
enum { TFT_OK=0,TFT_BAD_CRC=1,TFT_BAD_FORMAT=2,TFT_BAD_COMMAND=3,
       TFT_BAD_LENGTH=4,TFT_BAD_RANGE=5,TFT_BUSY=6 };
void TFTProtocol_Init(void);
/** @brief Foreground: validate all fields before creating a drawing/calibration job. */
void TFTProtocol_Handle(const uint8_t *request);
void TFTProtocol_BuildReply(uint8_t *reply);
void TFTProtocol_TransportError(void);
uint16_t TFTProtocol_CRC(const uint8_t *data,uint32_t length);
#endif
