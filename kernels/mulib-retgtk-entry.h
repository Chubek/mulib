/* MuLIB graphical extension: retgtk-entry. C11, header-only. */
#ifndef MULIB_RETGTK_ENTRY_H
#define MULIB_RETGTK_ENTRY_H
#include "mulib-retgtk-widget.h"
#include "mulib-retgtk-kind.h"
#include "mulib-texteditor.h"
static inline void mret_entry_destroy(mulib_retgtk_widget_t *w){mulib_texteditor_t *e=w->data;if(e){mulib_texteditor_free(e);free(e);}}
static inline int mret_entry_event(mulib_retgtk_widget_t *w,const mgx_event *e)
{
    if (!e || !mulib_retgtk_widget_is_interactive(w)) return 0;
    mulib_texteditor_t *t = w->data;
    int changed = 0;
    if (e->type == MGX_EVENT_TEXT && w->focused) {
        size_t n = 0;
        while (n < sizeof e->text && e->text[n]) ++n;
        changed = mulib_texteditor_insert(t, e->text, n);
    } else if (e->type == MGX_EVENT_KEY && w->focused && e->down) {
        if (e->code == 14) changed = mulib_texteditor_delete(t, 1);
        else if (e->code == 111) changed = mulib_texteditor_delete(t, 0);
        else if (e->code == 105 || e->code == 106) {
            mulib_textselection_move(&t->selection, &t->text,
                                    e->code == 105 ? -1 : 1, e->mods & 1);
            return 1;
        } else if ((e->mods & 2) && (e->code == 44 || e->code == 21))
            changed = mulib_texteditor_undo(t, e->code == 21);
    } else return mret_control_event(w, e);
    if (changed) {
        mulib_retgtk_widget_text(w, t->text.data);
        mulib_retgtk_signal_emit(&w->signals, w, MRET_CHANGED, t->text.data);
    }
    return changed;
}
static const mulib_retgtk_widget_ops mret_entry_ops={mret_entry_destroy,NULL,NULL,mret_control_paint,mret_entry_event};
static inline mulib_retgtk_widget_t *mulib_retgtk_entry(uint64_t id,const char *text){mulib_retgtk_widget_t *w=mret_control(id,text,&mret_entry_ops);if(!w)return NULL;mulib_texteditor_t *e=calloc(1,sizeof *e);if(!e||!mulib_texteditor_init(e,text?text:"",text?strlen(text):0)){free(e);mulib_retgtk_widget_unref(w);return NULL;}w->data=e;w->kind=MRET_KIND_ENTRY;return w;}
#endif
