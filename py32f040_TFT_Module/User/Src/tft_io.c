/** @file tft_io.c
 *  @brief MAR2406 ILI9341 write-only parallel IO and four-wire resistive touch.
 *  Initialization values adapted from lcdwiki/LCDWIKI_kbv (MIT); see
 *  ThirdParty/LCDWIKI_LICENSE.txt. The HAL/board glue and touch sequencing
 *  here target the PY32. Touch excitation follows the LCDWIKI TouchScreen
 *  reference, (c) ladyada/adafruit (MIT); see ThirdParty/TOUCHSCREEN_LICENSE.txt.
 *  All entry points are foreground-only; SPI IRQs
 *  never change LCD or touch pins. No interrupt mask spans a touch scan.
 */
#include "tft_board.h"
#include "tft_config.h"
#include "tft_io.h"
static TIM_HandleTypeDef delay_timer;
static ADC_HandleTypeDef adc;
static uint8_t adc_ok;

/** @brief TIM3 runs continuously at 1 MHz, no interrupt, wrap-safe short delays. */
static void DelayUs(uint16_t us)
{
  uint16_t start=(uint16_t)__HAL_TIM_GET_COUNTER(&delay_timer);
  while ((uint16_t)((uint16_t)__HAL_TIM_GET_COUNTER(&delay_timer)-start)<us) {}
}
static void Pin(GPIO_TypeDef *port,uint16_t pin,uint8_t high)
{ HAL_GPIO_WritePin(port,pin,high?GPIO_PIN_SET:GPIO_PIN_RESET); }
static void Mode(GPIO_TypeDef *port,uint16_t pins,uint32_t mode)
{
  GPIO_InitTypeDef gpio={0};
  gpio.Pin=pins;gpio.Mode=mode;gpio.Pull=GPIO_NOPULL;gpio.Speed=GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(port,&gpio);
}
/** @brief Restore latches before output modes; WR/RD/CS remain inactive HIGH. */
static void RestoreLCD(void)
{
  /* Equal levels on the four touch electrodes avoid continuous sheet
   * current between operations. Data changes are harmless with WR/RD HIGH. */
  Pin(GPIOB,TFT_RS_PIN|TFT_WR_PIN|TFT_CS_PIN|TFT_RESET_PIN,1);
  Pin(GPIOA,TFT_RD_PIN|GPIO_PIN_0|GPIO_PIN_1,1);
  Mode(GPIOA,TFT_DATA_MASK|TFT_RD_PIN,GPIO_MODE_OUTPUT_PP);
  Mode(GPIOB,TFT_RS_PIN|TFT_WR_PIN|TFT_CS_PIN|TFT_RESET_PIN,GPIO_MODE_OUTPUT_PP);
}
static void WriteByte(uint8_t value)
{
  /* Only PA0..7 change. Preserve PA11 RD, PA15 host NSS and SWD pins. */
  GPIOA->BSRR=((uint32_t)(TFT_DATA_MASK & ~value)<<16U)|value;
  DelayUs(1);
  Pin(GPIOB,TFT_WR_PIN,0);DelayUs(1);
  Pin(GPIOB,TFT_WR_PIN,1);DelayUs(1);
}
static void Command(uint8_t command,const uint8_t *data,uint8_t count)
{
  Pin(GPIOB,TFT_CS_PIN,0);Pin(GPIOB,TFT_RS_PIN,0);WriteByte(command);
  Pin(GPIOB,TFT_RS_PIN,1);
  for(uint8_t i=0;i<count;++i) WriteByte(data[i]);
  Pin(GPIOB,TFT_CS_PIN,1);
  Pin(GPIOA,GPIO_PIN_0|GPIO_PIN_1,1); /* Park XP/YM HIGH alongside XM/YP. */
}
void TFTIO_SetRotation(uint8_t rotation)
{
  static const uint8_t madctl[]={0x48,0x28,0x88,0xe8};
  Command(0x36,&madctl[rotation],1U);
}
void TFTIO_Row(uint16_t x,uint16_t y,uint16_t count,uint16_t color,const uint16_t *pixels)
{
  uint16_t end=x+count-1U;
  uint8_t columns[]={x>>8U,(uint8_t)x,end>>8U,(uint8_t)end};
  uint8_t rows[]={y>>8U,(uint8_t)y,y>>8U,(uint8_t)y};
  Command(0x2a,columns,4);Command(0x2b,rows,4);
  Pin(GPIOB,TFT_CS_PIN,0);Pin(GPIOB,TFT_RS_PIN,0);WriteByte(0x2c);Pin(GPIOB,TFT_RS_PIN,1);
  for(uint16_t i=0;i<count;++i) {
    uint16_t value=pixels?pixels[i]:color;
    WriteByte((uint8_t)(value>>8U));WriteByte((uint8_t)value);
  }
  Pin(GPIOB,TFT_CS_PIN,1);
  Pin(GPIOA,GPIO_PIN_0|GPIO_PIN_1,1);
}

void TFTIO_Init(void)
{
  /* Command,count,data records; 0xff means delay milliseconds, 0xfe ends. */
  static const uint8_t sequence[]={
    0x01,0,0xff,50,0x28,0,0xf6,3,1,1,0,0xcf,3,0,0x81,0x30,
    0xed,4,0x64,3,0x12,0x81,0xe8,3,0x85,0x10,0x78,
    0xcb,5,0x39,0x2c,0,0x34,2,0xf7,1,0x20,0xea,2,0,0,
    0xb0,1,0,0xb4,1,0,0xc0,1,0x21,0xc1,1,0x11,
    0xc5,2,0x3f,0x3c,0xc7,1,0xb5,0x3a,1,0x55,0xb1,2,0,0x1b,
    0x36,1,0x48,0xf2,1,0,0x26,1,1,
    0xe0,15,0x0f,0x26,0x24,0x0b,0x0e,9,0x54,0xa8,0x46,0x0c,0x17,9,0x0f,7,0,
    0xe1,15,0,0x19,0x1b,4,0x10,7,0x2a,0x47,0x39,3,6,6,0x30,0x38,0x0f,
    0xb7,1,7,0x11,0,0xff,150,0x29,0,0xfe
  };
  uint32_t i=0;
  __HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_GPIOB_CLK_ENABLE();__HAL_RCC_TIM3_CLK_ENABLE();
  delay_timer.Instance=TIM3;delay_timer.Init.Prescaler=71;delay_timer.Init.Period=65535;
  delay_timer.Init.CounterMode=TIM_COUNTERMODE_UP;delay_timer.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
  if(HAL_TIM_Base_Init(&delay_timer)!=HAL_OK || HAL_TIM_Base_Start(&delay_timer)!=HAL_OK) APP_ErrorHandler();
  RestoreLCD();
  /* Park the unused SD device deselected; no SPI2 clock or SD driver. */
  Pin(GPIOB,TFT_SD_CS_PIN,1);Mode(GPIOB,TFT_SD_CS_PIN,GPIO_MODE_OUTPUT_PP);
  Pin(GPIOB,TFT_RESET_PIN,0);HAL_Delay(20);Pin(GPIOB,TFT_RESET_PIN,1);HAL_Delay(150);
  while(sequence[i]!=0xfe) {
    uint8_t command=sequence[i++],count=sequence[i++];
    if(command==0xff) HAL_Delay(count);
    else { Command(command,sequence+i,count);i+=count; }
  }
  __HAL_RCC_ADC_CLK_ENABLE();
  __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_PCLK_DIV8); /* 72 / 8 = 9 MHz. */
  adc.Instance=ADC1;adc.Init.Resolution=ADC_RESOLUTION_12B;adc.Init.DataAlign=ADC_DATAALIGN_RIGHT;
  adc.Init.ScanConvMode=ADC_SCAN_DISABLE;adc.Init.NbrOfConversion=1;
  adc.Init.ContinuousConvMode=DISABLE;adc.Init.DiscontinuousConvMode=DISABLE;
  adc.Init.ExternalTrigConv=ADC_SOFTWARE_START;
  adc_ok=HAL_ADC_Init(&adc)==HAL_OK && HAL_ADCEx_Calibration_Start(&adc)==HAL_OK;
}

/** @brief All panel electrodes high-Z before switching excitation direction. */
static void FloatTouch(void)
{
  Mode(TFT_TOUCH_XP_PORT,TFT_TOUCH_XP_PIN|TFT_TOUCH_YM_PIN,GPIO_MODE_ANALOG);
  Mode(TFT_TOUCH_XM_PORT,TFT_TOUCH_XM_PIN|TFT_TOUCH_YP_PIN,GPIO_MODE_ANALOG);
}
static void Drive(GPIO_TypeDef *port,uint16_t pin,uint8_t high)
{ Pin(port,pin,high);Mode(port,pin,GPIO_MODE_OUTPUT_PP); }
static uint8_t Convert(uint32_t channel,uint16_t *value)
{
  ADC_ChannelConfTypeDef config={0};
  uint8_t ok;
  config.Channel=channel;config.Rank=ADC_REGULAR_RANK_1;config.SamplingTime=ADC_SAMPLETIME_239CYCLES_5;
  if(HAL_ADC_ConfigChannel(&adc,&config)!=HAL_OK) return 0;
  ok=HAL_ADC_Start(&adc)==HAL_OK && HAL_ADC_PollForConversion(&adc,1U)==HAL_OK;
  if(ok) *value=(uint16_t)HAL_ADC_GetValue(&adc);
  if(HAL_ADC_Stop(&adc)!=HAL_OK) ok=0;
  return ok;
}
static uint8_t Sample(uint32_t channel,uint16_t *value,uint8_t *stable)
{
  uint16_t a,b,c,discard,t;
  /* Discard charge from the previous channel, then median-filter three reads. */
  if(!Convert(channel,&discard)||!Convert(channel,&a)||!Convert(channel,&b)||!Convert(channel,&c)) return 0;
  if(a>b){t=a;a=b;b=t;}if(b>c){t=b;b=c;c=t;}if(a>b){t=a;a=b;b=t;}
  if((uint32_t)(c-a)>TFT_TOUCH_SPREAD) *stable=0;
  *value=b;return 1;
}
uint8_t TFTIO_ReadTouch(TFTRawTouch *raw)
{
  uint16_t x,y,z1,z2;
  uint8_t ok=0,stable=1;
  if(!adc_ok) return 0;
  /* WR and RD stay HIGH throughout. CS is itself YP and may float during
   * sensing; inactive strobes prevent LCD transactions even if CS floats LOW. */
  Pin(GPIOB,TFT_WR_PIN|TFT_CS_PIN,1);Pin(GPIOA,TFT_RD_PIN,1);
  FloatTouch();
  Drive(TFT_TOUCH_XP_PORT,TFT_TOUCH_XP_PIN,1);
  Drive(TFT_TOUCH_XM_PORT,TFT_TOUCH_XM_PIN,0);DelayUs(20);
  /* X: excite the X layer, sense floating YP/CS=PB0 through ADC8. */
  if(!Sample(TFT_TOUCH_YP_ADC,&x,&stable)) goto done;
  FloatTouch();
  Drive(TFT_TOUCH_YP_PORT,TFT_TOUCH_YP_PIN,1);
  Drive(TFT_TOUCH_YM_PORT,TFT_TOUCH_YM_PIN,0);DelayUs(20);
  /* Y: excite the Y layer, sense floating XM/RS=PB1 through ADC9. */
  if(!Sample(TFT_TOUCH_XM_ADC,&y,&stable)) goto done;
  FloatTouch();
  Drive(TFT_TOUCH_XP_PORT,TFT_TOUCH_XP_PIN,0);
  Drive(TFT_TOUCH_YM_PORT,TFT_TOUCH_YM_PIN,1);DelayUs(20);
  /* Conventional pressure diagonal: XP LOW / YM HIGH; sense XM then YP.
   * Recalibrate the real panel; this replaces the older complementary scan. */
  if(!Sample(TFT_TOUCH_XM_ADC,&z1,&stable)||!Sample(TFT_TOUCH_YP_ADC,&z2,&stable)) goto done;
  raw->x=4095U-x;raw->y=4095U-y;
  raw->z=z2>=z1?4095U-(z2-z1):0U;raw->stable=stable;ok=1;
done:
  FloatTouch();RestoreLCD(); /* Also restore every shared pin after ADC errors. */
  return ok;
}
