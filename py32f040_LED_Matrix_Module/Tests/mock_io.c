/* Link real application/transport/encoding code against controlled test IO.
 * LED DMA and physical SPI shifting are NOT simulated; test boundaries/commands.
 */
#include "main.h"
#include "matrix_protocol.h"
#include "led_waveform.h"
#include "led_output.h"
#include "spi_transport.h"
#include "../Host/matrix_host.h"
#include <string.h>
SPI_TypeDef test_spi;
EXTI_TypeDef test_exti;
uint8_t test_request[160],test_reply[160],test_rgb[144],test_pixels[144];
uint16_t test_waveform[MATRIX_WAVEFORM_WORDS];
static uint32_t tick,cs,shows,last_brightness;
static uint8_t busy,error,fail_start;
static uint8_t corrupt_fetch;
static uint32_t exchanges;
static MatrixHost host;
void Test_SPIReset(void) { memset(&test_spi,0,sizeof(test_spi)); }
uint32_t HAL_GetTick(void) { return tick; }
uint32_t HAL_GPIO_ReadPin(void *port,uint32_t pin) { (void)port;(void)pin;return cs; }
void HAL_GPIO_Init(void *port,GPIO_InitTypeDef *gpio) { (void)port;(void)gpio; }
void HAL_NVIC_SetPriority(uint32_t irq,uint32_t priority,uint32_t sub) { (void)irq;(void)priority;(void)sub; }
void HAL_NVIC_EnableIRQ(uint32_t irq) { (void)irq; }
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *spi) { spi->Instance->CR1=0U;spi->Instance->CR2=0U;return HAL_OK; }
void APP_ErrorHandler(void) { for (;;) {} }
uint8_t LEDOutput_IsBusy(void) { return busy; }
uint8_t LEDOutput_HasError(void) { return error; }
uint8_t LEDOutput_Start(const uint8_t *pixels,uint8_t brightness)
{
  if (busy || error || fail_start) return 0U;
  memcpy(test_pixels,pixels,144U);last_brightness=brightness;++shows;return 1U;
}
void Test_Init(void)
{
  cs=1U;tick=0U;shows=0U;busy=0U;error=0U;fail_start=0U;
  MatrixProtocol_Init();SPITransport_Init();
}
void Test_Command(void) { MatrixProtocol_Handle(test_request);MatrixProtocol_BuildReply(test_reply); }
void Test_Reply(void) { MatrixProtocol_BuildReply(test_reply); }
void Test_Encode(uint32_t brightness) { LEDWaveform_Encode(test_rgb,(uint8_t)brightness,test_waveform); }
uint32_t Test_CRC(void) { return MatrixProtocol_CRC(test_request,9U); }
uint32_t Test_Shows(void) { return shows; }
uint32_t Test_Brightness(void) { return last_brightness; }
void Test_Busy(uint32_t value) { busy=value; }
void Test_Error(uint32_t value) { error=value; }
void Test_Fail(uint32_t value) { fail_start=value; }
void Test_TransportError(void) { MatrixProtocol_TransportError(); }
void Test_Time(uint32_t value) { tick=value; }
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
  if (tx[4]==MATRIX_FETCH && corrupt_fetch) { rx[158]^=1U;--corrupt_fetch; }
  return true;
}
static void HostWait(void *context,uint32_t microseconds)
{ (void)context;tick+=microseconds/1000U;SPITransport_Process(); }
void Test_CorruptFetch(uint32_t value) { corrupt_fetch=(uint8_t)value; }
uint32_t Test_Exchanges(void) { return exchanges; }
uint32_t Test_Host(void)
{
  host.exchange=HostExchange;host.wait_us=HostWait;
  return MatrixHost_Command(&host,test_request[4],test_request+8U,test_request[5],test_reply);
}
