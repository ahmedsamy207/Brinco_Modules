/* Minimal test HAL. Production builds use User/Inc/main.h and the real HAL.
 * These stubs model GPIO/time/UART calls, not peripheral register behavior. */
#ifndef STEPPER_TEST_MAIN_H
#define STEPPER_TEST_MAIN_H
#include <stdint.h>
#include <stddef.h>
typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef struct {
  void *Instance;
  struct { uint32_t BaudRate, WordLength, StopBits, Parity, Mode, HwFlowCtl, OverSampling; } Init;
} UART_HandleTypeDef;
#define GPIOA (&test_gpioa)
#define GPIOB (&test_gpiob)
#define USART3 ((void *)2)
#define GPIO_PIN_2 4U
#define GPIO_PIN_4 16U
#define GPIO_PIN_5 32U
#define GPIO_PIN_SET 1U
#define GPIO_MODE_INPUT 0U
#define GPIO_MODE_AF_PP 1U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_HIGH 1U
#define GPIO_AF4_USART3 4U
#define UART_WORDLENGTH_8B 0U
#define UART_STOPBITS_1 0U
#define UART_PARITY_NONE 0U
#define UART_MODE_TX_RX 3U
#define UART_HWCONTROL_NONE 0U
#define UART_OVERSAMPLING_16 0U
#define USART3_4_IRQn 29U
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_USART3_CLK_ENABLE() ((void)0)
#define __HAL_UART_CLEAR_OREFLAG(uart) ((void)(uart))
static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t mask) { (void)mask; }
static inline void __DMB(void) {}
uint32_t HAL_GetTick(void);
uint32_t HAL_GPIO_ReadPin(void *, uint32_t);
void HAL_GPIO_Init(void *, GPIO_InitTypeDef *);
void HAL_NVIC_SetPriority(uint32_t, uint32_t, uint32_t);
void HAL_NVIC_EnableIRQ(uint32_t);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
void HAL_UART_IRQHandler(UART_HandleTypeDef *);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
void APP_ErrorHandler(void);

typedef struct { uint32_t BSRR,IDR; } GPIO_TypeDef;
extern GPIO_TypeDef test_gpioa,test_gpiob;
#define GPIO_PIN_0 1U
#define GPIO_PIN_1 2U
#define GPIO_PIN_3 8U
#define GPIO_PIN_6 64U
#define GPIO_PIN_7 128U
#define GPIO_PIN_8 256U
#define GPIO_PIN_9 512U
#define GPIO_PIN_10 1024U
#define GPIO_PIN_11 2048U
#define GPIO_MODE_OUTPUT_PP 2U
#define GPIO_MODE_IT_FALLING 3U
#define GPIO_MODE_ANALOG 4U
#define GPIO_PULLDOWN 2U
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM6_CLK_ENABLE() ((void)0)
#define EXTI4_15_IRQn 7U
#define TIM6_LPTIM1_IRQn 17U
#define TIM_COUNTERMODE_UP 0U
#define TIM_AUTORELOAD_PRELOAD_DISABLE 0U
#define TIM_SR_UIF 1U
#define TIM_FLAG_UPDATE 1U
#define __HAL_TIM_CLEAR_FLAG(h,f) ((h)->Instance->SR &= ~(f))
typedef struct { uint32_t SR; } TIM_TypeDef;
typedef struct {
 TIM_TypeDef *Instance;
 struct { uint32_t Prescaler,Period,CounterMode,AutoReloadPreload; } Init;
} TIM_HandleTypeDef;
extern TIM_TypeDef test_tim6;
#define TIM6 (&test_tim6)
typedef struct { uint32_t PR; } EXTI_TypeDef;
extern EXTI_TypeDef test_exti;
#define EXTI (&test_exti)
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *);

#define ENABLE 1U
#define DISABLE 0U
#define __HAL_RCC_OPA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_ADC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_DMA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_SYSCFG_CLK_ENABLE() ((void)0)
#define __HAL_RCC_ADC_CONFIG(x) ((void)(x))
#define RCC_ADCCLKSOURCE_PCLK_DIV8 8U
#define OPA ((void *)4)
#define OPA1 0U
#define OPA2 1U
typedef struct { void *Instance; struct { uint32_t Part; } Init; } OPA_HandleTypeDef;
typedef struct { uint32_t CNDTR; } DMA_Channel_TypeDef;
typedef struct { uint32_t ISR; } DMA_TypeDef;
extern DMA_Channel_TypeDef test_dma_channel;
extern DMA_TypeDef test_dma;
#define DMA1_Channel1 (&test_dma_channel)
#define DMA1 (&test_dma)
typedef struct {
 DMA_Channel_TypeDef *Instance;
 struct { uint32_t Direction,PeriphInc,MemInc,PeriphDataAlignment,MemDataAlignment,Mode,Priority; } Init;
} DMA_HandleTypeDef;
typedef struct {
 void *Instance;
 struct { uint32_t Resolution,DataAlign,ScanConvMode,ContinuousConvMode,NbrOfConversion,DiscontinuousConvMode,ExternalTrigConv; } Init;
 DMA_HandleTypeDef *DMA_Handle;
} ADC_HandleTypeDef;
typedef struct { uint32_t Channel,Rank,SamplingTime; } ADC_ChannelConfTypeDef;
#define ADC1 ((void *)5)
#define ADC_RESOLUTION_12B 12U
#define ADC_DATAALIGN_RIGHT 0U
#define ADC_SCAN_ENABLE 1U
#define ADC_SOFTWARE_START 0U
#define ADC_VREFBUF_VCCA 0U
#define ADC_SAMPLETIME_134CYCLES_5 134U
#define ADC_SAMPLETIME_239CYCLES_5 239U
#define ADC_CHANNEL_2 2U
#define ADC_CHANNEL_4 4U
#define ADC_CHANNEL_VREFINT 17U
#define ADC_REGULAR_RANK_1 1U
#define ADC_REGULAR_RANK_2 2U
#define ADC_REGULAR_RANK_3 3U
#define DMA_PERIPH_TO_MEMORY 0U
#define DMA_PINC_DISABLE 0U
#define DMA_MINC_ENABLE 1U
#define DMA_PDATAALIGN_HALFWORD 2U
#define DMA_MDATAALIGN_HALFWORD 2U
#define DMA_CIRCULAR 1U
#define DMA_PRIORITY_HIGH 2U
#define DMA_CHANNEL_MAP_ADC1 0U
#define DMA1_Channel1_IRQn 9U
#define ADC_COMP_IRQn 12U
#define DMA_FLAG_GL1 15U
#define DMA_FLAG_HT1 4U
#define DMA_FLAG_TC1 2U
#define DMA_FLAG_TE1 8U
#define DMA_IT_HT 4U
#define DMA_IT_TC 2U
#define DMA_IT_TE 8U
#define ADC_IT_OVR 1U
#define __HAL_LINKDMA(h,f,d) ((h)->f=&(d))
#define __HAL_DMA_DISABLE(h) Test_StopDMA()
#define __HAL_DMA_DISABLE_IT(h,b) ((void)(b))
#define __HAL_DMA_CLEAR_FLAG(h,b) (test_dma.ISR &= ~(b))
#define __HAL_DMA_GET_COUNTER(h) Test_DMACounter()
#define __HAL_ADC_DISABLE(h) ((void)(h))
#define __HAL_ADC_ENABLE_IT(h,b) Test_ADCInterrupt(1U)
#define __HAL_ADC_DISABLE_IT(h,b) Test_ADCInterrupt(0U)
uint32_t Test_DMACounter(void);
void Test_StopDMA(void);
void Test_ADCInterrupt(uint32_t);
void HAL_Delay(uint32_t);
HAL_StatusTypeDef HAL_OPA_Init(OPA_HandleTypeDef *);
HAL_StatusTypeDef HAL_OPA_Start(OPA_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *);
HAL_StatusTypeDef HAL_ADC_ConfigVrefBuf(ADC_HandleTypeDef *,uint32_t);
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *,ADC_ChannelConfTypeDef *);
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *);
void HAL_DMA_ChannelMap(DMA_HandleTypeDef *,uint32_t);
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *,uint32_t *,uint32_t);
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *);
void HAL_ADC_IRQHandler(ADC_HandleTypeDef *);
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *);
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *);
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *);
#endif
