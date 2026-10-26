/** @file sound_config.h @brief Acquisition units and board assumptions. */
#ifndef BRINCO_SOUND_CONFIG_H
#define BRINCO_SOUND_CONFIG_H
#define SOUND_SAMPLE_HZ 4000U
#define SOUND_WINDOW_SAMPLES 80U /* 20 ms per result; 160-word DMA ping-pong. */
#define SOUND_WINDOW_MS 20U
#define SOUND_STARTUP_MS 100U /* Engineering settling guard, verify on hardware. */
#define SOUND_STALE_MS 100U
#define SOUND_ADC_MAX 4095U
#define SOUND_CLIP_MARGIN 5U /* Near either ADC rail; not an acoustic limit. */
#ifndef SOUND_DETECT_ACTIVE_LOW
#define SOUND_DETECT_ACTIVE_LOW 1U /* LM393 sinks D0 when A0 > mid-supply. */
#endif
#if SOUND_DETECT_ACTIVE_LOW != 0 && SOUND_DETECT_ACTIVE_LOW != 1
#error SOUND_DETECT_ACTIVE_LOW must be 0 or 1
#endif
#endif
