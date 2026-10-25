/** @file spi_transport.h @brief SPI1 mode-0 slave on the dedicated TFT header. */
#ifndef BRINCO_SPI_TRANSPORT_H
#define BRINCO_SPI_TRANSPORT_H
void SPITransport_Init(void);
/** @brief Finish CS-delimited frames and arm the next frozen response. */
void SPITransport_Process(void);
void SPITransport_IRQHandler(void);
void SPITransport_NSS_IRQHandler(void);
#endif
