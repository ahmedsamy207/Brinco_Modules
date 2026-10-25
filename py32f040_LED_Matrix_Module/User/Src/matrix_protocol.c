/** @file matrix_protocol.c
 *  @brief Atomic RGB commands and pipelined replies for the future ESP32 host.
 *  RGB state is independent of the DMA waveform; rejected/corrupt/busy commands
 *  leave it unchanged. Successful update means transmission started, not that
 *  light output was optically verified. FETCH preserves the last command ACK.
 */
#include "matrix_protocol.h"
#include "led_output.h"
#include <string.h>

static uint8_t pixels[MATRIX_RGB_BYTES];
static uint8_t brightness;
static uint8_t ack_sequence, ack_command, ack_status, ack_length;
static uint8_t ack_data[4];
static uint8_t transport_error;

uint16_t MatrixProtocol_CRC(const uint8_t *data, uint32_t length)
{
  uint16_t crc = 0xFFFFU;
  for (uint32_t i=0U; i<length; ++i) {
    crc ^= (uint16_t)data[i] << 8U;
    for (uint8_t bit=0U; bit<8U; ++bit) {
      crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
    }
  }
  return crc;
}

void MatrixProtocol_Init(void)
{
  memset(pixels, 0, sizeof(pixels));
  brightness = MATRIX_DEFAULT_BRIGHTNESS;
  ack_sequence = 0xFFU;
  ack_command = 0xFFU; /* Startup identity cannot match a normal command ACK. */
  ack_status = MATRIX_OK;
  ack_length = 0U;
  transport_error = 0U;
  if (!LEDOutput_Start(pixels, brightness)) ack_status = MATRIX_OUTPUT_ERROR;
}

void MatrixProtocol_TransportError(void) { transport_error = 1U; }

void MatrixProtocol_Handle(const uint8_t *request)
{
  uint8_t sequence = request[3], command = request[4], length = request[5];
  uint16_t received_crc = ((uint16_t)request[158] << 8U) | request[159];
  uint8_t status = MATRIX_OK;
  uint8_t next_pixels[MATRIX_RGB_BYTES];
  uint8_t next_brightness = brightness;
  const uint8_t *data = request + 8U;

  if (received_crc != MatrixProtocol_CRC(request, 158U)) status = MATRIX_BAD_CRC;
  else if (request[0]!=0x42U || request[1]!=0x4DU || request[2]!=1U ||
           request[6]!=0U || request[7]!=0U) status = MATRIX_BAD_FORMAT;
  else if (length > MATRIX_RGB_BYTES) status = MATRIX_BAD_LENGTH;
  else {
    /* Canonical zero padding catches host length/serialization mistakes. */
    for (uint32_t i=8U+length; i<158U; ++i) {
      if (request[i]!=0U) { status = MATRIX_BAD_FORMAT; break; }
    }
  }
  if (status == MATRIX_OK && command == MATRIX_FETCH && length == 0U) return;
  ack_sequence=sequence; ack_command=command; ack_status=status; ack_length=0U;
  if (status != MATRIX_OK) return;
  transport_error = 0U;
  if (command == MATRIX_INFO || command == MATRIX_STATUS) {
    if (length!=0U) { ack_status=MATRIX_BAD_LENGTH; return; }
    ack_data[0]=MATRIX_LEDS; ack_data[1]=3U; ack_data[2]=brightness; ack_data[3]=1U;
    ack_length=4U; /* count, bytes/pixel, brightness, GRB wire-order identifier. */
    return;
  }
  if (command == MATRIX_FETCH) { ack_status=MATRIX_BAD_LENGTH; return; }
  memcpy(next_pixels,pixels,sizeof(pixels));
  switch (command) {
    case MATRIX_FRAME:
      if (length!=MATRIX_RGB_BYTES) { ack_status=MATRIX_BAD_LENGTH; return; }
      memcpy(next_pixels,data,sizeof(next_pixels));
      break;
    case MATRIX_FILL:
      if (length!=3U) { ack_status=MATRIX_BAD_LENGTH; return; }
      for (uint32_t i=0U;i<MATRIX_LEDS;++i) memcpy(next_pixels+i*3U,data,3U);
      break;
    case MATRIX_PIXEL:
      if (length!=4U) { ack_status=MATRIX_BAD_LENGTH; return; }
      if (data[0]>=MATRIX_LEDS) { ack_status=MATRIX_BAD_RANGE; return; }
      memcpy(next_pixels+data[0]*3U,data+1U,3U);
      break;
    case MATRIX_CLEAR:
      if (length!=0U) { ack_status=MATRIX_BAD_LENGTH; return; }
      memset(next_pixels,0,sizeof(next_pixels));
      break;
    case MATRIX_BRIGHTNESS:
      if (length!=1U) { ack_status=MATRIX_BAD_LENGTH; return; }
      next_brightness=data[0];
      break;
    default: ack_status=MATRIX_BAD_COMMAND; return;
  }
  if (LEDOutput_HasError()) { ack_status=MATRIX_OUTPUT_ERROR; return; }
  if (LEDOutput_IsBusy()) { ack_status=MATRIX_BUSY; return; }
  if (!LEDOutput_Start(next_pixels,next_brightness)) { ack_status=MATRIX_OUTPUT_ERROR; return; }
  memcpy(pixels,next_pixels,sizeof(pixels));
  brightness=next_brightness;
}

void MatrixProtocol_BuildReply(uint8_t *reply)
{
  uint16_t crc;
  memset(reply,0,MATRIX_SPI_FRAME_BYTES);
  reply[0]=0x4DU; reply[1]=0x42U; reply[2]=1U;
  reply[3]=ack_sequence; reply[4]=ack_command; reply[5]=ack_status;
  reply[6]=(LEDOutput_IsBusy()?1U:0U) | (LEDOutput_HasError()?2U:0U) | (transport_error?4U:0U);
  reply[7]=ack_length;
  memcpy(reply+8U,ack_data,ack_length);
  crc=MatrixProtocol_CRC(reply,158U);
  reply[158]=(uint8_t)(crc>>8U); reply[159]=(uint8_t)crc;
}
