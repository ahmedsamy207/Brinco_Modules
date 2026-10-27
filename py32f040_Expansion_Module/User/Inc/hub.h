/** @file hub.h
 * Binary-transparent, four-port bridge. Protocol runs only in foreground;
 * hardware IRQs own UART byte movement and downstream SPI progress.
 */
#ifndef HUB_H
#define HUB_H
#include <stdint.h>
#define HUB_FRAME 64U
#define HUB_PAYLOAD 48U
#define HUB_MAX_HOP 7U
#define HUB_SPI_TIMEOUT_MS 100U
/* Request operations; FETCH never replaces an outstanding command. */
enum { HUB_FETCH=0, HUB_INFO=1, HUB_WRITE=2, HUB_READ=3, HUB_STATUS=4, HUB_CLEAR=5 };
enum { HUB_OK=0, HUB_BUSY=1, HUB_INVALID=2, HUB_FULL=3, HUB_LINK=4 };
/** Initialize an empty startup receipt before enabling either SPI port. */
void Hub_Init(void);
/** Foreground only: accept one complete validated64-byte request. */
void Hub_Handle(const uint8_t *request);
/** Foreground only: advance the pending route; never wait for IO. */
void Hub_Process(void);
/** Copy a64-byte immutable snapshot into caller-owned storage. */
void Hub_BuildReply(uint8_t *reply);
/** Foreground only: saturating diagnostic counter, no receipt mutation. */
void Hub_TransportError(void);
uint16_t Hub_CRC(const uint8_t *data, uint16_t count);
/* Platform boundary. UART indices are zero-based here, one-based on the wire.
 * Read consumes bytes; Write must enqueue all bytes or return zero. */
/** Configure fixed pins/baud and arm one-byte IRQ reception on all ports. */
void Ports_Init(void);
/** Foreground TX kick/RX recovery; no waits for module responses. */
void Ports_Process(void);
/** Queue1..48 bytes atomically; return1 accepted or0 insufficient space. */
uint8_t Ports_Write(uint8_t port, const uint8_t *data, uint8_t count);
/** Consume up to count bytes; return actual count including possible0. */
uint8_t Ports_Read(uint8_t port, uint8_t *data, uint8_t count);
/** Snapshot RX/TX occupancy and sticky flags as three little-endian uint16s. */
void Ports_Status(uint8_t port, uint8_t *data);
/** Clear sticky error flags without discarding RX/TX or cancelling IO. */
void Ports_Clear(uint8_t port);
/** Configure SPI2 master and leave downstream CS inactive. */
void Downstream_Init(void);
/** Copy a64-byte request and assert CS when idle/gap permits;0 means retry later. */
uint8_t Downstream_Start(const uint8_t *request);
/* 0=in progress, 1=complete, 2=hardware timeout/error; copies exactly64 bytes. */
uint8_t Downstream_Result(uint8_t *reply);
/** Foreground: handle setup, transfer completion and physical timeout. */
void Downstream_Process(void);
/** Foreground: abort active transfer and return to idle after route timeout. */
void Downstream_Cancel(void);
/** SPI2 IRQ byte service; no blocking HAL polling functions. */
void Downstream_IRQHandler(void);
/** Service the selected0-based UART from its physical vector. */
void Ports_IRQHandler(uint8_t port);
#endif
