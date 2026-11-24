/* MuLIB graphical extension: immgtk-textedit. C11, header-only. */
#ifndef MULIB_IMMGTK_TEXTEDIT_H
#define MULIB_IMMGTK_TEXTEDIT_H
#include "mulib-immgtk-io.h"
#include "mulib-texteditor.h"
static inline int mulib_immgtk_textedit(mulib_immgtk_native_t *g,mulib_immgtk_io_t *io,const char *id,mgx_rect r,mulib_texteditor_t *e){uint64_t k=mig_id(g,id);mig_interact(g,k,r,1);int changed=0;if(g->focus==k){if(io->keys.pressed[105])mulib_textselection_move(&e->selection,&e->text,-1,io->keys.mods&1);if(io->keys.pressed[106])mulib_textselection_move(&e->selection,&e->text,1,io->keys.mods&1);if(io->keys.pressed[14])changed|=mulib_texteditor_delete(e,1);if(io->keys.pressed[111])changed|=mulib_texteditor_delete(e,0);if((io->keys.mods&2)&&io->keys.pressed[44])changed|=mulib_texteditor_undo(e,0);if((io->keys.mods&2)&&io->keys.pressed[21])changed|=mulib_texteditor_undo(e,1);if(io->text.n)changed|=mulib_texteditor_insert(e,io->text.data,io->text.n);}mig_rect(g,r,g->focus==k?g->theme.active:g->theme.normal);mig_label(g,r,e->text.data?e->text.data:"");if(g->focus==k&&g->font){mgx_point a=mulib_glx_text_measure(g->font,e->text.data,e->selection.caret,g->theme.font_size);mig_rect(g,(mgx_rect){r.x+g->theme.padding+mgx_int(a.x),r.y+4,1,mgx_max(1,r.h-8)},g->theme.text);}return changed;}
#endif
