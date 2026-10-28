/** @file module_uart.h
 *  @brief UART service API. Initialization and processing are foreground-only.
 */
#ifndef BRINCO_MODULE_UART_H
#define BRINCO_MODULE_UART_H
/** @brief Initialize the module UART after HAL and system clock setup. */
void ModuleUART_Init(void);
/** @brief Drain received bytes, recover errors, and dispatch one command. */
void ModuleUART_Process(void);
/** @brief HAL interrupt adapter; call only from the matching USART vector. */
void ModuleUART_IRQHandler(void);
#endif
