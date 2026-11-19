/* MuLIB graphical extension: immgtk-terminal. C11, header-only. */
#ifndef MULIB_IMMGTK_TERMINAL_H
#define MULIB_IMMGTK_TERMINAL_H
#include "mulib-immgtk-native.h"
typedef struct{char *cells;size_t w,h,x,y;int escape;}mig_terminal;
static inline int mulib_immgtk_terminal_init(mig_terminal *t,size_t w,size_t h){if(!w||!h||w>SIZE_MAX/h)return 0;char *p=malloc(w*h);if(!p)return 0;memset(p,' ',w*h);*t=(mig_terminal){p,w,h,0,0,0};return 1;}
static inline void mulib_immgtk_terminal_feed(mig_terminal *t,const char *p,size_t n){if(!t->cells)return;while(n--){unsigned char c=*p++;if(t->escape){if(c>=0x40&&c<=0x7e&&!(t->escape==1&&c=='['))t->escape=0;else t->escape=2;continue;}if(c==27){t->escape=1;continue;}if(c==13){t->x=0;continue;}if(c==8){if(t->x)t->x--;continue;}if(c==10){t->x=0;t->y++;}else if(c>=32&&c<127){t->cells[t->y*t->w+t->x++]=c;if(t->x==t->w){t->x=0;t->y++;}}if(t->y==t->h){memmove(t->cells,t->cells+t->w,(t->h-1)*t->w);memset(t->cells+(t->h-1)*t->w,' ',t->w);t->y--;}}}
static inline void mulib_immgtk_terminal_draw(mulib_immgtk_native_t *g,const mig_terminal *t,mgx_rect r){mig_flush(g);if(!g->font||!g->surface)return;mgx_rect old=g->surface->clip;g->surface->clip=mgx_intersect(old,r);for(size_t i=0;i<t->h&&i<(size_t)r.h/g->theme.font_size;i++)mulib_glx_text_draw(g->surface,g->font,r.x,r.y+(int)i*g->theme.font_size,t->cells+i*t->w,t->w,g->theme.font_size,g->theme.text);g->surface->clip=old;}
static inline void mulib_immgtk_terminal_free(mig_terminal *t){free(t->cells);memset(t,0,sizeof *t);}
#endif
