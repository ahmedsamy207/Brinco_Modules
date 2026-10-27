/** @file tft_touch.c
 *  @brief Raw contact filtering and calibrated pixel mapping, no heap/float.
 *  Calibration is RAM-only; host persists it if required. No guessed factory
 *  endpoints are shipped. Pressure proxy thresholds require real panel checks.
 */
#include "main.h"
#include "tft_touch.h"
#include "tft_io.h"
#include "tft_render.h"
#include "tft_config.h"
static TFTTouchReading reading;
static uint16_t ends[4],previous_x,previous_y;
static uint8_t calibration_flags,have_calibration,qualified;
static uint32_t last_ms;

void TFTTouch_Init(void)
{ reading=(TFTTouchReading){0};last_ms=HAL_GetTick();qualified=have_calibration=0; }
static int32_t Absolute(int32_t x) { return x<0?-x:x; }
static uint16_t Map(uint16_t raw,uint16_t a,uint16_t b,uint16_t size)
{
  int32_t result=((int32_t)raw-a)*(size-1)/((int32_t)b-a);
  if (result<0) result=0;
  if (result>=size) result=size-1;
  return (uint16_t)result;
}
uint8_t TFTTouch_Calibrate(uint16_t x0,uint16_t x1,uint16_t y0,uint16_t y1,uint8_t flags)
{
  if (x0>4095U || x1>4095U || y0>4095U || y1>4095U || flags>1U ||
      Absolute((int32_t)x1-x0)<128 || Absolute((int32_t)y1-y0)<128) return 0U;
  ends[0]=x0;ends[1]=x1;ends[2]=y0;ends[3]=y1;
  calibration_flags=flags;have_calibration=1U;
  return 1U;
}
void TFTTouch_Process(void)
{
  TFTRawTouch raw;
  uint32_t now=HAL_GetTick();
  if ((uint32_t)(now-last_ms)<TFT_TOUCH_PERIOD_MS) return;
  if ((uint32_t)(now-last_ms)>TFT_TOUCH_STALE_MS) qualified=0U;
  last_ms=now;
  reading.valid=0U;reading.down=0U;
  reading.x=reading.y=0xffffU;
  if (!TFTIO_ReadTouch(&raw)) { qualified=0U;return; }
  reading.raw_x=raw.x;reading.raw_y=raw.y;reading.z=raw.z;
  reading.updated_ms=HAL_GetTick();++reading.sequence;reading.valid=1U;
  if (raw.stable && raw.z>=TFT_TOUCH_Z_MIN && raw.z<=TFT_TOUCH_Z_MAX) {
    if (qualified && Absolute((int32_t)raw.x-previous_x)<=160 &&
        Absolute((int32_t)raw.y-previous_y)<=160) reading.down=1U;
    qualified=1U;previous_x=raw.x;previous_y=raw.y;
  } else qualified=0U;
}
TFTTouchReading TFTTouch_GetReading(void)
{
  TFTTouchReading value=reading;
  value.calibrated=have_calibration;
  value.x=value.y=0xffffU;
  if ((uint32_t)(HAL_GetTick()-value.updated_ms)>TFT_TOUCH_STALE_MS) value.valid=value.down=0U;
  if (value.valid && value.down && have_calibration) {
    uint16_t rx=calibration_flags?value.raw_y:value.raw_x;
    uint16_t ry=calibration_flags?value.raw_x:value.raw_y;
    uint16_t u=Map(rx,ends[0],ends[1],240U),v=Map(ry,ends[2],ends[3],320U);
    switch(TFTRender_Rotation()) {
      case 0:value.x=u;value.y=v;break;
      case 1:value.x=v;value.y=239U-u;break;
      case 2:value.x=239U-u;value.y=319U-v;break;
      default:value.x=319U-v;value.y=u;break;
    }
  }
  return value;
}
