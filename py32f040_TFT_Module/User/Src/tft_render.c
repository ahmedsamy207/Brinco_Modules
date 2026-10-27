/** @file tft_render.c
 *  @brief Cooperative rectangle/row/Bresenham rendering, <=64 pixels per call.
 *  No full framebuffer: images and host-rendered fonts arrive as RGB565 rows.
 *  SPI IRQs stay enabled; touch runs only between these closed LCD operations.
 */
#include "tft_render.h"
#include "tft_io.h"
#include <string.h>
typedef enum {IDLE, RECT, ROW, LINE} Job;
static Job job;
static uint8_t rotation;
static uint16_t x,y,width,height,column,row,color;
static uint16_t pixels[TFT_MAX_ROW_PIXELS];
static int32_t lx,ly,end_x,end_y,dx,dy,sx,sy,error;

void TFTRender_Init(void)
{
  rotation=0U;job=IDLE;
  TFTIO_Init();
  /* Display RAM is undefined after reset; clear it as a cooperative job. */
  TFTRender_Rect(0,0,TFT_NATIVE_WIDTH,TFT_NATIVE_HEIGHT,0);
}
uint8_t TFTRender_Busy(void) { return job!=IDLE; }
uint8_t TFTRender_Rotation(void) { return rotation; }
uint16_t TFTRender_Width(void) { return rotation&1U ? TFT_NATIVE_HEIGHT:TFT_NATIVE_WIDTH; }
uint16_t TFTRender_Height(void) { return rotation&1U ? TFT_NATIVE_WIDTH:TFT_NATIVE_HEIGHT; }
void TFTRender_SetRotation(uint8_t value) { rotation=value;TFTIO_SetRotation(value); }
void TFTRender_Rect(uint16_t xx,uint16_t yy,uint16_t w,uint16_t h,uint16_t c)
{ x=xx;y=yy;width=w;height=h;color=c;column=row=0U;job=RECT; }
void TFTRender_Row(uint16_t xx,uint16_t yy,uint16_t count,const uint16_t *data)
{ x=xx;y=yy;width=count;column=row=0U;memcpy(pixels,data,count*sizeof(uint16_t));job=ROW; }
void TFTRender_Line(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint16_t c)
{
  lx=x0;ly=y0;end_x=x1;end_y=y1;color=c;
  dx=end_x>lx ? end_x-lx:lx-end_x;
  dy=-(end_y>ly ? end_y-ly:ly-end_y);
  sx=lx<end_x?1:-1;sy=ly<end_y?1:-1;error=dx+dy;job=LINE;
}
void TFTRender_Process(void)
{
  if (job==LINE) {
    for (uint16_t i=0;i<TFT_DRAW_BUDGET;++i) {
      int32_t twice;
      TFTIO_Row((uint16_t)lx,(uint16_t)ly,1U,color,0);
      if (lx==end_x && ly==end_y) { job=IDLE;break; }
      twice=2*error;
      if (twice>=dy) { error+=dy;lx+=sx; }
      if (twice<=dx) { error+=dx;ly+=sy; }
    }
  } else if (job==RECT || job==ROW) {
    uint16_t count=width-column;
    if (count>TFT_DRAW_BUDGET) count=TFT_DRAW_BUDGET;
    TFTIO_Row(x+column,y+row,count,color,job==ROW?pixels+column:0);
    column+=count;
    if (column==width) {
      column=0U;
      if (job==ROW || ++row==height) job=IDLE;
    }
  }
}
