/* MuLIB graphical extension: gui-ime. C11, header-only. */
#ifndef MULIB_GUI_IME_H
#define MULIB_GUI_IME_H
#include "mulib-texteditor.h"
typedef struct{mulib_textbuffer_t preedit;size_t cursor;int active;}mulib_gui_ime_t;
static inline int mulib_gui_ime_preedit(mulib_gui_ime_t *i,const char *s,size_t n,size_t cursor){if(cursor>n||!mgx_utf8_valid(s,n)||!mgx_utf8_boundary(s,n,cursor)||!mulib_textbuffer_set(&i->preedit,s,n))return 0;i->cursor=cursor;i->active=n!=0;return 1;}
static inline int mulib_gui_ime_commit(mulib_gui_ime_t *i,mulib_texteditor_t *e,const char *s,size_t n){if(!mulib_texteditor_insert(e,s,n))return 0;mulib_textbuffer_free(&i->preedit);i->cursor=0;i->active=0;return 1;}
static inline void mulib_gui_ime_free(mulib_gui_ime_t *i){mulib_textbuffer_free(&i->preedit);memset(i,0,sizeof *i);}
#endif
