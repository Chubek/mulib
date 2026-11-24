/* MuLIB graphical extension: immgtk-colorpicker. C11, header-only. */
#ifndef MULIB_IMMGTK_COLORPICKER_H
#define MULIB_IMMGTK_COLORPICKER_H
#include "mulib-immgtk-native.h"
static inline uint32_t mulib_immgtk_color_hsv(double h,double s,double v,double a){if(!isfinite(h+s+v+a))return 0;h=(h-floor(h))*6;s=mgx_clamp(s,0,1);v=mgx_clamp(v,0,1);double c=v*s,x=c*(1-fabs(fmod(h,2)-1)),m=v-c,r=0,g=0,b=0;int k=(int)h;switch(k){case 0:r=c;g=x;break;case 1:r=x;g=c;break;case 2:g=c;b=x;break;case 3:g=x;b=c;break;case 4:r=x;b=c;break;default:r=c;b=x;}return (uint32_t)(mgx_clamp(a,0,1)*255+.5)<<24|(uint32_t)((r+m)*255+.5)<<16|(uint32_t)((g+m)*255+.5)<<8|(uint32_t)((b+m)*255+.5);}
static inline int mulib_immgtk_colorpicker(mulib_immgtk_native_t *g,const char *id,mgx_rect r,uint32_t *color){if(!color||!mig_push_id(g,id))return 0;int changed=0;const char *keys[]={"A","R","G","B"};for(int i=0;i<4;i++){int shift=24-i*8;double v=mgx_ch(*color,shift);mgx_rect a={r.x,r.y+i*r.h/4,r.w,r.h/4};if(mig_slider(g,keys[i],a,&v,0,255)){*color=(*color&~(255u<<shift))|((uint32_t)(v+.5)<<shift);changed=1;}mig_label(g,a,keys[i]);}mig_pop_id(g);return changed;}
#endif
