/**
  ******************************************************************************
  * @file    py32f0xx_hal_msp.c
  * @author  Brinco
  * @brief   MSP Initialization and de-Initialization code.
  ******************************************************************************
  * @attention
  *
  * HAL_MspInit() runs once, before any peripheral is initialised. Put the
  * clock/GPIO/NVIC setup of a peripheral into the matching
  * HAL_<Peripheral>_MspInit() callback here (for example HAL_I2C_MspInit(),
  * HAL_TIM_Base_MspInit(), HAL_UART_MspInit()).
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/**
  * @brief Initialise the global MSP.
  */
void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
}

/* Example - uncomment and adapt when you add a peripheral:

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim_base)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (htim_base->Instance == TIM1)
  {
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF13_TIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}
*/

/************************ (C) COPYRIGHT Puya *****END OF FILE****/
