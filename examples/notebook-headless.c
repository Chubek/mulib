/* Device-free notebook shell. Build from the repository root:
 * cc -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude examples/notebook-headless.c \
 *    -lm -ldl -o notebook-headless
 * ./notebook-headless --ppm notebook.ppm [--scroll 220] [--save notebook.mnb]
 */
#define MULIB_IMPL_UTF8
#include "notebook-headless.h"
#include "mulib-nb-store.h"
#include "mulib-wm-focus.h"

#include <inttypes.h>

enum {
    INSET_X = 18, INSET_TOP = 44, INSET_BOTTOM = 14, GAP = 8,
    HEADING_H = 26, BODY_X = 10, BODY_Y = 32, BODY_BOTTOM = 6,
    CLIENT_W = 208, CLIENT_H = 76, HEIGHT_LIMIT = 4096,
    SOURCE_LIMIT = 4095, LANG_LIMIT = 63, NAME_LIMIT = 127,
    STRING_BUDGET = (SOURCE_LIMIT + LANG_LIMIT + 2) * MNB_DEMO_CELLS +
                    NAME_LIMIT + 1
};

static const uint32_t desk_color = UINT32_C(0xff151e2c);
static const uint32_t heading_color = UINT32_C(0xff34465e);
static const uint32_t selection_color = UINT32_C(0xff3b5872);

static int fixture(mulib_nb *doc)
{
    static const struct {
        mulib_nb_kind kind;
        const char *source, *language;
        double height;
    } cells[] = {
        {MULIB_NB_MARKDOWN, "A notebook is the document, not the running app.", "", 60},
        {MULIB_NB_CODE, "// This is editable text, never evaluated.\nlet answer = 42", "mune", 94},
        {MULIB_NB_APP, "Chart preview", "", 112},
        {MULIB_NB_CODE, "print('another source cell')", "python", 66},
        {MULIB_NB_APP, "Inspector", "", 120},
        {MULIB_NB_RAW, "End of notebook.", "", 52}
    };
    if (!mulib_nb_init(doc, 1, "MuLIB / Notebook desktop")) return 0;
    for (size_t i = 0; i < sizeof cells / sizeof *cells; ++i)
        if (!mulib_nb_insert(doc, doc->n, cells[i].kind, cells[i].source,
                             cells[i].language, cells[i].height)) return 0;
    return 1;
}

static int displayable_doc(const mulib_nb *doc)
{
    size_t length;
    if (!mulib_nb_text_length(doc->name, NAME_LIMIT, &length)) return 0;
    for (size_t i = 0; i < doc->n; ++i) {
        const mulib_nb_cell *c = &doc->v[i];
        if (c->height > HEIGHT_LIMIT ||
            !mulib_nb_text_length(c->source, SOURCE_LIMIT, &length) ||
            !mulib_nb_text_length(c->language, LANG_LIMIT, &length)) return 0;
    }
    return 1;
}

static int add_fake_client(mnb_demo *d, const mulib_nb_cell *cell)
{
    if (d->client_count >= sizeof d->clients / sizeof *d->clients) return 0;
    size_t ordinal = d->client_count++;
    mnb_demo_client *c = &d->clients[ordinal];
    c->cell = cell->id;
    c->window = mulib_wm_add(&d->wm, MULIB_WM_WAYLAND, ordinal + 1,
                             cell->source, "notebook-headless.fake");
    if (!c->window || !mulib_wm_place(&d->wm, c->window,
                                     (mulib_wm_rect){0, 0, CLIENT_W, CLIENT_H}) ||
        !mulib_nb_window_attach(&d->doc, &d->wm, c->cell, c->window) ||
        !mgx_surface_init(&c->pixels, CLIENT_W, CLIENT_H)) return 0;
    mulib_wm_find(&d->wm, c->window)->flags |= MULIB_WM_MAPPED;

    /* A fake producer supplies straight ARGB pixels, including translucent
     * pixels so the sample exercises source-over rather than opaque blitting. */
    uint32_t color = ordinal ? UINT32_C(0xffce9963) : UINT32_C(0xff54bdb4);
    mgx_clear(&c->pixels, ordinal ? UINT32_C(0x80cd693e) : UINT32_C(0x804da6a0));
    mgx_fill(&c->pixels, (mgx_rect){0, 0, CLIENT_W, 18}, UINT32_C(0xff173044));
    for (int i = 0; i < 8; ++i) {
        int h = 6 + (i * 11) % 29;
        mgx_fill(&c->pixels, (mgx_rect){14 + i * 24, 68 - h, 12, h}, color);
    }
    mulib_glx_text_draw(&c->pixels, &d->font, 6, 4, cell->source,
                        strlen(cell->source), 11, UINT32_C(0xfff0f3f8));

    if (!mulib_glx_clientbuffer_from_surface(&c->buffer, &c->pixels,
                                             CLIENT_W * CLIENT_H, NULL, NULL))
        return 0;
    mulib_glx_scenenode view = mulib_glx_scenenode_default();
    view.buffer = &c->buffer;
    c->node = mulib_glx_scene_add(&d->scene, d->canvas_node, &view);
    return c->node != 0;
}

int mnb_demo_init(mnb_demo *d, const char *path)
{
    mulib_immgtk_native_init(&d->immediate);
    if (!mulib_immgtk_native_limits(&d->immediate, MNB_DEMO_CELLS,
                                   MNB_DEMO_CELLS, 8, 8)) return 0;
    d->io.max_events = 8;
    d->io.max_text = sizeof ((mgx_event){0}).text - 1;
    mulib_glx_platform_headless(&d->platform);
    if (!(path ? mulib_nb_store_load(path, &d->doc, MNB_DEMO_CELLS, STRING_BUDGET) :
                 fixture(&d->doc)) ||
        !displayable_doc(&d->doc) || !mulib_glx_font_builtin(&d->font) ||
        !mulib_glx_window_init(&d->output, &d->platform, 1, "MuLIB notebook",
                               MNB_DEMO_WIDTH, MNB_DEMO_HEIGHT)) return 0;
    d->immediate.font = &d->font;
    d->title = mulib_retgtk_label(1, d->doc.name);
    d->status = mulib_retgtk_label(2, "HEADLESS / NO EXECUTION");
    if (!d->title || !d->status) return 0;

    mulib_glx_scenenode view = mulib_glx_scenenode_default();
    view.accept_input = 0;
    d->clip_node = mulib_glx_scene_add(&d->scene, 0, &view);
    if (!d->clip_node) return 0;
    d->canvas_node = mulib_glx_scene_add(&d->scene, d->clip_node, &view);
    if (!d->canvas_node) return 0;
    if (!path)
        for (size_t i = 0; i < d->doc.n; ++i)
            if (d->doc.v[i].kind == MULIB_NB_APP &&
                !add_fake_client(d, &d->doc.v[i])) return 0;
    return mnb_demo_view(d, 0, 1, MNB_DEMO_WIDTH, MNB_DEMO_HEIGHT);
}

static void free_chrome(mnb_demo_chrome *chrome)
{
    mulib_retgtk_widget_unref(chrome->heading);
    mulib_texteditor_free(&chrome->editor);
    *chrome = (mnb_demo_chrome){0};
}

void mnb_demo_free(mnb_demo *d)
{
    /* Drop the scene's refs before dropping our refs and their pixel storage. */
    mulib_glx_scene_free(&d->scene);
    for (size_t i = 0; i < d->client_count; ++i) {
        mulib_glx_clientbuffer_unref(&d->clients[i].buffer);
        mgx_surface_free(&d->clients[i].pixels);
    }
    for (size_t i = 0; i < MNB_DEMO_CELLS; ++i) free_chrome(&d->chrome[i]);
    mulib_retgtk_widget_unref(d->status);
    mulib_retgtk_widget_unref(d->title);
    mulib_immgtk_io_free(&d->io);
    mulib_immgtk_native_free(&d->immediate);
    mulib_glx_font_free(&d->font);
    mulib_glx_window_free(&d->output);
    mulib_glx_platform_close(&d->platform);
    mulib_nb_layout_free(&d->layout);
    mulib_wm_core_free(&d->wm);
    mulib_nb_free(&d->doc);
    memset(d, 0, sizeof *d);
}

static int sync_layout(mnb_demo *d)
{
    if (mulib_nb_layout_current(&d->layout, &d->doc)) return 1;
    return d->doc.n <= MNB_DEMO_CELLS && displayable_doc(&d->doc) &&
           mulib_nb_layout_build(&d->layout, &d->doc, GAP, HEADING_H);
}

static int set_viewport(mnb_demo *d, mulib_nb_viewport *out, double scroll,
                        double zoom, mgx_rect clip)
{
    return mulib_nb_viewport_set(out, scroll, zoom, clip.w, clip.h,
                                 mulib_nb_layout_prefix(&d->layout, d->layout.n));
}

static void invalidate_gui_input(mnb_demo *d)
{
    /* Previous-frame widget rectangles may no longer match. The logical
     * selection and fake native focus are independent of widget lifetime. */
    mulib_immgtk_io_reset(&d->io);
    mulib_immgtk_native_invalidate(&d->immediate);
    d->ready = 0;
}

int mnb_demo_view(mnb_demo *d, double scroll, double zoom, int w, int h)
{
    if (w < 160 || h < 96 || w > 2048 || h > 2048 || !isfinite(scroll) ||
        !isfinite(zoom) || zoom < .5 || zoom > 2) return 0;
    if (!mulib_nb_layout_current(&d->layout, &d->doc)) invalidate_gui_input(d);
    if (!sync_layout(d)) return 0;
    mgx_rect clip = {INSET_X, INSET_TOP, w - 2 * INSET_X, h - INSET_TOP - INSET_BOTTOM};
    mulib_nb_viewport next;
    if (!set_viewport(d, &next, scroll, zoom, clip)) return 0;
    if ((w != d->output.bounds.w || h != d->output.bounds.h) &&
        !mulib_glx_window_resize(&d->output, w, h)) return 0;
    d->viewport = next;
    d->clip = clip;
    invalidate_gui_input(d);
    return 1;
}

static mgx_rect cell_band(const mnb_demo *d, double top, double bottom, int inset)
{
    /* Round edges using the scene renderer's pixel-center convention. */
    int y = (int)ceil(d->clip.y + mulib_nb_viewport_y(&d->viewport, top) - .5);
    int end = (int)ceil(d->clip.y + mulib_nb_viewport_y(&d->viewport, bottom) - .5);
    int x = (int)ceil(inset * d->viewport.zoom - .5);
    return (mgx_rect){d->clip.x + x, y, d->clip.w - 2 * x, mgx_max(0, end - y)};
}

static double cell_height(const mnb_demo *d, size_t i)
{
    return fmax(0, d->layout.extent[i] - GAP);
}

static const char *cell_kind(mulib_nb_kind kind)
{
    switch (kind) {
    case MULIB_NB_MARKDOWN: return "Markdown";
    case MULIB_NB_CODE: return "Code";
    case MULIB_NB_APP: return "Native app";
    case MULIB_NB_TERMINAL: return "Terminal";
    case MULIB_NB_MEDIA: return "Media";
    case MULIB_NB_GROUP: return "Group";
    default: return "Raw";
    }
}

static int paint_cell(mnb_demo *d, size_t i, mret_paint *paint)
{
    mulib_nb_cell *cell = &d->doc.v[i];
    mnb_demo_chrome *chrome = &d->chrome[i];
    double top = mulib_nb_layout_prefix(&d->layout, i);
    double height = cell_height(d, i), bottom = top + height;
    mgx_rect card = cell_band(d, top, bottom, 0);
    mgx_rect head = cell_band(d, top, fmin(bottom, top + HEADING_H), 0);
    int selected = d->selected_cell == cell->id;
    if (chrome->cell != cell->id || chrome->revision != cell->revision)
        free_chrome(chrome);
    if (!chrome->heading) {
        char label[LANG_LIMIT + 32];
        snprintf(label, sizeof label, "%s%s%s", cell_kind(cell->kind),
                 *cell->language ? " / " : "", cell->language);
        chrome->heading = mulib_retgtk_label(cell->id, label);
        if (!chrome->heading) return 0;
        chrome->cell = cell->id;
        chrome->revision = cell->revision;
    }
    mgx_fill(paint->surface, card, paint->theme.panel);
    mgx_fill(paint->surface, head, selected ? selection_color : heading_color);
    if (selected) mgx_fill(paint->surface, (mgx_rect){head.x, head.y, 3, head.h},
                           paint->theme.accent);
    mulib_retgtk_widget_arrange(chrome->heading, head);
    mulib_retgtk_widget_paint(chrome->heading, paint);

    mgx_rect body = cell_band(d, top + BODY_Y, bottom - BODY_BOTTOM, BODY_X);
    mgx_rect exposed = mgx_intersect(body, paint->surface->clip);
    if (exposed.w <= 0 || exposed.h <= 0) return 1;
    if (!mulib_immgtk_clip_push(&d->immediate, body)) return 0;
    int ok = 1;
    if (cell->kind == MULIB_NB_CODE) {
        char id[32];
        snprintf(id, sizeof id, "code-%" PRIu64, cell->id);
        if (!chrome->editor.text.data)
            ok = mulib_texteditor_init_with_limits(&chrome->editor, cell->source,
                                                   strlen(cell->source), 16, 65536);
        if (ok && mulib_immgtk_codeeditor(&d->immediate, &d->io, id, body,
                                          &chrome->editor)) {
            /* Commit after painting all cells. Editing the document invalidates
             * the Fenwick index, even for an edit that preserves cell height. */
            d->pending_edit = i + 1;
        } else if (d->io.text.n && d->immediate.focus == mig_id(&d->immediate, id)) {
            ok = 0; /* Editor rejected a queued edit, e.g. on allocation failure. */
        }
    } else if (cell->kind != MULIB_NB_APP || !cell->window) {
        mig_label(&d->immediate, body, cell->kind == MULIB_NB_APP ?
                  "No client attached" : cell->source);
    }
    return mulib_immgtk_clip_pop(&d->immediate) && ok;
}

static int commit_edit(mnb_demo *d)
{
    if (!d->pending_edit) return 1;
    size_t i = d->pending_edit - 1;
    d->pending_edit = 0;
    mulib_nb_cell *cell = &d->doc.v[i];
    mulib_texteditor_t *editor = &d->chrome[i].editor;
    if (!mulib_nb_edit(&d->doc, cell->id, editor->text.data, cell->language)) {
        mulib_texteditor_free(editor);
        return 0;
    }
    d->chrome[i].revision = cell->revision;
    return sync_layout(d);
}

/* Both rendering and picking use this clip / scroll-zoom / client hierarchy.
 * The slot clip confines fake clients to their notebook card; no resize or
 * native configure is sent on scroll, zoom, clipping or output resize. */
static int sync_scene(mnb_demo *d)
{
    mulib_glx_scenenode view = mulib_glx_scenenode_default();
    view.accept_input = 0;
    view.x = d->clip.x;
    view.y = d->clip.y;
    view.clipped = 1;
    if (!mulib_wm_rect_from_mgx((mgx_rect){0, 0, d->clip.w, d->clip.h}, &view.clip) ||
        !mulib_glx_scene_set_view(&d->scene, d->clip_node, &view)) return 0;
    view = mulib_glx_scenenode_default();
    view.accept_input = 0;
    view.y = mulib_nb_viewport_y(&d->viewport, 0);
    view.sx = view.sy = d->viewport.zoom;
    if (!mulib_glx_scene_set_view(&d->scene, d->canvas_node, &view)) return 0;
    for (size_t j = 0; j < d->client_count; ++j) {
        mnb_demo_client *client = &d->clients[j];
        mulib_nb_cell *cell = mulib_nb_find(&d->doc, client->cell);
        view = mulib_glx_scenenode_default();
        view.visible = cell && cell->window == client->window;
        if (view.visible) {
            size_t i = (size_t)(cell - d->doc.v);
            view.x = BODY_X;
            view.y = mulib_nb_layout_prefix(&d->layout, i) + BODY_Y;
            view.clipped = 1;
            view.clip = (mulib_wm_rect){0, 0, CLIENT_W,
                mgx_max(0, (int)floor(cell_height(d, i) - BODY_Y - BODY_BOTTOM))};
        }
        if (!mulib_glx_scene_set_view(&d->scene, client->node, &view)) return 0;
    }
    return 1;
}

int mnb_demo_render(mnb_demo *d)
{
    d->ready = 0;
    if (!mulib_nb_layout_current(&d->layout, &d->doc)) invalidate_gui_input(d);
    if (!sync_layout(d) || !set_viewport(d, &d->viewport, d->viewport.scroll,
                                         d->viewport.zoom, d->clip) ||
        !sync_scene(d)) return 0;
    mgx_surface *frame = mulib_glx_swapchain_acquire(&d->output.swap);
    if (!frame) return 0;
    mgx_clear(frame, desk_color);
    mret_paint paint = {frame, &d->font, mgui_default_theme()};
    mgx_fill(frame, d->clip, paint.theme.background);
    int status_w = frame->w >= 400 ? 194 : 0;
    mulib_retgtk_widget_arrange(d->title,
        (mgx_rect){INSET_X, 6, frame->w - 2 * INSET_X - status_w, 32});
    mulib_retgtk_widget_arrange(d->status,
        (mgx_rect){frame->w - INSET_X - status_w, 6, status_w, 32});
    mulib_retgtk_widget_paint(d->title, &paint);
    mulib_retgtk_widget_paint(d->status, &paint);
    frame->clip = d->clip;
    mulib_immgtk_io_begin(&d->immediate, frame, &d->io);
    d->visible = mulib_nb_virtualize(&d->layout, &d->viewport, 0);
    int ok = 1;
    for (size_t i = 0; i < MNB_DEMO_CELLS; ++i) {
        if (i >= d->visible.first && i < d->visible.last && cell_height(d, i) > 0)
            ok = paint_cell(d, i, &paint) && ok;
        else free_chrome(&d->chrome[i]);
    }
    mulib_immgtk_native_end(&d->immediate);
    ok = commit_edit(d) && ok;
    mulib_immgtk_io_reset(&d->io);
    frame->clip = (mgx_rect){0, 0, frame->w, frame->h};

    size_t words;
    if (!ok || d->immediate.error ||
        !mgx_surface_required_words(frame->stride, frame->w, frame->h, &words) ||
        !mulib_glx_scene_render_surface(&d->scene, frame, words) ||
        !mulib_glx_window_present(&d->output)) {
        (void)mulib_glx_swapchain_cancel(&d->output.swap);
        return 0;
    }
    d->ready = 1;
    return 1;
}

int mnb_demo_pick(mnb_demo *d, double x, double y, mnb_demo_hit *hit)
{
    if (!d->ready || !mulib_nb_layout_current(&d->layout, &d->doc) ||
        !hit || !isfinite(x) || !isfinite(y)) return 0;
    *hit = (mnb_demo_hit){0};
    if (!mgx_contains(d->clip, x, y)) return 1;
    double doc_y = mulib_nb_viewport_inverse(&d->viewport, y - d->clip.y);
    size_t i = mulib_nb_layout_at(&d->layout, doc_y);
    if (i >= d->doc.n ||
        doc_y >= mulib_nb_layout_prefix(&d->layout, i) + cell_height(d, i)) return 1;
    hit->cell = d->doc.v[i].id;
    hit->native = (mulib_glx_hit){.x = x, .y = y};
    /* Propagate traversal failure instead of treating it as a GUI hit. */
    return mulib_glx_scene_walk(&d->scene, mulib_glx_hit_visit, &hit->native);
}

int mnb_demo_click(mnb_demo *d, double x, double y)
{
    mnb_demo_hit hit;
    if (!mnb_demo_pick(d, x, y, &hit)) return 0;
    d->ready = 0;
    d->selected_cell = hit.cell;
    if (hit.native.id) {
        for (size_t i = 0; i < d->client_count; ++i)
            if (d->clients[i].node == hit.native.id && d->clients[i].cell == hit.cell) {
                mulib_immgtk_native_invalidate(&d->immediate);
                return mulib_wm_focus(&d->wm, d->clients[i].window, 0);
            }
        return 0;
    }

    /* A click in chrome can focus a widget but can never send native input. */
    mgx_event motion = {.type = MGX_EVENT_MOTION, .x = x, .y = y};
    mgx_event down = {.type = MGX_EVENT_BUTTON, .down = 1};
    mgx_event up = {.type = MGX_EVENT_BUTTON};
    return mulib_wm_focus(&d->wm, 0, 0) &&
           mulib_immgtk_io_feed(&d->io, &motion) &&
           mulib_immgtk_io_feed(&d->io, &down) &&
           mulib_immgtk_io_feed(&d->io, &up);
}

int mnb_demo_text(mnb_demo *d, const char *utf8)
{
    if (!d->ready || !mulib_nb_layout_current(&d->layout, &d->doc) ||
        !d->immediate.focus || d->wm.focused || !utf8) return 0;
    mulib_nb_cell *cell = mulib_nb_find(&d->doc, d->selected_cell);
    if (!cell || cell->kind != MULIB_NB_CODE) return 0;
    size_t n;
    mgx_event event = {.type = MGX_EVENT_TEXT};
    if (!mulib_nb_text_length(utf8, sizeof event.text - 1, &n) ||
        !n || n > SOURCE_LIMIT - strlen(cell->source)) return 0;
    memcpy(event.text, utf8, n + 1);
    if (!mulib_immgtk_io_feed(&d->io, &event)) return 0;
    d->ready = 0;
    return 1;
}

const mgx_surface *mnb_demo_frame(const mnb_demo *d)
{
    return d->ready ? &d->output.swap.image[d->output.swap.front] : NULL;
}

#ifndef MULIB_NOTEBOOK_HEADLESS_NO_MAIN
/* As with gfx-headless.c, dump logical RGB bytes (rather than host word order). */
static int write_ppm(const mgx_surface *s, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fprintf(f, "P6\n%d %d\n255\n", s->w, s->h) > 0;
    for (int y = 0; ok && y < s->h; ++y)
        for (int x = 0; x < s->w; ++x) {
            uint32_t p = s->pixels[(size_t)y * s->stride + (size_t)x];
            unsigned char rgb[] = {(unsigned char)(p >> 16), (unsigned char)(p >> 8),
                                   (unsigned char)p};
            if (fwrite(rgb, 1, sizeof rgb, f) != sizeof rgb) { ok = 0; break; }
        }
    return fclose(f) == 0 && ok;
}

int main(int argc, char **argv)
{
    const char *ppm = NULL, *load = NULL, *save = NULL, *scroll_arg = NULL;
    double scroll = 0;
    for (int i = 1; i < argc; i += 2) {
        const char **option = !strcmp(argv[i], "--ppm") ? &ppm :
                              !strcmp(argv[i], "--load") ? &load :
                              !strcmp(argv[i], "--save") ? &save :
                              !strcmp(argv[i], "--scroll") ? &scroll_arg : NULL;
        if (!option || i + 1 >= argc || *option) {
            fprintf(stderr, "usage: %s [--load input.mnb] [--save output.mnb]"
                    " [--scroll document-y] [--ppm frame.ppm]\n", argv[0]);
            return 2;
        }
        *option = argv[i + 1];
    }
    if (scroll_arg) {
        char *end;
        scroll = strtod(scroll_arg, &end);
        if (end == scroll_arg || *end || !isfinite(scroll)) {
            fprintf(stderr, "invalid scroll position: %s\n", scroll_arg);
            return 2;
        }
    }
    mnb_demo d = {0};
    int ok = mnb_demo_init(&d, load) &&
             (!scroll_arg || mnb_demo_view(&d, scroll, 1, MNB_DEMO_WIDTH, MNB_DEMO_HEIGHT)) &&
             mnb_demo_render(&d);
    if (!ok) fprintf(stderr, "could not initialize or render the notebook\n");
    else if (save && !mulib_nb_store_save(save, &d.doc)) {
        fprintf(stderr, "could not save notebook: %s\n", save);
        ok = 0;
    } else if (ppm && !write_ppm(mnb_demo_frame(&d), ppm)) {
        fprintf(stderr, "could not write image: %s\n", ppm);
        ok = 0;
    }
    mnb_demo_free(&d);
    return ok ? 0 : 1;
}
#endif
