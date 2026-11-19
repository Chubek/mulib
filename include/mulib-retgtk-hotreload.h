/* MuLIB graphical extension: retgtk-hotreload. C11, header-only. */
#ifndef MULIB_RETGTK_HOTRELOAD_H
#define MULIB_RETGTK_HOTRELOAD_H
#include "mulib-retgtk-uiml.h"
#include "mulib-retgtk-tree.h"
#include "mulib-retgtk-scene.h"
#include "mulib-retgtk-entry.h"
/* Match explicit stable kinds, never header-local operation-table addresses.
 * Untyped/legacy containers retain the new document's state. Entry text and
 * selection transfer, but undo history starts a new baseline. Models, signals
 * and router focus/capture are deliberately not copied between trees. */
static inline int mulib_retgtk_hotreload_transfer_checked(
    const mulib_retgtk_widget_t *old, mulib_retgtk_widget_t *fresh)
{
    if (!fresh) return 0;
    if (!old) return 1;
    mulib_retgtk_widget_t *o = mulib_retgtk_tree_find(
        (mulib_retgtk_widget_t *)old, fresh->object.id);
    if (o && o->kind > MRET_KIND_BASE && o->kind == fresh->kind) {
        if (fresh->kind == MRET_KIND_ENTRY && o->data && fresh->data) {
            const mulib_texteditor_t *source = o->data;
            mulib_texteditor_t *editor = calloc(1, sizeof *editor);
            if (!editor) return 0;
            if (!mulib_texteditor_init(editor, source->text.data, source->text.n)) {
                free(editor);
                return 0;
            }
            if (!mulib_retgtk_widget_text(fresh,
                    source->text.data ? source->text.data : "")) {
                mulib_texteditor_free(editor); free(editor);
                return 0;
            }
            if (source->selection.anchor <= source->text.n &&
                source->selection.caret <= source->text.n)
                editor->selection = source->selection;
            mret_entry_destroy(fresh);
            fresh->data = editor;
        }
        fresh->value = o->value;
        mulib_retgtk_widget_set_visible(fresh, o->visible);
    }
    for (size_t i = 0; i < fresh->n; ++i)
        if (!mulib_retgtk_hotreload_transfer_checked(old, fresh->children[i]))
            return 0;
    mulib_retgtk_widget_invalidate(fresh);
    return 1;
}
static inline void mulib_retgtk_hotreload_transfer(
    const mulib_retgtk_widget_t *old, mulib_retgtk_widget_t *fresh)
{
    (void)mulib_retgtk_hotreload_transfer_checked(old, fresh);
}
static inline int mulib_retgtk_hotreload(mulib_retgtk_scene_t *s,
    const mret_builder_factory *f, size_t n, const char *markup)
{
    if (!s) return 0;
    mulib_retgtk_uiml_t u = {0};
    if (!mulib_retgtk_uiml_parse(&u, markup)) return 0;
    mulib_retgtk_widget_t *w = mulib_retgtk_builder_build(f, n, u.v, u.n);
    mulib_retgtk_uiml_free(&u);
    if (!w) return 0;
    if (!mulib_retgtk_hotreload_transfer_checked(s->root, w)) {
        mulib_retgtk_widget_unref(w);
        return 0;
    }
    mulib_retgtk_scene_set(s, w);
    mulib_retgtk_widget_unref(w);
    return 1;
}
#endif
