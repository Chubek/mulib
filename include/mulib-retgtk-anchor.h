/* MuLIB graphical extension: retgtk-anchor. C11, header-only. */
#ifndef MULIB_RETGTK_ANCHOR_H
#define MULIB_RETGTK_ANCHOR_H
#include "mulib-retgtk-widget.h"
enum{MRET_LEFT=1,MRET_RIGHT=2,MRET_TOP=4,MRET_BOTTOM=8};
static inline mgx_rect mulib_retgtk_anchor(mgx_rect p,mgx_rect child,unsigned flags,int margin){if((flags&3)==3){child.x=p.x+margin;child.w=mgx_max(0,p.w-2*margin);}else if(flags&MRET_RIGHT)child.x=p.x+p.w-child.w-margin;else if(flags&MRET_LEFT)child.x=p.x+margin;if((flags&12)==12){child.y=p.y+margin;child.h=mgx_max(0,p.h-2*margin);}else if(flags&MRET_BOTTOM)child.y=p.y+p.h-child.h-margin;else if(flags&MRET_TOP)child.y=p.y+margin;return child;}
#endif
