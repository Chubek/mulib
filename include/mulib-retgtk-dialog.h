/* MuLIB graphical extension: retgtk-dialog. C11, header-only. */
#ifndef MULIB_RETGTK_DIALOG_H
#define MULIB_RETGTK_DIALOG_H
#include "mulib-retgtk-widget.h"
typedef struct{mulib_retgtk_widget_t *content;int open,result;}mulib_retgtk_dialog_t;
static inline void mulib_retgtk_dialog_open(mulib_retgtk_dialog_t *d,mulib_retgtk_widget_t *w){if(w)mulib_retgtk_object_ref(&w->object);mulib_retgtk_widget_unref(d->content);d->content=w;d->open=w!=NULL;d->result=0;}
static inline void mulib_retgtk_dialog_close(mulib_retgtk_dialog_t *d,int result){d->open=0;d->result=result;mulib_retgtk_widget_unref(d->content);d->content=NULL;}
#endif
