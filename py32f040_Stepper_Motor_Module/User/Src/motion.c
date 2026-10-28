/** @file motion.c
 * Fixed-point acceleration and phase accumulation, independent of motor HAL.
 * The driver limits current in hardware at0.2V/0.2ohm nominal1A; no software
 * current regulation is claimed. OPA/ADC supervision is in current_monitor.c.
 */
#include "main.h"
#include "motion.h"
#include "current_monitor.h"
#include <string.h>
static volatile MotionStatus motor;
static uint32_t phase_accumulator,lease;
static uint16_t ramp_ticks,wake_ticks;
static uint8_t continuous;
static uint32_t Lock(void) { uint32_t m=__get_PRIMASK();__disable_irq();return m; }
static uint8_t Active(void) { return motor.state!=MOTION_IDLE && motor.state!=MOTION_FAULT; }
static void Release(void)
{
 MotorIO_Enable(0);motor.velocity_q16=0;motor.remaining=0;
 phase_accumulator=0;motor.state=motor.fault?MOTION_FAULT:MOTION_IDLE;
}
void Motion_Trip(uint8_t reason)
{
 uint32_t m=Lock();
 if(!motor.fault) motor.fault=reason;
 Release();__set_PRIMASK(m);
}
void Motion_Init(void)
{
 memset((void *)&motor,0,sizeof motor);motor.speed=50;motor.acceleration=100;
 lease=0;ramp_ticks=0;wake_ticks=0;continuous=0;Release();
}
static uint8_t Start(int32_t direction,uint32_t count,uint8_t run)
{
 uint32_t m=Lock();uint8_t result=MOTION_OK;
 if(motor.fault) result=MOTION_LATCHED;
 else if(motor.state!=MOTION_IDLE && motor.state!=MOTION_HOLD) result=MOTION_BUSY;
 else if(!CurrentMonitor_Get().ready) result=MOTION_NOT_READY;
 else if(MotorIO_Fault()) { Motion_Trip(FAULT_DRIVER);result=MOTION_LATCHED; }
 else {
  uint8_t held=motor.state==MOTION_HOLD;
  motor.direction=(int8_t)direction;motor.remaining=count;continuous=run;
  motor.velocity_q16=0;phase_accumulator=0;ramp_ticks=0;wake_ticks=0;lease=0;
  motor.state=held?(run?MOTION_RUNNING:MOTION_MOVING):MOTION_WAKING;
  if(!held) MotorIO_Enable(1);
 }
 __set_PRIMASK(m);return result;
}
uint8_t Motion_Move(int32_t steps)
{
 if(steps>1000000 || steps< -1000000 || steps==0) return MOTION_BAD_VALUE;
 return Start(steps<0?-1:1,(uint32_t)(steps<0?-steps:steps),0);
}
uint8_t Motion_Run(int32_t direction)
{
 if(direction!=1 && direction!=-1) return MOTION_BAD_VALUE;
 return Start(direction,0,1);
}
uint8_t Motion_Speed(uint32_t value)
{
 if(!value || value>MOTION_MAX_SPEED) return MOTION_BAD_VALUE;
 uint32_t m=Lock();motor.speed=(uint16_t)value;__set_PRIMASK(m);return MOTION_OK;
}
uint8_t Motion_Acceleration(uint32_t value)
{
 if(!value || value>MOTION_MAX_ACCEL) return MOTION_BAD_VALUE;
 uint32_t m=Lock();motor.acceleration=(uint16_t)value;__set_PRIMASK(m);return MOTION_OK;
}
void Motion_KeepAlive(void) { uint32_t m=Lock();lease=0;__set_PRIMASK(m); }
void Motion_Stop(void)
{
 uint32_t m=Lock();
 if(motor.state==MOTION_WAKING) Release();
 else if(motor.state==MOTION_MOVING || motor.state==MOTION_RUNNING) {
  motor.remaining=0;motor.state=MOTION_STOPPING;
 }
 __set_PRIMASK(m);
}
void Motion_Release(void) { uint32_t m=Lock();Release();__set_PRIMASK(m); }
uint8_t Motion_Zero(void)
{
 uint32_t m=Lock();uint8_t r=MOTION_OK;
 if(motor.state!=MOTION_IDLE && motor.state!=MOTION_HOLD) r=MOTION_BUSY;
 else motor.position=0;
 __set_PRIMASK(m);return r;
}
uint8_t Motion_Clear(void)
{
 uint32_t m=Lock();uint8_t r=MOTION_OK;
 if(motor.state!=MOTION_FAULT && motor.state!=MOTION_IDLE) r=MOTION_BUSY;
 else if(MotorIO_Fault()) r=MOTION_LATCHED;
 else if(!CurrentMonitor_Clear()) r=MOTION_LATCHED;
 else { motor.fault=FAULT_NONE;Release(); }
 __set_PRIMASK(m);return r;
}
MotionStatus Motion_GetStatus(void)
{
 uint32_t m=Lock();MotionStatus value=motor;__set_PRIMASK(m);return value;
}
/** Integer floor square root; called only with a bounded value <=260000. */
static uint32_t Root(uint32_t value)
{
 uint32_t result=0,bit=1UL<<30;
 while(bit>value) bit>>=2;
 while(bit) {
  if(value>=result+bit) { value-=result+bit;result=(result>>1)+bit; }
  else result>>=1;
  bit>>=2;
 }
 return result;
}
/** Ramp every1ms. Finite moves cap speed with sqrt(2*a*remaining), recomputed
 * after each step, avoiding hunting between acceleration and braking targets.
 * Remaining is in whole steps: endpoint completion necessarily has discrete
 * resolution. Exact final count takes precedence over fractional-step timing.
 */
static void Ramp(void)
{
 uint32_t desired=(uint32_t)motor.speed<<16;
 uint32_t change=((uint32_t)motor.acceleration<<16)/1000U;
 if(motor.state==MOTION_STOPPING) desired=0;
 else if(motor.state==MOTION_MOVING) {
  uint32_t divisor=2U*motor.acceleration;
  uint32_t boundary=((uint32_t)motor.speed*motor.speed+divisor-1U)/divisor;
  if(motor.remaining<=boundary) {
   uint32_t limit=Root(divisor*motor.remaining)<<16;
   if(limit<desired) desired=limit;
  }
 }
 if(motor.velocity_q16<desired) {
  uint32_t v=motor.velocity_q16+change;motor.velocity_q16=v>desired?desired:v;
 } else if(motor.velocity_q16>desired) {
  uint32_t delta=motor.velocity_q16-desired;
  motor.velocity_q16-=delta<change?delta:change;
 }
 if(motor.state==MOTION_STOPPING && !motor.velocity_q16) {
  motor.state=MOTION_HOLD;phase_accumulator=0;
 }
}
void Motion_Tick(void)
{
 CurrentMonitor_Tick();
 if(!Active()) return;
 if(MotorIO_Fault()) { Motion_Trip(FAULT_DRIVER);return; }
 if(++lease>=MOTION_LEASE_TICKS) { Motion_Trip(FAULT_TIMEOUT);return; }
 if(motor.state==MOTION_WAKING) {
  /* Forty timer edges provide at least1.95ms even when wake starts just
   * before a tick, exceeding the driver's1ms maximum wake specification. */
  if(++wake_ticks>=40U) {
   MotorIO_Phase(motor.phase);
   motor.state=continuous?MOTION_RUNNING:MOTION_MOVING;
  }
  return;
 }
 if(motor.state==MOTION_HOLD) return;
 if(++ramp_ticks>=20U) { ramp_ticks=0;Ramp(); }
 phase_accumulator+=motor.velocity_q16;
 if(phase_accumulator>=(MOTION_TICK_HZ<<16)) {
  phase_accumulator-=(MOTION_TICK_HZ<<16);
  if((motor.direction>0 && motor.position==INT32_MAX) ||
     (motor.direction<0 && motor.position==INT32_MIN)) { Motion_Trip(FAULT_POSITION);return; }
  motor.phase=(uint8_t)((motor.phase+(motor.direction>0?1U:3U))&3U);
  MotorIO_Phase(motor.phase);motor.position+=motor.direction;
  if(motor.state==MOTION_MOVING && --motor.remaining==0U) {
   motor.velocity_q16=0;phase_accumulator=0;motor.state=MOTION_HOLD;
  }
 }
}
