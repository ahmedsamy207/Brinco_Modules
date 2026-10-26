/**
  ******************************************************************************
  * @file    py32f0xx_it.h
  * @author  Brinco
  * @brief   Headers of the interrupt handlers.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PY32F0XX_IT_H
#define __PY32F0XX_IT_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Exported functions prototypes ---------------------------------------------*/
void NMI_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/* Add the prototypes of the peripheral handlers you implement, e.g.
   void USART1_IRQHandler(void); */

#ifdef __cplusplus
}
#endif

#endif /* __PY32F0XX_IT_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE****/
