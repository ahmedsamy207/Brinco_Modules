/** @file current_monitor.h
 * Approximate positive shunt-current supervision, not a winding-current servo.
 * 128 triplets/window: external ADC2=A OPA1, ADC4=B OPA2, internal VREFINT.
 */
#ifndef CURRENT_MONITOR_H
#define CURRENT_MONITOR_H
#include <stdint.h>
#define CURRENT_SCANS 128U
#define CURRENT_WORDS (3U*CURRENT_SCANS)
#define CURRENT_DEFAULT_LIMIT_MA 750U
#define CURRENT_STALE_TICKS 800U /*40ms of the existing20kHz motor tick.*/
typedef struct {
 uint16_t mean_ma[2],peak_ma[2],vcca_mv,limit_ma;
 uint8_t ready,fault;
 uint32_t sequence;
} CurrentReading;
/* Analyze into private storage before checking that DMA still owns the other
 * half. Only Commit may publish telemetry or change protection state. */
typedef struct {
 uint16_t mean_mv[2],peak_mv[2],vcca_mv;
 uint16_t over[2],clipped[2];
} CurrentBlock;
/** Initialize not-ready. Startup requires2 settle +8 released calibration windows. */
void CurrentMonitor_Init(void);
/** DMA context: validate raw12-bit triplets and calculate compensated statistics. */
uint8_t CurrentMonitor_Analyze(const volatile uint16_t *samples,CurrentBlock *block);
/** DMA context: publish a stable block, calibrate offsets, qualify faults. */
void CurrentMonitor_Commit(const CurrentBlock *block);
/** Any context: latch acquisition failure, release motor; reset required. */
void CurrentMonitor_Fail(void);
/**20kHz motor IRQ: detect acquisition silence, including while motor is idle. */
void CurrentMonitor_Tick(void);
/** Foreground/motion context: coherent snapshot. READY requires fresh calibrated data. */
CurrentReading CurrentMonitor_Get(void);
/** Foreground with released motor:100..1000mA supervisory threshold, RAM only. */
uint8_t CurrentMonitor_SetLimit(uint32_t milliamps);
/** Motion CLEAR: allow only healthy acquisition and3 consecutive safe windows. */
uint8_t CurrentMonitor_Clear(void);
/* Hardware adapter; IRQ methods are called only by their matching vectors. */
void CurrentADC_Init(void);
void CurrentADC_DMAIRQ(void);
void CurrentADC_ADCIRQ(void);
#endif
