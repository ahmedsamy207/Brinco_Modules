/** @file current_monitor.c
 * External gain1+15k/1k=16, shunt0.2ohm ->3.2mV/mA.
 * Automatic released offset calibration improves zero display, but OPA rail
 * clipping/offset and chopped low-side visibility prevent precision/RMS claims.
 */
#include "main.h"
#include "motion.h"
#include "current_monitor.h"
#include <string.h>
static volatile CurrentReading reading;
static volatile uint16_t age;
static uint16_t offset_mv[2];
static uint32_t offset_sum[2];
static uint8_t settle,calibration,over_streak,clip_streak,safe_streak;
static uint8_t started;
static uint32_t Lock(void) { uint32_t m=__get_PRIMASK();__disable_irq();return m; }
static uint16_t Milliamps(uint16_t mv,uint16_t zero)
{ return mv>zero?(uint16_t)(((uint32_t)(mv-zero)*5U+8U)/16U):0; }
void CurrentMonitor_Init(void)
{
 memset((void *)&reading,0,sizeof reading);reading.limit_ma=CURRENT_DEFAULT_LIMIT_MA;
 offset_mv[0]=offset_mv[1]=0;offset_sum[0]=offset_sum[1]=0;
 settle=calibration=over_streak=clip_streak=safe_streak=0;age=0;started=1;
}
void CurrentMonitor_Fail(void)
{
 uint32_t m=Lock();reading.ready=0;reading.fault=FAULT_SENSE;
 Motion_Trip(FAULT_SENSE);__set_PRIMASK(m);
}
void CurrentMonitor_Tick(void)
{
 if(started && reading.fault!=FAULT_SENSE && ++age>=CURRENT_STALE_TICKS) CurrentMonitor_Fail();
}
CurrentReading CurrentMonitor_Get(void)
{
 uint32_t m=Lock();CurrentReading result=reading;__set_PRIMASK(m);return result;
}
uint8_t CurrentMonitor_Analyze(const volatile uint16_t *samples,CurrentBlock *block)
{
 uint32_t sums[3]={0};uint16_t peaks[2]={0};
 memset(block,0,sizeof *block);
 for(uint16_t i=0;i<CURRENT_SCANS;i++) {
  for(uint8_t c=0;c<3;c++) {
   uint16_t x=samples[3U*i+c];if(x>4095U) return 0;
   sums[c]+=x;
   if(c<2) { if(x>peaks[c]) peaks[c]=x;if(x>=3800U) block->clipped[c]++; }
  }
 }
 if(!sums[2]) return 0;
 /* Nominal1.200V VREFINT; no factory-trim accuracy claim (datasheet1.17..1.23).
  * 239.5/9MHz >=20us minimum acquisition for the internal channel. */
 uint32_t vcca=(1200UL*4095U*CURRENT_SCANS+sums[2]/2U)/sums[2];
 if(vcca<4200U || vcca>5500U) return 0; /* This is a5V board. */
 block->vcca_mv=(uint16_t)vcca;
 for(uint8_t c=0;c<2;c++) {
  block->mean_mv[c]=(uint16_t)((sums[c]*vcca+CURRENT_SCANS*4095U/2U)/(CURRENT_SCANS*4095U));
  block->peak_mv[c]=(uint16_t)(((uint32_t)peaks[c]*vcca+2047U)/4095U);
  /* Round threshold upward so an equal reading does not count as excessive. */
  uint32_t threshold_mv=offset_mv[c]+((uint32_t)reading.limit_ma*16U+4U)/5U;
  uint32_t threshold_code=(threshold_mv*4095U+vcca-1U)/vcca;
  for(uint16_t i=0;i<CURRENT_SCANS;i++) if(samples[3U*i+c]>threshold_code) block->over[c]++;
 }
 return 1;
}
void CurrentMonitor_Commit(const CurrentBlock *block)
{
 uint32_t m=Lock();
 /* A higher-priority motor tick may have declared a stale acquisition while
  * Analyze was preempted; a late DMA result must never clear that latch. */
 if(reading.fault==FAULT_SENSE) { __set_PRIMASK(m);return; }
 age=0;reading.sequence++;reading.vcca_mv=block->vcca_mv;
 if(settle<2U) { settle++;__set_PRIMASK(m);return; }
 if(calibration<8U) {
  /* Motion is gated until ready; require plausible released outputs. Never
   * zero a large stuck signal into an apparently healthy current reading. */
  for(uint8_t c=0;c<2;c++) {
   if(block->mean_mv[c]>300U || block->peak_mv[c]>500U) {
    __set_PRIMASK(m);CurrentMonitor_Fail();return;
   }
   offset_sum[c]+=block->mean_mv[c];
  }
  calibration++;
  if(calibration==8U) {
   offset_mv[0]=(uint16_t)((offset_sum[0]+4U)/8U);offset_mv[1]=(uint16_t)((offset_sum[1]+4U)/8U);
   reading.ready=1;
  }
  __set_PRIMASK(m);return;
 }
 for(uint8_t c=0;c<2;c++) {
  reading.mean_ma[c]=Milliamps(block->mean_mv[c],offset_mv[c]);
  reading.peak_ma[c]=Milliamps(block->peak_mv[c],offset_mv[c]);
 }
 /* At least1/8 of a window above threshold, in3 consecutive windows (~24ms).
  * Isolated switching spikes do not trip. Missing pulses still cannot be
  * inferred: the driver's independent chopping/OCP remains essential. */
 uint8_t high=block->over[0]>=16U || block->over[1]>=16U;
 uint8_t clipped=block->clipped[0]>=16U || block->clipped[1]>=16U;
 over_streak=high?(uint8_t)(over_streak<3?over_streak+1:3):0;
 clip_streak=clipped?(uint8_t)(clip_streak<3?clip_streak+1:3):0;
 safe_streak=(!high && !clipped)?(uint8_t)(safe_streak<3?safe_streak+1:3):0;
 uint8_t trip=clip_streak>=3?FAULT_SENSE:(over_streak>=3?FAULT_CURRENT:0);
 if(trip) {
  reading.fault=trip;if(trip==FAULT_SENSE) reading.ready=0;
  Motion_Trip(trip);
 }
 __set_PRIMASK(m);
}
uint8_t CurrentMonitor_SetLimit(uint32_t value)
{
 if(value<100U || value>1000U) return MOTION_BAD_VALUE;
 uint32_t m=Lock();MotionStatus s=Motion_GetStatus();
 if(s.state!=MOTION_IDLE) { __set_PRIMASK(m);return MOTION_BUSY; }
 reading.limit_ma=(uint16_t)value;over_streak=clip_streak=safe_streak=0;
 __set_PRIMASK(m);return MOTION_OK;
}
uint8_t CurrentMonitor_Clear(void)
{
 uint32_t m=Lock();uint8_t ok=reading.ready && reading.fault!=FAULT_SENSE;
 if(reading.fault==FAULT_CURRENT) ok=ok && safe_streak>=3U;
 if(ok) { reading.fault=0;over_streak=clip_streak=0; }
 __set_PRIMASK(m);return ok;
}
