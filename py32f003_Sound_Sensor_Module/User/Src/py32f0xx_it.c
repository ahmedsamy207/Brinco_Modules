/**
  ******************************************************************************
  * @file    py32f0xx_it.c
  * @author  Brinco
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Add one xxx_IRQHandler() per enabled interrupt in the matching peripheral
  * section, and delegate to the HAL (for example:
  *   void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }
  * ). Remember to enable the IRQ with HAL_NVIC_EnableIRQ() in the MSP file.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "sound_sensor.h"
#include "module_uart.h"
#include "py32f0xx_it.h"

/******************************************************************************/
/*          Cortex-M0+ Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
}

/******************************************************************************/
/*                Peripheral Interruption and Exception Handlers              */
/******************************************************************************/

/* Example - uncomment and adapt when you enable a peripheral interrupt:

void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}
*/

/************************ (C) COPYRIGHT Puya *****END OF FILE****/

/* Board-specific vectors; bounded acquisition IRQs preempt UART work. */
void USART2_IRQHandler(void) { ModuleUART_IRQHandler(); }
void DMA1_Channel1_IRQHandler(void) { SoundSensor_DMAIRQ(); }
void ADC_COMP_IRQHandler(void) { SoundSensor_ADCIRQ(); }
void EXTI2_3_IRQHandler(void) { SoundSensor_DetectIRQ(); }
