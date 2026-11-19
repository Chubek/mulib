/* MuLIB graphical extension: immgtk-clipper. C11, header-only. */
#ifndef MULIB_IMMGTK_CLIPPER_H
#define MULIB_IMMGTK_CLIPPER_H
#include "mulib-immgtk-native.h"
typedef struct{size_t first,last;}mig_range;
static inline mig_range mulib_immgtk_clipper(size_t count,double offset,double height,double row){if(!isfinite(offset+height+row)||height<=0||row<=0)return (mig_range){0,0};double a=floor(fmax(0,offset)/row),b=ceil(fmax(0,offset+height)/row);size_t first=a>=(double)count?count:(size_t)a,last=b>=(double)count?count:(size_t)b;return (mig_range){first,last<first?first:last};}
#endif
