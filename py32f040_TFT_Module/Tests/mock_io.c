/* Run real rendering, touch filtering/calibration, protocol and SPI transport.
 * GPIO/LCD/ADC IO is mocked; the large framebuffer below exists only in tests. */
#include "main.h"
#include "tft_protocol.h"
#include "tft_render.h"
#include "tft_touch.h"
#include "tft_io.h"
#include "spi_transport.h"
#include "../Host/tft_host.h"
#include <string.h>
SPI_TypeDef test_spi;
EXTI_TypeDef test_exti;
uint8_t test_request[160],test_reply[160];
uint16_t test_framebuffer[240U*320U];
TFTRawTouch test_raw;
static uint32_t tick,cs,written,rows,violations,raw_reads,max_row,exchanges;
static uint8_t touch_failure,corrupt_fetch;
static TFTHost host;
void Test_SPIReset(void) { memset(&test_spi,0,sizeof(test_spi)); }
uint32_t HAL_GetTick(void) { return tick; }
uint32_t HAL_GPIO_ReadPin(void *p,uint32_t pin) { (void)p;(void)pin;return cs; }
void HAL_GPIO_Init(void *p,GPIO_InitTypeDef *g) { (void)p;(void)g; }
void HAL_NVIC_SetPriority(uint32_t i,uint32_t p,uint32_t s) { (void)i;(void)p;(void)s; }
void HAL_NVIC_EnableIRQ(uint32_t i) { (void)i; }
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *h) { h->Instance->CR1=h->Instance->CR2=0;return HAL_OK; }
void APP_ErrorHandler(void) { for(;;){} }
void TFTIO_Init(void) {}
void TFTIO_SetRotation(uint8_t r) { (void)r; }
void TFTIO_Row(uint16_t x,uint16_t y,uint16_t n,uint16_t color,const uint16_t *data)
{
  uint16_t w=TFTRender_Width(),h=TFTRender_Height();
  if(!n || x+n>w || y>=h){++violations;return;}
  ++rows;written+=n;if(n>max_row)max_row=n;
  for(uint16_t i=0;i<n;++i)test_framebuffer[(uint32_t)y*w+x+i]=data?data[i]:color;
}
uint8_t TFTIO_ReadTouch(TFTRawTouch *raw)
{ ++raw_reads;*raw=test_raw;return !touch_failure; }
void Test_Flush(void)
{ for(uint32_t i=0;i<2000U && TFTRender_Busy();++i)TFTRender_Process(); }
void Test_Init(void)
{
  cs=1;tick=written=rows=violations=raw_reads=max_row=exchanges=0;touch_failure=corrupt_fetch=0;
  test_raw=(TFTRawTouch){0};memset(&host,0,sizeof(host));
  TFTRender_Init();Test_Flush();written=rows=max_row=0;
  TFTTouch_Init();TFTProtocol_Init();SPITransport_Init();
}
void Test_Command(void) { TFTProtocol_Handle(test_request);TFTProtocol_BuildReply(test_reply); }
void Test_Reply(void) { TFTProtocol_BuildReply(test_reply); }
void Test_DrawStep(void) { TFTRender_Process(); }
void Test_TouchStep(uint32_t now) { tick=now;TFTTouch_Process(); }
void Test_Time(uint32_t now) { tick=now; }
void Test_TouchFailure(uint32_t f) { touch_failure=(uint8_t)f; }
void Test_TransportError(void) { TFTProtocol_TransportError(); }
uint32_t Test_CRC(void) { return TFTProtocol_CRC(test_request,9U); }
uint32_t Test_Field(uint32_t field)
{
  TFTTouchReading t=TFTTouch_GetReading();
  switch(field){
    case 0:return TFTRender_Busy();case 1:return written;case 2:return rows;
    case 3:return violations;case 4:return max_row;case 5:return TFTRender_Width();case 6:return TFTRender_Height();
    case 7:return TFTRender_Rotation();case 8:return t.valid;case 9:return t.down;
    case 10:return t.x;case 11:return t.y;case 12:return t.calibrated;case 13:return raw_reads;
    case 14:return t.sequence;default:return 0xffffffffU;
  }
}
void Test_Begin(void) { cs=0U;SPITransport_Process(); }
uint32_t Test_Byte(uint32_t value)
{
  uint32_t output=test_spi.DR & 255U;
  test_spi.DR=value;test_spi.SR=SPI_SR_RXNE|SPI_SR_TXE;
  SPITransport_IRQHandler();test_spi.SR=SPI_SR_TXE;
  return output;
}
void Test_EndOnly(void)
{ cs=1U;test_exti.PR=GPIO_PIN_15;SPITransport_NSS_IRQHandler();test_exti.PR=0U; }
void Test_Process(void) { SPITransport_Process(); }
void Test_End(void) { Test_EndOnly();SPITransport_Process(); }
void Test_SPIError(void)
{ test_spi.SR=SPI_SR_OVR|SPI_SR_RXNE;SPITransport_IRQHandler();test_spi.SR=SPI_SR_TXE; }

static bool HostExchange(void *context,const uint8_t *tx,uint8_t *rx)
{
  (void)context;++exchanges;
  Test_Begin();
  for (uint32_t i=0;i<160U;++i) rx[i]=(uint8_t)Test_Byte(tx[i]);
  Test_End();
  if (tx[4]==TFT_FETCH && corrupt_fetch) { rx[158]^=1U;--corrupt_fetch; }
  return true;
}
static void HostWait(void *context,uint32_t microseconds)
{ (void)context;tick+=microseconds/1000U;SPITransport_Process(); }
void Test_CorruptFetch(uint32_t value) { corrupt_fetch=(uint8_t)value; }
uint32_t Test_Exchanges(void) { return exchanges; }
uint32_t Test_Host(void)
{
  host.exchange=HostExchange;host.wait_us=HostWait;
  return TFTHost_Command(&host,test_request[4],test_request+8U,test_request[5],test_reply);
}
