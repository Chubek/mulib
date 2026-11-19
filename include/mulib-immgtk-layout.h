/* MuLIB graphical extension: immgtk-layout. C11, header-only. */
#ifndef MULIB_IMMGTK_LAYOUT_H
#define MULIB_IMMGTK_LAYOUT_H
#include "mulib-immgtk-native.h"
typedef struct{mgx_rect box;int cursor,gap,row,horizontal;}mulib_immgtk_layout_t;
static inline void mulib_immgtk_layout_init(mulib_immgtk_layout_t *l,mgx_rect b,int row,int gap,int horiz){*l=(mulib_immgtk_layout_t){b,0,mgx_max(0,gap),mgx_max(1,row),horiz};}
static inline mgx_rect mulib_immgtk_layout_next(mulib_immgtk_layout_t *l,int extent){if(extent<=0)extent=l->row;mgx_rect r=l->box;if(l->horizontal){r.x+=l->cursor;r.w=extent;}else{r.y+=l->cursor;r.h=extent;}l->cursor=mgx_int((double)l->cursor+extent+l->gap);return mgx_intersect(r,l->box);}
#endif
