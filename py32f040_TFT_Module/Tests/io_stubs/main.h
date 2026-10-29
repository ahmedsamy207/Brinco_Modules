/* GPIO/ADC/timer model for the actual TFT IO driver, not an electrical model. */
#ifndef TFT_IO_TEST_MAIN_H
#define TFT_IO_TEST_MAIN_H
#include <stdint.h>
typedef enum { HAL_OK=0,HAL_ERROR=1 } HAL_StatusTypeDef;
typedef struct { uint32_t BSRR,ODR; } GPIO_TypeDef;
extern GPIO_TypeDef io_gpioa,io_gpiob;
#define GPIOA (&io_gpioa)
#define GPIOB (&io_gpiob)
#define GPIO_PIN_0 1U
#define GPIO_PIN_1 2U
#define GPIO_PIN_3 8U
#define GPIO_PIN_4 16U
#define GPIO_PIN_5 32U
#define GPIO_PIN_8 256U
#define GPIO_PIN_9 512U
#define GPIO_PIN_11 2048U
#define GPIO_PIN_12 4096U
#define GPIO_PIN_13 8192U
#define GPIO_PIN_14 16384U
#define GPIO_PIN_15 32768U
#define GPIO_PIN_SET 1U
#define GPIO_PIN_RESET 0U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_ANALOG 2U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_HIGH 1U
#define GPIO_AF0_SPI1 0U
typedef struct { uint32_t Pin,Mode,Pull,Speed,Alternate; } GPIO_InitTypeDef;
typedef struct { void *Instance;struct { uint32_t Prescaler,Period,CounterMode,ClockDivision; } Init; } TIM_HandleTypeDef;
#define TIM3 ((void *)3)
#define TIM_COUNTERMODE_UP 0U
#define TIM_CLOCKDIVISION_DIV1 0U
#define __HAL_TIM_GET_COUNTER(h) IO_TimerTick()
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM3_CLK_ENABLE() ((void)0)
#define __HAL_RCC_ADC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_ADC_CONFIG(x) ((void)(x))
#define RCC_ADCCLKSOURCE_PCLK_DIV8 8U
#define ADC1 ((void *)4)
#define ADC_RESOLUTION_12B 12U
#define ADC_DATAALIGN_RIGHT 0U
#define ADC_SCAN_DISABLE 0U
#define ADC_SOFTWARE_START 0U
#define DISABLE 0U
#define ADC_CHANNEL_8 8U
#define ADC_CHANNEL_9 9U
#define ADC_REGULAR_RANK_1 1U
#define ADC_SAMPLETIME_239CYCLES_5 239U
typedef struct {
 void *Instance;
 struct { uint32_t Resolution,DataAlign,ScanConvMode,NbrOfConversion,ContinuousConvMode,DiscontinuousConvMode,ExternalTrigConv; } Init;
} ADC_HandleTypeDef;
typedef struct { uint32_t Channel,Rank,SamplingTime; } ADC_ChannelConfTypeDef;
uint32_t IO_TimerTick(void);
void HAL_GPIO_WritePin(GPIO_TypeDef *,uint16_t,uint32_t);
void HAL_GPIO_Init(GPIO_TypeDef *,GPIO_InitTypeDef *);
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *);
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *);
void HAL_Delay(uint32_t);
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *,ADC_ChannelConfTypeDef *);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *,uint32_t);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *);
void APP_ErrorHandler(void);
#endif
