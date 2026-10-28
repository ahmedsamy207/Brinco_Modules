/** @file motion.h
 * NFP-15BY-A open-loop full-step motion. One step is18 degrees;20/revolution.
 * Only the timer ISR calls Motion_Tick; foreground commands mask IRQs briefly.
 */
#ifndef BRINCO_MOTION_H
#define BRINCO_MOTION_H
#include <stdint.h>
#define MOTION_TICK_HZ 20000U
#define MOTION_MAX_SPEED 500U
#define MOTION_MAX_ACCEL 5000U
#define MOTION_LEASE_TICKS (2U*MOTION_TICK_HZ)
enum { MOTION_IDLE, MOTION_WAKING, MOTION_MOVING, MOTION_RUNNING, MOTION_STOPPING, MOTION_HOLD, MOTION_FAULT };
enum { FAULT_NONE, FAULT_DRIVER, FAULT_TIMEOUT, FAULT_POSITION, FAULT_UART, FAULT_CURRENT, FAULT_SENSE };
enum { MOTION_OK, MOTION_BUSY, MOTION_BAD_VALUE, MOTION_LATCHED, MOTION_NOT_READY };
typedef struct {
 int32_t position;
 uint32_t remaining, velocity_q16;
 uint16_t speed,acceleration;
 uint8_t state,fault,phase;
 int8_t direction;
} MotionStatus;
/** Initialize released with50 steps/s,100 steps/s^2. Foreground startup only. */
void Motion_Init(void);
/** Timer IRQ only: advance one50us scheduling tick, at most one coil transition. */
void Motion_Tick(void);
/** Foreground: start a relative signed full-step move (maximum1,000,000 steps). */
uint8_t Motion_Move(int32_t steps);
/** Foreground: start continuous rotation, direction+1 or-1. */
uint8_t Motion_Run(int32_t direction);
/** Foreground: set speed1..500 full steps/s; may change during motion. */
uint8_t Motion_Speed(uint32_t steps_per_second);
/** Foreground: acceleration/deceleration1..5000 full steps/s^2. */
uint8_t Motion_Acceleration(uint32_t steps_per_second_squared);
/** Foreground: controlled deceleration to energized HOLD. */
void Motion_Stop(void);
/** Foreground: immediate release, cancel queued motion, retain fault latch. */
void Motion_Release(void);
/** Foreground: set logical position0 only when not moving; no physical homing. */
uint8_t Motion_Zero(void);
/** Foreground: clear latch only with nFAULT high and healthy current acquisition; no restart. */
uint8_t Motion_Clear(void);
/** Foreground: refresh the2s lease from a valid host command, including STATUS. */
void Motion_KeepAlive(void);
/** IRQ/foreground: latch fault and immediately disable both bridges. */
void Motion_Trip(uint8_t reason);
/** Foreground: coherent snapshot; returned position counts commanded transitions. */
MotionStatus Motion_GetStatus(void);
/* Hardware boundary; phase0..3 has A+B+,A-B+,A-B-,A+B- order. */
void MotorIO_InitSafe(void);
void MotorIO_StartTimer(void);
void MotorIO_Enable(uint8_t enabled);
void MotorIO_Phase(uint8_t phase);
uint8_t MotorIO_Fault(void);
#endif
