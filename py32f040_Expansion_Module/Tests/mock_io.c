/** Test HAL/register model. Runs real routing, queues and both SPI drivers.
 * Deliberately does not model analog signal levels, FIFOs or cycle deadlines. */
#include "main.h"
#include "hub.h"
#include "spi_transport.h"
#include <string.h>
SPI_TypeDef test_spi,test_spi2;
EXTI_TypeDef test_exti;
USART_TypeDef test_usart[4];
uint8_t test_request[64],test_reply[64],test_output[4][2048];
uint32_t test_output_count[4];
static UART_HandleTypeDef *handles[4];
static uint8_t *dest[4],transmitting[4];
static uint32_t tick,cs=1,cs2=1,tx_fail;
static uint8_t Index(UART_HandleTypeDef *h) { return (uint8_t)(h->Instance-test_usart); }
uint32_t HAL_GetTick(void) { return tick; }
void Test_SPIReset(void) { memset(&test_spi,0,sizeof test_spi); }
void Test_SPI2Reset(void) { memset(&test_spi2,0,sizeof test_spi2); }
uint32_t HAL_GPIO_ReadPin(void *p,uint32_t n) { (void)p;(void)n;return cs; }
void HAL_GPIO_WritePin(void *p,uint32_t n,uint32_t v) { (void)p;(void)n;cs2=v; }
void HAL_GPIO_Init(void *p,GPIO_InitTypeDef *g) { (void)p;(void)g; }
void HAL_NVIC_SetPriority(uint32_t a,uint32_t b,uint32_t c) { (void)a;(void)b;(void)c; }
void HAL_NVIC_EnableIRQ(uint32_t a) { (void)a; }
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *h) { h->Instance->CR1=h->Instance->CR2=0;return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *h) { handles[Index(h)]=h;return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h,uint8_t *p,uint16_t n)
{ (void)n;dest[Index(h)]=p;h->RxState=1;return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h,uint8_t *p,uint16_t n)
{
 uint8_t i=Index(h);(void)n;if(tx_fail)return HAL_ERROR;
 if(test_output_count[i]<2048)test_output[i][test_output_count[i]++]=*p;
 transmitting[i]=1;return HAL_OK;
}
void HAL_UART_IRQHandler(UART_HandleTypeDef *h) { (void)h; }
void APP_ErrorHandler(void) { for(;;){} }
void Test_Init(void) { Hub_Init();Ports_Init();Downstream_Init();SPITransport_Init(); }
void Test_Command(void) { Hub_Handle(test_request);Hub_BuildReply(test_reply); }
void Test_Reply(void) { Hub_BuildReply(test_reply); }
void Test_Time(uint32_t t) { tick=t; }
void Test_Step(void) { SPITransport_Process();Ports_Process();Downstream_Process();Hub_Process(); }
void Test_Begin(void) { cs=0;SPITransport_Process(); }
uint32_t Test_Byte(uint32_t v)
{
 uint32_t o=test_spi.DR&255U;test_spi.DR=v;test_spi.SR=SPI_SR_RXNE|SPI_SR_TXE;
 SPITransport_IRQHandler();test_spi.SR=SPI_SR_TXE;return o;
}
void Test_EndOnly(void) { cs=1;test_exti.PR=GPIO_PIN_4;SPITransport_NSS_IRQHandler();test_exti.PR=0; }
void Test_End(void) { Test_EndOnly();SPITransport_Process(); }
void Test_Process(void) { SPITransport_Process(); }
void Test_Drain(void)
{
 for(uint32_t j=0;j<300;j++) {
  Ports_Process();for(uint8_t i=0;i<4;i++) if(transmitting[i]) { transmitting[i]=0;HAL_UART_TxCpltCallback(handles[i]); }
 }
}
void Test_Feed(uint32_t v)
{
 uint8_t i=(uint8_t)(v>>8);*dest[i]=(uint8_t)v;handles[i]->RxState=HAL_UART_STATE_READY;
 HAL_UART_RxCpltCallback(handles[i]);
}
void Test_UARTError(uint32_t i) { handles[i]->RxState=HAL_UART_STATE_READY;HAL_UART_ErrorCallback(handles[i]); }
void Test_TxFail(uint32_t value) { tx_fail=value; }
uint32_t Test_MasterReady(void) { return !cs2 && (test_spi2.CR2&SPI_CR2_RXNEIE); }
uint32_t Test_MasterFirst(void) { test_spi2.SR=SPI_SR_TXE;Downstream_IRQHandler();return test_spi2.DR&255U; }
uint32_t Test_MasterByte(uint32_t v)
{
 test_spi2.DR=v;test_spi2.SR=SPI_SR_RXNE|SPI_SR_TXE;Downstream_IRQHandler();
 test_spi2.SR=SPI_SR_TXE;return test_spi2.DR&255U;
}
void Test_MasterError(void) { test_spi2.SR=SPI_SR_OVR;Downstream_IRQHandler();test_spi2.SR=0; }
