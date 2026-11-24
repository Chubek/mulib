/* MuLIB graphical extension: immgtk-plot-stream. C11, header-only. */
#ifndef MULIB_IMMGTK_PLOT_STREAM_H
#define MULIB_IMMGTK_PLOT_STREAM_H
#include "mulib-glx-core.h"
typedef struct{double *v;size_t cap,n,head;}mig_plot_stream;
static inline int mulib_immgtk_plot_stream_init(mig_plot_stream *s,size_t n){if(!n||n>SIZE_MAX/sizeof(double))return 0;s->v=malloc(n*sizeof(double));if(!s->v)return 0;s->cap=n;s->n=s->head=0;return 1;}
static inline void mulib_immgtk_plot_stream_push(mig_plot_stream *s,double v){if(!s->cap)return;s->v[s->head]=v;s->head=(s->head+1)%s->cap;if(s->n<s->cap)s->n++;}
static inline double mulib_immgtk_plot_stream_at(const mig_plot_stream *s,size_t i){return i<s->n?s->v[(s->head+s->cap-s->n+i)%s->cap]:NAN;}
static inline void mulib_immgtk_plot_stream_free(mig_plot_stream *s){free(s->v);memset(s,0,sizeof *s);}
#endif
