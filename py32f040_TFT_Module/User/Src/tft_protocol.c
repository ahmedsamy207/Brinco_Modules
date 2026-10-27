/** @file tft_protocol.c
 *  @brief Fixed 160-byte BM-style framing with distinct BT/TB TFT signatures.
 *  Sequence/CRC validation, busy rejection and one outstanding drawing job.
 *  Payload uint16/uint32 fields are little-endian; CRC itself is high byte first.
 *  FETCH preserves the previous ACK. Drawing OK means queued, not completed.
 */
#include "main.h"
#include "tft_protocol.h"
#include "tft_render.h"
#include "tft_touch.h"
#include <string.h>
static uint8_t ack_sequence,ack_command,ack_status,ack_length,transport_error;
static uint8_t ack_data[20];
static uint16_t Word(const uint8_t *p) { return (uint16_t)p[0]|((uint16_t)p[1]<<8U); }
static void PutWord(uint8_t *p,uint16_t value) { p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8U); }
static void PutLong(uint8_t *p,uint32_t value)
{ for(uint8_t i=0;i<4;++i)p[i]=(uint8_t)(value>>(i*8U)); }
uint16_t TFTProtocol_CRC(const uint8_t *p,uint32_t length)
{
  uint16_t crc=0xffffU;
  for(uint32_t i=0;i<length;++i) {
    crc^=(uint16_t)p[i]<<8U;
    for(uint8_t bit=0;bit<8;++bit)crc=(crc&0x8000U)?(uint16_t)((crc<<1U)^0x1021U):(uint16_t)(crc<<1U);
  }
  return crc;
}
void TFTProtocol_Init(void)
{ ack_sequence=ack_command=0xffU;ack_status=ack_length=transport_error=0U; }
void TFTProtocol_TransportError(void) { transport_error=1U; }
void TFTProtocol_Handle(const uint8_t *p)
{
  uint8_t command=p[4],length=p[5],status=TFT_OK;
  const uint8_t *d=p+8;
  uint16_t x,y,w,h,color;
  if(TFTProtocol_CRC(p,158U)!=(((uint16_t)p[158]<<8U)|p[159])) status=TFT_BAD_CRC;
  else if(p[0]!=0x42U||p[1]!=0x54U||p[2]!=1U||p[6]!=0U||p[7]!=0U)status=TFT_BAD_FORMAT;
  else if(length>TFT_MAX_PAYLOAD)status=TFT_BAD_LENGTH;
  else for(uint32_t i=8U+length;i<158U;++i)if(p[i]){status=TFT_BAD_FORMAT;break;}
  if(status==TFT_OK && command==TFT_FETCH && length==0U)return;
  ack_sequence=p[3];ack_command=command;ack_status=status;ack_length=0U;
  if(status!=TFT_OK)return;
  transport_error=0U;
  if(command==TFT_INFO||command==TFT_STATUS) {
    if(length){ack_status=TFT_BAD_LENGTH;return;}
    PutWord(ack_data,TFTRender_Width());PutWord(ack_data+2,TFTRender_Height());
    ack_data[4]=TFTRender_Rotation();ack_data[5]=16U;PutWord(ack_data+6,0x9341);
    ack_data[8]=TFT_MAX_ROW_PIXELS;ack_data[9]=1U;ack_length=10U;return;
  }
  if(command==TFT_TOUCH) {
    TFTTouchReading touch;
    if(length){ack_status=TFT_BAD_LENGTH;return;}
    touch=TFTTouch_GetReading();
    ack_data[0]=touch.valid|(touch.down<<1U)|(touch.calibrated<<2U);
    ack_data[1]=TFTRender_Rotation();PutWord(ack_data+2,touch.x);PutWord(ack_data+4,touch.y);
    PutWord(ack_data+6,touch.raw_x);PutWord(ack_data+8,touch.raw_y);PutWord(ack_data+10,touch.z);
    PutLong(ack_data+12,HAL_GetTick()-touch.updated_ms);PutLong(ack_data+16,touch.sequence);
    ack_length=20U;return;
  }
  if(command==TFT_CALIBRATE) {
    if(length!=9U){ack_status=TFT_BAD_LENGTH;return;}
    if(!TFTTouch_Calibrate(Word(d),Word(d+2),Word(d+4),Word(d+6),d[8]))ack_status=TFT_BAD_RANGE;
    return;
  }
  /* Validate before BUSY so malformed commands have deterministic errors. */
  switch(command) {
    case TFT_CLEAR:
      if(length!=2U){ack_status=TFT_BAD_LENGTH;return;}
      if(TFTRender_Busy()){ack_status=TFT_BUSY;return;}
      TFTRender_Rect(0,0,TFTRender_Width(),TFTRender_Height(),Word(d));return;
    case TFT_RECT:
      if(length!=10U){ack_status=TFT_BAD_LENGTH;return;}
      x=Word(d);y=Word(d+2);w=Word(d+4);h=Word(d+6);color=Word(d+8);
      if(!w||!h||(uint32_t)x+w>TFTRender_Width()||(uint32_t)y+h>TFTRender_Height()){
        ack_status=TFT_BAD_RANGE;return;}
      if(TFTRender_Busy()){ack_status=TFT_BUSY;return;}
      TFTRender_Rect(x,y,w,h,color);return;
    case TFT_PIXELS: {
      uint16_t pixels[TFT_MAX_ROW_PIXELS];
      if(length<8U){ack_status=TFT_BAD_LENGTH;return;}
      x=Word(d);y=Word(d+2);w=Word(d+4);
      if(!w||w>TFT_MAX_ROW_PIXELS||(uint32_t)x+w>TFTRender_Width()||y>=TFTRender_Height()){
        ack_status=TFT_BAD_RANGE;return;}
      if(length!=6U+w*2U){ack_status=TFT_BAD_LENGTH;return;}
      if(TFTRender_Busy()){ack_status=TFT_BUSY;return;}
      for(uint16_t i=0;i<w;++i)pixels[i]=Word(d+6U+i*2U);
      TFTRender_Row(x,y,w,pixels);return;
    }
    case TFT_ROTATE:
      if(length!=1U){ack_status=TFT_BAD_LENGTH;return;}
      if(d[0]>3U){ack_status=TFT_BAD_RANGE;return;}
      if(TFTRender_Busy()){ack_status=TFT_BUSY;return;}
      TFTRender_SetRotation(d[0]);return;
    case TFT_LINE:
      if(length!=10U){ack_status=TFT_BAD_LENGTH;return;}
      x=Word(d);y=Word(d+2);w=Word(d+4);h=Word(d+6);color=Word(d+8);
      if(x>=TFTRender_Width()||w>=TFTRender_Width()||y>=TFTRender_Height()||h>=TFTRender_Height()){
        ack_status=TFT_BAD_RANGE;return;}
      if(TFTRender_Busy()){ack_status=TFT_BUSY;return;}
      TFTRender_Line(x,y,w,h,color);return;
    default:ack_status=command==TFT_FETCH?TFT_BAD_LENGTH:TFT_BAD_COMMAND;return;
  }
}
void TFTProtocol_BuildReply(uint8_t *p)
{
  TFTTouchReading touch=TFTTouch_GetReading();
  uint16_t crc;
  memset(p,0,TFT_SPI_FRAME_BYTES);
  p[0]=0x54;p[1]=0x42;p[2]=1;p[3]=ack_sequence;p[4]=ack_command;p[5]=ack_status;
  p[6]=TFTRender_Busy()|(touch.down<<1U)|(transport_error<<2U)|(touch.valid<<3U)|(touch.calibrated<<4U);
  p[7]=ack_length;memcpy(p+8,ack_data,ack_length);
  crc=TFTProtocol_CRC(p,158);p[158]=(uint8_t)(crc>>8U);p[159]=(uint8_t)crc;
}
