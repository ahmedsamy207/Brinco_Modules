/** @file motion_protocol.c
 * Foreground UART grammar, integer units and coherent status formatting.
 * No unsolicited completion events: poll STATUS, which also renews the lease.
 */
#include "motion.h"
#include "motion_protocol.h"
#include <string.h>
/** Strict bounded decimal parser; reject spaces, trailing text and overflow. */
static uint8_t Number(const char *p,int32_t *value)
{
 uint8_t negative=0;uint32_t n=0;
 if(*p=='-' || *p=='+') { negative=*p=='-';p++; }
 if(!*p) return 0;
 while(*p) {
  if(*p<'0' || *p>'9') return 0;
  uint32_t d=(uint32_t)(*p++-'0');
  if(n>(1000000U-d)/10U) return 0;
  n=n*10U+d;
 }
 *value=negative?-(int32_t)n:(int32_t)n;return 1;
}
/** Bounded status writer uses no printf heap or floating-point library. */
static char *Text(char *p,char *end,const char *text)
{
 while(*text && p<end) *p++=*text++;
 *p=0;return p;
}
static char *Unsigned(char *p,char *end,uint32_t value)
{
 char digits[10];uint8_t n=0;
 do { digits[n++]=(char)('0'+value%10U);value/=10U; } while(value);
 while(n && p<end) *p++=digits[--n];
 *p=0;return p;
}
static void Status(char *reply,MotionStatus s,const char *name)
{
 char *p=reply,*end=reply+159;
 p=Text(p,end,"STATUS ");p=Text(p,end,name);p=Text(p,end," POS ");
 if(s.position<0) p=Text(p,end,"-");
 p=Unsigned(p,end,s.position<0?0U-(uint32_t)s.position:(uint32_t)s.position);
 p=Text(p,end," REM ");p=Unsigned(p,end,s.remaining);
 p=Text(p,end," VEL ");p=Unsigned(p,end,(uint32_t)(((uint64_t)s.velocity_q16*1000U)>>16));
 p=Text(p,end," SPEED ");p=Unsigned(p,end,s.speed);
 p=Text(p,end," ACCEL ");p=Unsigned(p,end,s.acceleration);
 p=Text(p,end," FAULT ");p=Unsigned(p,end,s.fault);(void)Text(p,end,"\r\n");
}
void MotionProtocol_Execute(const char *line,char *reply)
{
 uint8_t result=MOTION_BAD_VALUE,matched=0;int32_t value;
 static const char *const names[]={"IDLE","WAKING","MOVING","RUNNING","STOPPING","HOLD","FAULT"};
 if(!strcmp(line,"PING")) { Motion_KeepAlive();strcpy(reply,"PONG\r\n");return; }
 if(!strcmp(line,"INFO")) {
  Motion_KeepAlive();strcpy(reply,"INFO BRINCO STEPPER 1 NFP-15BY-A FULL 20\r\n");return;
 }
 if(!strcmp(line,"STATUS")) {
  MotionStatus s=Motion_GetStatus();Motion_KeepAlive();
  Status(reply,s,names[s.state]);return;
 }
 if(!strcmp(line,"STOP")) { Motion_Stop();result=MOTION_OK;matched=1; }
 else if(!strcmp(line,"RELEASE")) { Motion_Release();result=MOTION_OK;matched=1; }
 else if(!strcmp(line,"ZERO")) { result=Motion_Zero();matched=1; }
 else if(!strcmp(line,"CLEAR")) { result=Motion_Clear();matched=1; }
 else {
  static const char *const verbs[]={"MOVE ","RUN ","SPEED ","ACCEL "};
  for(uint8_t i=0;i<4;i++) {
   size_t n=strlen(verbs[i]);
   if(!strncmp(line,verbs[i],n)) {
    matched=1;
    if(Number(line+n,&value)) {
     if(i==0) result=Motion_Move(value);
     else if(i==1) result=Motion_Run(value);
     else if(i==2 && value>0) result=Motion_Speed((uint32_t)value);
     else if(i==3 && value>0) result=Motion_Acceleration((uint32_t)value);
    }
    break;
   }
  }
 }
 if(!matched) strcpy(reply,"ERR COMMAND\r\n");
 else if(result==MOTION_OK) { Motion_KeepAlive();strcpy(reply,"OK\r\n"); }
 else if(result==MOTION_BUSY) strcpy(reply,"ERR BUSY\r\n");
 else if(result==MOTION_LATCHED) strcpy(reply,"ERR FAULT\r\n");
 else strcpy(reply,"ERR ARG\r\n");
}
