/* MuLIB graphical extension: immgtk-table. C11, header-only. */
#ifndef MULIB_IMMGTK_TABLE_H
#define MULIB_IMMGTK_TABLE_H
#include "mulib-immgtk-native.h"
typedef struct{mgx_rect bounds;const double *weights;size_t columns;int row_height;}mulib_immgtk_table_t;
static inline mgx_rect mulib_immgtk_table_cell(const mulib_immgtk_table_t *t,size_t row,size_t col){if(!t||!t->weights||col>=t->columns||t->row_height<=0)return (mgx_rect){0,0,0,0};double total=0,prior=0;for(size_t i=0;i<t->columns;i++){if(!isfinite(t->weights[i])||t->weights[i]<0)return (mgx_rect){0,0,0,0};total+=t->weights[i];if(i<col)prior+=t->weights[i];}if(total<=0)return (mgx_rect){0,0,0,0};int a=(int)(t->bounds.w*prior/total),b=(int)(t->bounds.w*(prior+t->weights[col])/total);return mgx_intersect(t->bounds,(mgx_rect){t->bounds.x+a,mgx_int(t->bounds.y+(double)row*t->row_height),b-a,t->row_height});}
#endif
