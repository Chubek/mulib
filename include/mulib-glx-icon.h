/* MuLIB graphical extension: glx-icon. C11, header-only. */
#ifndef MULIB_GLX_ICON_H
#define MULIB_GLX_ICON_H
#include "mulib-glx-core.h"
typedef enum{MGX_ICON_CHECK,MGX_ICON_CLOSE,MGX_ICON_PLUS,MGX_ICON_ARROW}mulib_glx_icon_t;
static inline void mulib_glx_icon_draw(mgx_surface *s,mgx_rect r,mulib_glx_icon_t k,uint32_t c){double x=r.x,y=r.y,w=r.w,h=r.h,t=fmax(1,fmin(w,h)/8);mgx_point a={x+w*.2,y+h*.2},b={x+w*.8,y+h*.8};if(k==MGX_ICON_CHECK){mgx_line(s,(mgx_point){x+w*.15,y+h*.5},(mgx_point){x+w*.4,y+h*.75},t,c);mgx_line(s,(mgx_point){x+w*.4,y+h*.75},(mgx_point){x+w*.85,y+h*.2},t,c);}else if(k==MGX_ICON_CLOSE){mgx_line(s,a,b,t,c);mgx_line(s,(mgx_point){a.x,b.y},(mgx_point){b.x,a.y},t,c);}else if(k==MGX_ICON_PLUS){mgx_line(s,(mgx_point){a.x,y+h/2},(mgx_point){b.x,y+h/2},t,c);mgx_line(s,(mgx_point){x+w/2,a.y},(mgx_point){x+w/2,b.y},t,c);}else{mgx_line(s,(mgx_point){a.x,a.y},(mgx_point){b.x,y+h/2},t,c);mgx_line(s,(mgx_point){b.x,y+h/2},(mgx_point){a.x,b.y},t,c);}}
#endif
