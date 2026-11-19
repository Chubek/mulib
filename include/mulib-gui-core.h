/* MuLIB graphical extension: gui-core. C11, header-only. */
#ifndef MULIB_GUI_CORE_H
#define MULIB_GUI_CORE_H
#include "mulib-glx-drawlist.h"
#include "mulib-glx-event.h"
#include "mulib-glx-text.h"
typedef struct{int min_w,min_h,max_w,max_h;}mulib_gui_constraints_t;
typedef struct{int w,h;}mulib_gui_size_t;typedef mgx_rect mulib_gui_rect_t;typedef mgx_event mulib_gui_event_t;
typedef struct{uint32_t background,panel,normal,hover,active,text,accent,border;int padding,gap,row,font_size,radius;}mgui_theme;
static inline mgui_theme mgui_default_theme(void){return (mgui_theme){0xff202630,0xff2b3440,0xff394353,0xff4a586a,0xff426c9f,0xfff0f3f8,0xff56b4e9,0xff68798a,8,4,24,14,4};}
static inline mgx_rect mgui_inset(mgx_rect r,int p){p=mgx_max(0,mgx_min(p,mgx_min(r.w,r.h)/2));return (mgx_rect){r.x+p,r.y+p,r.w-2*p,r.h-2*p};}
/* Internal bounded string/stream helpers. A line is never silently truncated.
 * budget counts input bytes, including line endings; NUL bytes are rejected.
 * read_line returns 1 for a line, 0 for EOF, -1 for malformed/oversized input. */
static inline int mgui_string_length(const char *s,size_t capacity,size_t *length){
 if(!s)return 0;
 for(size_t i=0;i<capacity;i++)if(!s[i]){if(length)*length=i;return 1;}
 return 0;
}
static inline int mgui_read_line(FILE *f,char *line,size_t capacity,size_t *budget){
 size_t n=0;
 if(!f||!line||capacity<2||!budget)return -1;
 for(;;){
  int c=fgetc(f);
  if(c==EOF){if(ferror(f))return -1;if(!n)return 0;break;}
  if(!*budget)return -1;
  --*budget;
  if(!c)return -1;
  if(c=='\n')break;
  if(n==capacity-1)return -1;
  line[n++]=(char)c;
 }
 if(n&&line[n-1]=='\r')--n;
 line[n]=0;
 return strchr(line,'\r')?-1:1;
}
#endif
