/** Run real tft_io.c against pin-state and conversion traces.
 * The model checks electrode roles/strobe isolation at every conversion.
 * It does not model panel resistance, ADC settling or GPIO drive currents.
 */
#include "main.h"
#include "tft_io.h"
#include <string.h>
GPIO_TypeDef io_gpioa,io_gpiob;
uint32_t io_modes[2][16],io_channels[32],io_values[16],io_lcd_bytes[4096],io_lcd_rs[4096];
TFTRawTouch io_raw;
static uint32_t calls,violations,lcd_count,microseconds,result,failure,init_failure;
static uint8_t in_scan;
static void Sync(void)
{
 uint32_t bits=io_gpioa.BSRR;
 io_gpioa.ODR=(io_gpioa.ODR&~(bits>>16))|(bits&65535U);io_gpioa.BSRR=0;
 bits=io_gpiob.BSRR;io_gpiob.ODR=(io_gpiob.ODR&~(bits>>16))|(bits&65535U);io_gpiob.BSRR=0;
}
uint32_t IO_TimerTick(void) { Sync();return ++microseconds; }
static uint8_t Output(uint8_t port,uint8_t pin,uint8_t value)
{
 GPIO_TypeDef *gpio=port?GPIOB:GPIOA;
 return io_modes[port][pin]==GPIO_MODE_OUTPUT_PP && !!(gpio->ODR&(1U<<pin))==value;
}
static uint8_t Analog(uint8_t port,uint8_t pin) { return io_modes[port][pin]==GPIO_MODE_ANALOG; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pins,uint32_t value)
{
 Sync();
 if(in_scan && p==GPIOB && (pins&GPIO_PIN_8) && !value) violations++;
 if(p==GPIOB && (pins&GPIO_PIN_8) && !value && !(p->ODR&GPIO_PIN_0) && lcd_count<4096) {
  io_lcd_bytes[lcd_count]=GPIOA->ODR&255U;
  io_lcd_rs[lcd_count++]=!!(GPIOB->ODR&GPIO_PIN_1);
 }
 if(value) p->ODR|=pins;else p->ODR&=~pins;
}
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g)
{
 Sync();uint8_t port=p==GPIOB;
 for(uint8_t i=0;i<16;i++) if(g->Pin&(1U<<i)) io_modes[port][i]=g->Mode;
 if(in_scan && (!Output(1,8,1) || !Output(0,11,1))) violations++;
}
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *h)
{ return h->Init.Prescaler==71 && h->Init.Period==65535?HAL_OK:HAL_ERROR; }
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *h) { (void)h;return HAL_OK; }
void HAL_Delay(uint32_t ms) { microseconds+=ms*1000U; }
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *h) { (void)h;return init_failure?HAL_ERROR:HAL_OK; }
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h) { (void)h;return HAL_OK; }
static uint8_t Fails(uint32_t type) { return (failure>>8)==type && (failure&255U)==calls; }
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *h,ADC_ChannelConfTypeDef *c)
{
 (void)h;Sync();uint32_t index=calls++;
 if(index<32)io_channels[index]=c->Channel;
 uint32_t stage=index/4U;
 if(c->SamplingTime!=239 || c->Rank!=1 || stage>3) violations++;
 if(!Output(1,8,1) || !Output(0,11,1) || !Output(1,9,1))violations++;
 if(stage==0) {
  if(c->Channel!=8 || !Output(0,0,1) || !Output(1,1,0) || !Analog(0,1) || !Analog(1,0))violations++;
 } else if(stage==1) {
  if(c->Channel!=9 || !Output(1,0,1) || !Output(0,1,0) || !Analog(0,0) || !Analog(1,1))violations++;
 } else {
  if(c->Channel!=(stage==2?9U:8U) || !Output(0,0,0) || !Output(0,1,1) || !Analog(1,0) || !Analog(1,1))violations++;
 }
 return Fails(1)?HAL_ERROR:HAL_OK;
}
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h) { (void)h;return Fails(2)?HAL_ERROR:HAL_OK; }
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h,uint32_t ms)
{ (void)h;if(ms!=1)violations++;return Fails(3)?HAL_ERROR:HAL_OK; }
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h) { (void)h;return io_values[(calls-1U)&15U]; }
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h) { (void)h;return Fails(4)?HAL_ERROR:HAL_OK; }
void APP_ErrorHandler(void) { for(;;){} }
void IO_ResetLog(void) { calls=violations=lcd_count=0;failure=0; }
void IO_Init(void)
{
 memset(io_modes,0,sizeof io_modes);GPIOA->ODR=0xe000U;
 TFTIO_Init();IO_ResetLog();
 for(uint8_t i=0;i<4;i++) { io_values[i]=1000;io_values[i+4]=2000;io_values[i+8]=500;io_values[i+12]=3000; }
}
void IO_Failure(uint32_t f) { failure=f; }
void IO_ADCUnavailable(void) { init_failure=1;TFTIO_Init();IO_ResetLog(); }
void IO_Scan(void) { in_scan=1;result=TFTIO_ReadTouch(&io_raw);in_scan=0;Sync(); }
void IO_Row(void) { TFTIO_Row(2,3,1,0x1234,0);Sync(); }
void IO_Rotation(void) { TFTIO_SetRotation(1);Sync(); }
void IO_Time(uint32_t t) { microseconds=t; }
uint32_t IO_Field(uint32_t field)
{
 Sync();
 switch(field) {
  case 0:return result;case 1:return violations;case 2:return calls;case 3:return lcd_count;
  case 4:return microseconds;case 5:return GPIOA->ODR;case 6:return GPIOB->ODR;default:return 0xffffffffU;
 }
}
