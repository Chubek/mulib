/* MuLIB graphical extension: retgtk-model. C11, header-only. */
#ifndef MULIB_RETGTK_MODEL_H
#define MULIB_RETGTK_MODEL_H
#include "mulib-retgtk-widget.h"
typedef struct{void *user;size_t (*rows)(void *);size_t (*columns)(void *);const char *(*cell)(void *,size_t,size_t);int (*set)(void *,size_t,size_t,const char *);uint64_t revision;}mulib_retgtk_model_t;
static inline const char *mulib_retgtk_model_cell(const mulib_retgtk_model_t *m,size_t r,size_t c){if(!m||!m->rows||!m->columns||!m->cell||r>=m->rows(m->user)||c>=m->columns(m->user))return NULL;return m->cell(m->user,r,c);}
static inline int mulib_retgtk_model_index(const mulib_retgtk_model_t *m,size_t r,size_t c){return m&&m->rows&&m->columns&&r<m->rows(m->user)&&c<m->columns(m->user);}
static inline int mulib_retgtk_model_set(mulib_retgtk_model_t *m,size_t r,size_t c,const char *s){if(!s||!mulib_retgtk_model_index(m,r,c)||!m->set||!m->set(m->user,r,c,s))return 0;m->revision++;return 1;}
#endif
