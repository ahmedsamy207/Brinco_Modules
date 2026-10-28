/**
 * @file module_uart.c
 * @brief USART3 command transport for the stepper motor module.
 *
 * Hardware: PB10 TX / PB11 RX, AF4; schematic TX/RX labels name the main board side.
 * Link: 115200 baud, 8N1. Interrupt callbacks only enqueue incoming bytes;
 * the foreground parses LF/CRLF commands and transmits bounded responses.
 * The host must consume one response before sending the next command.
 */
#include "main.h"
#include "module_uart.h"
#include "motion.h"
#include "motion_protocol.h"
#include <string.h>

#define RX_CAPACITY 128U
#define LINE_CAPACITY 64U
#define LINE_TIMEOUT_MS 500U

static UART_HandleTypeDef uart;
static uint8_t rx_byte;
/* Single producer (UART ISR), single consumer (foreground). One slot stays
 * empty to distinguish full from empty: usable capacity is 127 bytes. */
static uint8_t rx_queue[RX_CAPACITY];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint8_t rx_lost;
static volatile uint8_t rx_restart;
/* Parser state belongs to the foreground. discard_line prevents a damaged
 * command suffix from becoming a new command before the next LF delimiter. */
static char line[LINE_CAPACITY];
static uint8_t line_size;
static uint8_t discard_line;
static uint8_t saw_cr;
static uint32_t last_byte_ms;

/** @brief Transmit a NUL-terminated diagnostic/text response in the foreground.
 *  @note The terminator is not sent. A failed transmission is not retried;
 *        the host detects a missing/truncated reply through its timeout.
 */
static void Reply(const char *message)
{
  /* RX remains interrupt-driven during this bounded foreground transmission.
   * A host must wait for the reply before sending the next command. */
  (void)HAL_UART_Transmit(&uart, (uint8_t *)message, (uint16_t)strlen(message), 20U);
}

/** @brief Dispatch one complete, validated line in foreground context.
 *  @note Dispatches bounded motion commands while the timer IRQ owns stepping.
 *        Every response is an ASCII line; no unsolicited events are emitted.
 */
static void ExecuteCommand(void)
{
  char response[160];
  line[line_size] = '\0';
  MotionProtocol_Execute(line,response);
  Reply(response);
}

/** @brief Configure board-specific UART pins and arm one-byte interrupt RX.
 *  @note Call after HAL/clock setup. Initialization failure is fatal.
 */
void ModuleUART_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_USART3_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF4_USART3;
  HAL_GPIO_Init(GPIOB, &gpio);

  uart.Instance = USART3;
  uart.Init.BaudRate = 115200U;
  uart.Init.WordLength = UART_WORDLENGTH_8B;
  uart.Init.StopBits = UART_STOPBITS_1;
  uart.Init.Parity = UART_PARITY_NONE;
  uart.Init.Mode = UART_MODE_TX_RX;
  uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uart.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&uart) != HAL_OK) APP_ErrorHandler();
  HAL_NVIC_SetPriority(USART3_4_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(USART3_4_IRQn);
  if (HAL_UART_Receive_IT(&uart, &rx_byte, 1U) != HAL_OK) APP_ErrorHandler();
}

/** @brief Forward the MCU vector to the HAL; called only in IRQ context. */
void ModuleUART_IRQHandler(void)
{
  HAL_UART_IRQHandler(&uart);
}

/** @brief ISR producer: enqueue a byte and immediately re-arm reception.
 *  @note On overflow, retain a loss flag rather than overwriting unread data.
 *        No parsing or blocking transmission is allowed in this callback.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *handle)
{
  uint16_t next;
  if (handle != &uart) return;
  next = (rx_head + 1U) % RX_CAPACITY;
  if (next == rx_tail) {
    rx_lost = 1U;
  } else {
    rx_queue[rx_head] = rx_byte;
    /* Publish data before making the new queue head visible to the consumer. */
    __DMB();
    rx_head = next;
  }
  if (HAL_UART_Receive_IT(&uart, &rx_byte, 1U) != HAL_OK) rx_restart = 1U;
}

/** @brief ISR error notification; defer abort/re-arm and parser reset.
 *  @note Even a recoverable UART error invalidates the partial command.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *handle)
{
  if (handle != &uart) return;
  rx_lost = 1U;
  rx_restart = 1U;
}

/** @brief Foreground consumer: recover RX errors and process at most one line.
 *  @note Call frequently. Limiting dispatch to one line per call keeps recovery
 *        responsive; the timer IRQ runs independently during replies.
 */
void ModuleUART_Process(void)
{
  uint32_t now = HAL_GetTick();
  if (rx_lost || rx_restart) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    /* A line with lost/corrupted bytes must never become a valid command. */
    rx_tail = rx_head;
    rx_lost = 0U;
    /* IRQs are masked while resetting shared queue/HAL RX state; preserve
     * the caller's original interrupt mask on exit from this short section. */
    if (rx_restart) {
      (void)HAL_UART_AbortReceive(&uart);
      __HAL_UART_CLEAR_OREFLAG(&uart);
      rx_restart = HAL_UART_Receive_IT(&uart, &rx_byte, 1U) != HAL_OK;
    }
    __set_PRIMASK(mask);
    Motion_Trip(FAULT_UART);
    line_size = 0U;
    discard_line = 1U;
    Reply("ERR RX\r\n");
  }
  if (line_size != 0U && (uint32_t)(now - last_byte_ms) >= LINE_TIMEOUT_MS) {
    line_size = 0U;
    discard_line = 1U;
    Reply("ERR LINE_TIMEOUT\r\n");
  }
  while (rx_tail != rx_head) {
    /* An IRQ may report corruption after this call began. Defer recovery
     * before dispatching any command assembled from that receive stream. */
    if (rx_lost || rx_restart) return;
    uint8_t byte = rx_queue[rx_tail];
    rx_tail = (rx_tail + 1U) % RX_CAPACITY;
    last_byte_ms = now;
    /* LF ends a normal line or ends discard mode. CR is checked below so
     * both LF and CRLF hosts are accepted. Blank lines do nothing. */
    if (byte == '\n') {
      if (discard_line) {
        discard_line = 0U;
      } else if (line_size != 0U) {
        if (rx_lost || rx_restart) return;
        ExecuteCommand();
      }
      line_size = 0U;
      saw_cr = 0U;
      /* Return to the foreground service loop before another command. */
      return;
    }
    if (discard_line) continue;
    if (byte == '\r' && !saw_cr) { saw_cr=1U;continue; }
    /* CR is accepted only immediately before LF, never inside a verb/number. */
    if (saw_cr) { discard_line=1U;line_size=0U;Reply("ERR LINE\r\n");return; }
    if (byte < 0x20U || byte > 0x7eU || line_size >= LINE_CAPACITY - 1U) {
      discard_line = 1U;
      line_size = 0U;
      Reply("ERR LINE\r\n");
      return;
    }
    line[line_size++] = (char)byte;
  }
}
