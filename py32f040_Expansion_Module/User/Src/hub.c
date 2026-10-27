/** @file hub.c
 * Hop routing, CRC validation and duplicate suppression, independent of HAL IO.
 * One outstanding command per chain. Replies remain cached until a NEW ID;
 * repeating an identical request cannot retransmit UART bytes or consume RX.
 * Cache is volatile: after reset the host must not retry uncertain mutations.
 */
#include "main.h"
#include "hub.h"
#include <string.h>
static uint8_t cached[HUB_FRAME], reply[HUB_FRAME], forward[HUB_FRAME];
static uint8_t have_request, pending, poll_started;
static uint32_t started, next_poll;
static uint16_t transport_errors;

/** CRC-16/CCITT-FALSE, polynomial1021, initialFFFF, no reflection/xorout. */
uint16_t Hub_CRC(const uint8_t *p, uint16_t n)
{
  uint16_t crc=0xffffU;
  while(n--) { crc^=(uint16_t)*p++ << 8;
    for(uint8_t i=0;i<8;i++) crc=(crc&0x8000U)?(uint16_t)((crc<<1)^0x1021U):(uint16_t)(crc<<1);
  }
  return crc;
}
static void Seal(uint8_t *p)
{
  uint16_t c=Hub_CRC(p,62); p[62]=(uint8_t)(c>>8);p[63]=(uint8_t)c;
}
static uint8_t Valid(const uint8_t *p, uint8_t response)
{
  if(p[0]!=(response?'H':'B') || p[1]!=(response?'B':'H') || p[2]!=1 || p[6]>HUB_PAYLOAD) return 0;
  if(response) {
    if(p[3]>HUB_CLEAR || p[4]>HUB_MAX_HOP || p[5]>HUB_LINK || p[7] || p[12] || p[13]) return 0;
    for(uint8_t i=(uint8_t)(14+p[6]);i<62;i++) if(p[i]) return 0;
  }
  return Hub_CRC(p,62)==((uint16_t)p[62]<<8|p[63]);
}
static uint32_t Transaction(const uint8_t *p)
{
  return (uint32_t)p[8]|((uint32_t)p[9]<<8)|((uint32_t)p[10]<<16)|((uint32_t)p[11]<<24);
}
void Hub_Init(void)
{
  memset(cached,0,sizeof cached);memset(reply,0,sizeof reply);
  have_request=0;pending=0;poll_started=0;transport_errors=0;
  reply[0]='H';reply[1]='B';reply[2]=1;reply[5]=HUB_BUSY;Seal(reply);
}
void Hub_TransportError(void) { if(transport_errors!=65535U) transport_errors++; }
void Hub_BuildReply(uint8_t *p) { memcpy(p,reply,HUB_FRAME); }

/** Validate the entire envelope before touching queues. Padding must be zero. */
void Hub_Handle(const uint8_t *p)
{
  if(!Valid(p,0)) { Hub_TransportError();return; }
  if(p[3]==HUB_FETCH) {
    for(uint8_t i=4;i<62;i++) if(p[i]) { Hub_TransportError();return; }
    return;
  }
  /* IDs are uint32 little-endian. Ignore conflicting reuse and premature commands:
   * keep the original outcome available rather than destroying its receipt. */
  if(have_request && memcmp(p+8,cached+8,4)==0) {
    if(memcmp(p,cached,HUB_FRAME)!=0) Hub_TransportError();
    return;
  }
  /* Strictly increasing IDs also reject an older request after cache eviction.
   * IDs never wrap within a powered session; ID0 is reserved for startup. */
  if(!Transaction(p) || (have_request && Transaction(p)<Transaction(cached))) {
    Hub_TransportError();return;
  }
  if(pending) return;
  memcpy(cached,p,HUB_FRAME);have_request=1;
  memset(reply,0,HUB_FRAME);reply[0]='H';reply[1]='B';reply[2]=1;
  reply[3]=p[3];reply[4]=p[4];memcpy(reply+8,p+8,4);
  reply[5]=HUB_INVALID;
  if(p[4]>HUB_MAX_HOP || p[7] || p[12] || p[13]) goto done;
  for(uint8_t i=(uint8_t)(14+p[6]);i<62;i++) if(p[i]) goto done;
  if(p[3]<HUB_INFO || p[3]>HUB_CLEAR) goto done;
  if(p[3]==HUB_INFO) { if(p[5] || p[6]) goto done; }
  else if(p[5]<1 || p[5]>4) goto done;
  if(p[3]!=HUB_WRITE && p[6]) goto done;
  if(p[3]==HUB_WRITE && !p[6]) goto done;
  if(p[4]) {
    memcpy(forward,p,HUB_FRAME);forward[4]--;Seal(forward);
    pending=1;poll_started=0;started=HAL_GetTick();next_poll=started;
    reply[5]=HUB_BUSY;goto done;
  }
  reply[5]=HUB_OK;
  switch(p[3]) {
    case HUB_INFO:
      /* Version1.0, four ports, max relativehop7, payload48, baud115200LE. */
      reply[6]=12;reply[14]=1;reply[15]=0;reply[16]=4;reply[17]=HUB_MAX_HOP;
      reply[18]=HUB_PAYLOAD;reply[19]=0;reply[20]=0x00;reply[21]=0xc2;reply[22]=0x01;
      reply[24]=(uint8_t)transport_errors;reply[25]=(uint8_t)(transport_errors>>8);break;
    case HUB_WRITE:
      if(!Ports_Write(p[5]-1,p+14,p[6])) reply[5]=HUB_FULL;
      else { reply[6]=1;reply[14]=p[6]; } break;
    case HUB_READ: reply[6]=Ports_Read(p[5]-1,reply+14,HUB_PAYLOAD);break;
    case HUB_STATUS: reply[6]=6;Ports_Status(p[5]-1,reply+14);break;
    case HUB_CLEAR: Ports_Clear(p[5]-1);break;
    default:break;
  }
done: Seal(reply);
}

/** Forward once, then FETCH only. Never replay a mutating command on a link
 * timeout: a lost reply cannot prove whether UART output already happened.
 * Budget grows with remaining hops, avoiding nested timeout inversion. */
void Hub_Process(void)
{
  uint8_t rx[HUB_FRAME], result;
  uint32_t now=HAL_GetTick();
  if(!pending) return;
  if((uint32_t)(now-started) > (uint32_t)(cached[4]+1U)*200U) {
    Downstream_Cancel();pending=0;reply[5]=HUB_LINK;Seal(reply);return;
  }
  if(poll_started) {
    result=Downstream_Result(rx);
    if(!result) return;
    poll_started=0;next_poll=now+5U;
    if(result==2) { pending=0;reply[5]=HUB_LINK;Seal(reply);return; }
    if(Valid(rx,1) && rx[3]==cached[3] && rx[4]==cached[4]-1U &&
       memcmp(rx+8,cached+8,4)==0 && rx[5]!=HUB_BUSY) {
      memcpy(reply,rx,HUB_FRAME);reply[4]=cached[4];Seal(reply);pending=0;
    }
    /* All subsequent transfers are harmless FETCH frames. The first transfer
     * returns the downstream's previous response because SPI is pipelined. */
    memset(forward,0,HUB_FRAME);forward[0]='B';forward[1]='H';forward[2]=1;Seal(forward);
  }
  if(pending && (int32_t)(now-next_poll)>=0 && Downstream_Start(forward)) poll_started=1;
}
