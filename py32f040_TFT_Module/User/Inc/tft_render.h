/** @file tft_render.h @brief Bounded-memory drawing jobs, foreground only. */
#ifndef BRINCO_TFT_RENDER_H
#define BRINCO_TFT_RENDER_H
#include <stdint.h>
#include "tft_config.h"
void TFTRender_Init(void);
void TFTRender_Process(void);
uint8_t TFTRender_Busy(void);
uint8_t TFTRender_Rotation(void);
uint16_t TFTRender_Width(void);
uint16_t TFTRender_Height(void);
/** @brief Call only after validated bounds and idle check; copies row data. */
void TFTRender_Rect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t color);
void TFTRender_Row(uint16_t x,uint16_t y,uint16_t count,const uint16_t *pixels);
void TFTRender_Line(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint16_t color);
void TFTRender_SetRotation(uint8_t rotation);
#endif
