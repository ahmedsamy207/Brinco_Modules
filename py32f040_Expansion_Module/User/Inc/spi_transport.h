/** @file spi_transport.h @brief SPI1 mode-0 slave on the EXP_IN link. */
#ifndef BRINCO_SPI_TRANSPORT_H
#define BRINCO_SPI_TRANSPORT_H
/** Configure PA4..7 AF0, hardware NSS and its CS-rising EXTI. */
void SPITransport_Init(void);
/** @brief Finish CS-delimited frames and arm the next frozen response. */
void SPITransport_Process(void);
/** SPI1 IRQ byte service; all payload processing stays in foreground. */
void SPITransport_IRQHandler(void);
/** EXTI4_15 vector: close only line4 frames; discard unexpected boundaries. */
void SPITransport_NSS_IRQHandler(void);
#endif
