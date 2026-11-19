/* MuLIB graphical extension: retgtk-tablemodel. C11, header-only. */
#ifndef MULIB_RETGTK_TABLEMODEL_H
#define MULIB_RETGTK_TABLEMODEL_H
#include "mulib-retgtk-model.h"
typedef struct{char **cells;size_t rows,cols;}mulib_retgtk_tablemodel_t;
/* Initialize only an empty object; reject reinitialization rather than leak. */
static inline int mulib_retgtk_tablemodel_init(mulib_retgtk_tablemodel_t *m,size_t rows,size_t cols){if(!m||m->cells||m->rows||m->cols||!rows||!cols||rows>SIZE_MAX/cols/sizeof(char*))return 0;char **c=calloc(rows*cols,sizeof *c);if(!c)return 0;*m=(mulib_retgtk_tablemodel_t){c,rows,cols};return 1;}
static inline int mret_table_set(void *u,size_t r,size_t c,const char *s){mulib_retgtk_tablemodel_t *m=u;if(r>=m->rows||c>=m->cols||!s)return 0;char *p=malloc(strlen(s)+1);if(!p)return 0;strcpy(p,s);free(m->cells[r*m->cols+c]);m->cells[r*m->cols+c]=p;return 1;}
static inline size_t mret_table_rows(void *u){return ((mulib_retgtk_tablemodel_t*)u)->rows;}
static inline size_t mret_table_cols(void *u){return ((mulib_retgtk_tablemodel_t*)u)->cols;}
static inline const char *mret_table_cell(void *u,size_t r,size_t c){mulib_retgtk_tablemodel_t *m=u;if(r>=m->rows||c>=m->cols)return NULL;const char *s=m->cells[r*m->cols+c];return s?s:"";}
static inline mulib_retgtk_model_t mulib_retgtk_tablemodel_interface(mulib_retgtk_tablemodel_t *m){return (mulib_retgtk_model_t){m,mret_table_rows,mret_table_cols,mret_table_cell,mret_table_set,0};}
static inline void mulib_retgtk_tablemodel_free(mulib_retgtk_tablemodel_t *m){for(size_t i=0;i<m->rows*m->cols;i++)free(m->cells[i]);free(m->cells);memset(m,0,sizeof *m);}
#endif
