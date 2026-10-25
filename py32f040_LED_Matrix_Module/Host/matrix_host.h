/** @file matrix_host.h
 *  @brief Portable ESP32-side reference helper; board SPI setup is external.
 *  Callbacks must transfer exactly 160 bytes with one CS pulse in mode 0,
 *  MSB first, <=250 kHz, >=10 us CS setup, and honor the requested idle gaps.
 *  Enable the main board's level shifter according to its verified hardware.
 */
#ifndef BRINCO_MATRIX_HOST_H
#define BRINCO_MATRIX_HOST_H
#include <stdint.h>
#include <stdbool.h>
#include "../User/Inc/matrix_protocol.h"
typedef struct {
  void *context;
  bool (*exchange)(void *context,const uint8_t *tx,uint8_t *rx);
  void (*wait_us)(void *context,uint32_t microseconds);
  uint8_t next_sequence;
} MatrixHost;
/** @brief Send one command, then FETCH its validated ACK (up to eight polls).
 *  @return true if a matching ACK arrived; inspect reply[5] for command status
 *          and reply[6] for live busy/fault flags. false means outcome unknown,
 *          not proof that the command was unexecuted. Only FETCH is retried.
 *  @param reply Storage for 160 bytes; inspect only on true return.
 */
bool MatrixHost_Command(MatrixHost *host,uint8_t command,const uint8_t *data,
                        uint8_t length,uint8_t *reply);
#endif
