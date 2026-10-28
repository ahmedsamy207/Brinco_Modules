/** @file motor_io.c
 * AT8833: AIN1/2=PB4/5, BIN1/2=PB0/1, nSLEEP=PB6, nFAULT=PB7.
 * TIM6 update IRQ at20kHz from72MHz APB. No STEP/DIR translator is fitted.
 * PA1..10 are analog-only here: physically connected sense/OPA nets must never
 * become conflicting GPIO outputs. OPA/ADC are not enabled in this version.
 */
#include "main.h"
#include "motion.h"
#define BRIDGE_PINS (GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5)
static TIM_HandleTypeDef timer;
void MotorIO_InitSafe(void)
{
 GPIO_InitTypeDef g={0};__HAL_RCC_GPIOB_CLK_ENABLE();__HAL_RCC_GPIOA_CLK_ENABLE();
 /* Preload output latches low before changing pins out of reset input mode. */
 GPIOB->BSRR=(uint32_t)(BRIDGE_PINS|GPIO_PIN_6)<<16;
 g.Pin=BRIDGE_PINS|GPIO_PIN_6;g.Mode=GPIO_MODE_OUTPUT_PP;g.Pull=GPIO_PULLDOWN;
 g.Speed=GPIO_SPEED_FREQ_HIGH;HAL_GPIO_Init(GPIOB,&g);
 g.Pin=GPIO_PIN_7;g.Mode=GPIO_MODE_IT_FALLING;g.Pull=GPIO_PULLUP;HAL_GPIO_Init(GPIOB,&g);
 g.Pin=GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10;
 g.Mode=GPIO_MODE_ANALOG;g.Pull=GPIO_NOPULL;HAL_GPIO_Init(GPIOA,&g);
}
void MotorIO_Enable(uint8_t enabled)
{
 if(!enabled) GPIOB->BSRR=(uint32_t)(BRIDGE_PINS|GPIO_PIN_6)<<16;
 else GPIOB->BSRR=GPIO_PIN_6; /* Inputs stay released until the wake delay ends. */
}
void MotorIO_Phase(uint8_t phase)
{
 static const uint16_t pattern[4]={GPIO_PIN_4|GPIO_PIN_0,GPIO_PIN_5|GPIO_PIN_0,
                                 GPIO_PIN_5|GPIO_PIN_1,GPIO_PIN_4|GPIO_PIN_1};
 uint32_t bits=pattern[phase&3U];
 /* One port write changes both coils together; no software inter-pin gap. */
 GPIOB->BSRR=((BRIDGE_PINS&~bits)<<16)|bits;
}
uint8_t MotorIO_Fault(void) { return (GPIOB->IDR&GPIO_PIN_7)==0; }
void MotorIO_StartTimer(void)
{
 __HAL_RCC_TIM6_CLK_ENABLE();
 timer.Instance=TIM6;timer.Init.Prescaler=71;timer.Init.Period=49;
 timer.Init.CounterMode=TIM_COUNTERMODE_UP;timer.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
 if(HAL_TIM_Base_Init(&timer)!=HAL_OK) APP_ErrorHandler();
 HAL_NVIC_SetPriority(EXTI4_15_IRQn,0,0);HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
 HAL_NVIC_SetPriority(TIM6_LPTIM1_IRQn,1,0);HAL_NVIC_EnableIRQ(TIM6_LPTIM1_IRQn);
 __HAL_TIM_CLEAR_FLAG(&timer,TIM_FLAG_UPDATE);
 if(HAL_TIM_Base_Start_IT(&timer)!=HAL_OK) APP_ErrorHandler();
}
/** Timer ISR never waits on UART/HAL transactions and never parses commands. */
void TIM6_LPTIM1_IRQHandler(void)
{
 if(TIM6->SR&TIM_SR_UIF) { TIM6->SR=~TIM_SR_UIF;Motion_Tick(); }
}
/** Latch a falling nFAULT immediately; do not resume when the driver retries. */
void EXTI4_15_IRQHandler(void)
{
 if(EXTI->PR&GPIO_PIN_7) { EXTI->PR=GPIO_PIN_7;Motion_Trip(FAULT_DRIVER); }
}
