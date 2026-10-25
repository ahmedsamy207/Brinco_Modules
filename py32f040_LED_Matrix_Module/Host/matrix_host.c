/** @file matrix_host.c @brief Packet helper, not a complete ESP32 application.
 *  Example calls: FILL {r,g,b}, PIXEL {index,r,g,b}, FRAME 144 RGB bytes,
 *  BRIGHTNESS {0..255}, CLEAR/INFO/STATUS no data. FETCH is handled internally.
 *  Serialize callers: one outstanding command per SPI device at a time.
 */
#include "matrix_host.h"
#include <string.h>

static uint16_t CRC(const uint8_t *bytes)
{
  uint16_t crc=0xffffU;
  for (uint32_t i=0;i<158U;++i) {
    crc^=(uint16_t)bytes[i]<<8U;
    for (uint8_t bit=0;bit<8U;++bit) crc=(crc&0x8000U)?(uint16_t)((crc<<1U)^0x1021U):(uint16_t)(crc<<1U);
  }
  return crc;
}
static void Pack(uint8_t *frame,uint8_t command,uint8_t sequence,const uint8_t *data,uint8_t length)
{
  uint16_t crc;
  memset(frame,0,MATRIX_SPI_FRAME_BYTES);
  frame[0]=0x42U;frame[1]=0x4dU;frame[2]=1U;frame[3]=sequence;frame[4]=command;frame[5]=length;
  if (length) memcpy(frame+8U,data,length);
  crc=CRC(frame);frame[158]=(uint8_t)(crc>>8U);frame[159]=(uint8_t)crc;
}

bool MatrixHost_Command(MatrixHost *host,uint8_t command,const uint8_t *data,
                        uint8_t length,uint8_t *reply)
{
  uint8_t request[MATRIX_SPI_FRAME_BYTES],fetch[MATRIX_SPI_FRAME_BYTES];
  uint8_t sequence;
  if (!host || !reply || !host->exchange || !host->wait_us ||
      command==MATRIX_FETCH || length>MATRIX_RGB_BYTES || (length && !data)) return false;
  sequence=host->next_sequence++;
  Pack(request,command,sequence,data,length);
  Pack(fetch,MATRIX_FETCH,0U,0,0U);
  host->wait_us(host->context,2000U);
  if (!host->exchange(host->context,request,reply)) return false;
  for (uint8_t poll=0;poll<8U;++poll) {
    uint16_t crc;
    host->wait_us(host->context,2000U);
    if (!host->exchange(host->context,fetch,reply)) return false;
    crc=CRC(reply);
    if (reply[0]==0x4dU && reply[1]==0x42U && reply[2]==1U &&
        reply[3]==sequence && reply[4]==command && reply[7]<=MATRIX_RGB_BYTES &&
        reply[158]==(uint8_t)(crc>>8U) && reply[159]==(uint8_t)crc) return true;
  }
  return false;
}
