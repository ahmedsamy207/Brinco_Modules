/** @file sound_sensor.h @brief Timer/ADC/DMA acquisition and D0 EXTI adapters. */
#ifndef BRINCO_SOUND_SENSOR_H
#define BRINCO_SOUND_SENSOR_H
/** @brief Initialize 4 ksample/s acquisition after 32 MHz HSE clock setup. */
void SoundSensor_Init(void);
/** @brief Foreground watchdog; faults stale/stalled acquisition after startup. */
void SoundSensor_Process(void);
void SoundSensor_DMAIRQ(void);
void SoundSensor_ADCIRQ(void);
void SoundSensor_DetectIRQ(void);
#endif
