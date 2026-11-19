/* M0 end-to-end regression; link against examples/notebook-headless.c. */
#include "notebook-headless.h"
#include "mulib-nb-store.h"
#include <assert.h>

static uint32_t pixel(const mnb_demo *d, int x, int y)
{
    const mgx_surface *s = mnb_demo_frame(d);
    assert(s && x >= 0 && y >= 0 && x < s->w && y < s->h);
    return s->pixels[(size_t)y * s->stride + (size_t)x];
}

static int has_color(const mnb_demo *d, mgx_rect band, uint32_t color)
{
    for (int y = band.y; y < band.y + band.h; ++y)
        for (int x = band.x; x < band.x + band.w; ++x)
            if (pixel(d, x, y) == color) return 1;
    return 0;
}

static void pick(mnb_demo *d, double x, double y, uint64_t cell,
                 uint64_t node, double lx, double ly)
{
    mnb_demo_hit h;
    assert(mnb_demo_pick(d, x, y, &h));
    if (h.cell != cell || h.native.id != node)
        fprintf(stderr, "pick (%g,%g): got cell %ju/node %ju, expected %ju/%ju\n",
                x, y, (uintmax_t)h.cell, (uintmax_t)h.native.id,
                (uintmax_t)cell, (uintmax_t)node);
    assert(h.cell == cell && h.native.id == node);
    if (node) {
        assert(fabs(h.native.local_x - lx) < 1e-9);
        assert(fabs(h.native.local_y - ly) < 1e-9);
    }
}

static void same_rect(mulib_wm_rect a, mulib_wm_rect b)
{
    assert(a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h);
}

static void client_unchanged(mnb_demo *d, mnb_demo_client *c, mulib_wm_rect rect,
                             const uint32_t *pixels, uint64_t serial)
{
    const mulib_wm_window *w = mulib_wm_find(&d->wm, c->window);
    const mulib_glx_scenenode *node = mulib_glx_scene_find(&d->scene, c->node);
    assert(w && node && node->parent == d->canvas_node && node->buffer == &c->buffer);
    assert(w->cell == c->cell && w->configure_serial == serial);
    assert(c->buffer.pixels == pixels && c->buffer.refs == 2);
    assert(c->buffer.w == rect.w && c->buffer.h == rect.h);
    same_rect(w->rect, rect);
}

static void test_pipeline(const char *saved)
{
    mnb_demo d = {0};
    assert(mnb_demo_init(&d, NULL));
    assert(!mnb_demo_frame(&d));
    assert(d.doc.n == 6 && d.client_count == 2);
    mnb_demo_client *c = &d.clients[0], *c2 = &d.clients[1];
    uint64_t notes = d.doc.v[0].id, code = d.doc.v[1].id, app = d.doc.v[2].id;
    assert(c->cell == app && d.doc.v[2].window == c->window);
    const mulib_wm_window *w = mulib_wm_find(&d.wm, c->window);
    assert(w);
    mulib_wm_rect native = w->rect;
    const uint32_t *storage = c->buffer.pixels;
    uint64_t serial = w->configure_serial;

    // Initial viewport contains three different cell heights. Chrome for
    // off-screen cells must not be instantiated, but both clients stay live.
    assert(mnb_demo_render(&d));
    assert(d.visible.first == 0 && d.visible.last == 3 && d.scene.n == 4);
    assert(d.chrome[1].editor.text.data && d.chrome[2].heading && !d.chrome[4].heading);
    assert(d.chrome[1].editor.text.data != d.doc.v[1].source);
    assert(mulib_nb_layout_prefix(&d.layout, 1) == 68 &&
           mulib_nb_layout_prefix(&d.layout, 2) == 170);
    assert(pixel(&d, 18, 44) == UINT32_C(0xff34465e));  // Cell heading background.
    assert(has_color(&d, (mgx_rect){26, 44, 100, 26}, UINT32_C(0xfff0f3f8))); // RetGTK text.
    assert(pixel(&d, 28, 144) == UINT32_C(0xff394353)); // ImmGTK editor background.
    assert(has_color(&d, (mgx_rect){36, 144, 100, 55}, UINT32_C(0xfff0f3f8))); // Editor text.
    assert(pixel(&d, 33, 247) == UINT32_C(0xff173044)); // Opaque native header.
    assert(pixel(&d, 33, 280) == UINT32_C(0xff3c6d70)); // 50% alpha over card color.
    assert(pixel(&d, 33, 306) == UINT32_C(0xff151e2c)); // Beyond viewport.
    pick(&d, 20.5, 103.5, notes, 0, 0, 0);
    pick(&d, 20.5, 104, 0, 0, 0, 0);                  // Half-open card edge, gap.
    pick(&d, 31.5, 146.5, code, 0, 0, 0);
    pick(&d, 33.5, 280.5, app, c->node, 5.5, 34.5);
    pick(&d, 33.5, 306, 0, 0, 0, 0);                   // Half-open viewport edge.

    // Selecting app chrome and directly hitting the fake client are
    // different operations. Editor input is not delivered to a native hit.
    assert(mnb_demo_click(&d, 31.5, 217.5) && mnb_demo_render(&d));
    assert(d.selected_cell == app && !d.wm.focused && !d.immediate.focus);
    assert(pixel(&d, 18, 215) == UINT32_C(0xff56b4e9));
    assert(mnb_demo_click(&d, 33.5, 280.5) && mnb_demo_render(&d));
    assert(d.selected_cell == app && d.wm.focused == c->window && !d.immediate.focus);
    assert(!mnb_demo_text(&d, "not native input"));
    assert(mnb_demo_click(&d, 31.5, 146.5) && mnb_demo_render(&d));
    assert(d.selected_cell == code && !d.wm.focused && d.immediate.focus);
    assert(pixel(&d, 28, 144) == UINT32_C(0xff426c9f));

    uint64_t before = d.doc.revision;
    assert(!mnb_demo_text(&d, "\xc0\x80"));
    assert(mnb_demo_text(&d, " + 1") && mnb_demo_render(&d));
    assert(d.doc.revision == before + 1);
    assert(!strcmp(d.doc.v[1].source,
                  "// This is editable text, never evaluated.\nlet answer = 42 + 1"));
    assert(!d.wm.focused && d.doc.v[1].window == 0);

    // Scroll with a native client focused. An occlusion/clipping change
    // neither moves/resizes the fake window nor changes its commit serial.
    pick(&d, 33.5, 280.5, app, c->node, 5.5, 34.5);
    assert(mnb_demo_click(&d, 33.5, 280.5) && mnb_demo_render(&d));
    assert(d.selected_cell == app && d.wm.focused == c->window && !d.immediate.focus);
    before = d.doc.revision;
    uint64_t wm_revision = d.wm.revision;
    assert(mnb_demo_view(&d, 260, 1, MNB_DEMO_WIDTH, MNB_DEMO_HEIGHT));
    mnb_demo_hit stale;
    assert(!mnb_demo_pick(&d, 33.5, 280.5, &stale));
    assert(mnb_demo_render(&d));
    assert(d.visible.first == 2 && d.visible.last == 6 && d.viewport.scroll == 260);
    assert(!d.chrome[1].heading && !d.chrome[1].editor.text.data && d.chrome[4].heading);
    assert(d.selected_cell == app && d.wm.focused == c->window && !d.immediate.focus);
    pick(&d, 33.5, 48.5, app, c->node, 5.5, 62.5);
    pick(&d, 33.5, 43.5, 0, 0, 0, 0);                 // Clipped by viewport top.
    pick(&d, 33.5, 60.5, app, 0, 0, 0);               // Clipped inside the cell.
    pick(&d, 32.5, 200.5, c2->cell, c2->node, 4.5, 20.5);
    assert(pixel(&d, 33, 43) == UINT32_C(0xff151e2c));
    assert(pixel(&d, 33, 48) == UINT32_C(0xff3c6d70));
    assert(pixel(&d, 33, 60) == UINT32_C(0xff2b3440));
    client_unchanged(&d, c, native, storage, serial);

    // A narrower output clips (not fits) the client. Fractional scroll and
    // zoom produce fractional local coordinates even after a swapchain resize.
    assert(mnb_demo_view(&d, 250.25, 1.5, 200, 300) && mnb_demo_render(&d));
    assert(mnb_demo_frame(&d)->w == 200 && mnb_demo_frame(&d)->h == 300);
    assert(d.clip.w == 164 && d.clip.h == 242);
    pick(&d, 36.375, 60.5, app, c->node, 2.25, 59.25);
    pick(&d, 181.5, 60.5, app, c->node, 99, 59.25);
    pick(&d, 182, 60.5, 0, 0, 0, 0);
    assert(pixel(&d, 181, 60) == UINT32_C(0xff3c6d70));
    assert(pixel(&d, 182, 60) == UINT32_C(0xff151e2c));
    client_unchanged(&d, c, native, storage, serial);
    assert(d.doc.revision == before && d.wm.revision == wm_revision);

    assert(mnb_demo_view(&d, 1e6, 1, MNB_DEMO_WIDTH, MNB_DEMO_HEIGHT));
    assert(d.viewport.scroll == 290 && mnb_demo_render(&d) && d.visible.first == 3);
    client_unchanged(&d, c, native, storage, serial);
    assert(!d.chrome[2].heading && d.wm.focused == c->window);
    assert(mnb_demo_view(&d, -100, 1, MNB_DEMO_WIDTH, MNB_DEMO_HEIGHT));
    assert(d.viewport.scroll == 0 && mnb_demo_render(&d) && d.visible.first == 0);
    assert(d.chrome[1].editor.text.data &&
           !strcmp(d.chrome[1].editor.text.data, d.doc.v[1].source));
    assert(!d.immediate.focus);
    assert(d.doc.revision == before && d.wm.revision == wm_revision);

    // MNB1 reload restores only the document, not native/GUI identities,
    // scene nodes, selection, viewport, or the source's execution trust.
    d.doc.v[1].flags |= MULIB_NB_TRUSTED;
    assert(mulib_nb_store_save(saved, &d.doc));
    mnb_demo fresh = {0};
    assert(mnb_demo_init(&fresh, saved));
    assert(fresh.doc.id == d.doc.id && fresh.doc.n == d.doc.n && fresh.doc.next == d.doc.next);
    assert(!strcmp(fresh.doc.name, d.doc.name));
    for (size_t i = 0; i < fresh.doc.n; ++i) {
        const mulib_nb_cell *a = &d.doc.v[i], *b = &fresh.doc.v[i];
        assert(a->id == b->id && a->kind == b->kind && a->height == b->height);
        assert(a->parent == b->parent && !strcmp(a->language, b->language));
        assert(!strcmp(a->source, b->source) && !b->window && !(b->flags & MULIB_NB_TRUSTED));
    }
    assert(fresh.client_count == 0 && fresh.wm.n == 0 && fresh.scene.n == 2);
    assert(!fresh.selected_cell && !fresh.wm.focused && !fresh.immediate.focus);
    assert(mnb_demo_render(&fresh));
    pick(&fresh, 33.5, 280.5, app, 0, 0, 0);
    assert(pixel(&fresh, 33, 280) == UINT32_C(0xff2b3440));

    // Resizing a notebook cell is itself a document edit: all following
    // canvas placements move, but the fake client geometry remains unchanged.
    assert(mulib_nb_set_height(&d.doc, notes, 100));
    assert(!mnb_demo_pick(&d, 33.5, 280.5, &stale));
    assert(!mnb_demo_view(&d, NAN, 1, MNB_DEMO_WIDTH, 360));
    assert(!mnb_demo_pick(&d, 33.5, 280.5, &stale));
    assert(mnb_demo_view(&d, 0, 1, MNB_DEMO_WIDTH, 360) && mnb_demo_render(&d));
    assert(mulib_nb_layout_prefix(&d.layout, 2) == 210);
    pick(&d, 33.5, 320.5, app, c->node, 5.5, 34.5);
    client_unchanged(&d, c, native, storage, serial);
    assert(d.wm.revision == wm_revision);

    mnb_demo_free(&fresh);
    mnb_demo_free(&d);
}

static void test_source_is_inert(const char *saved, const char *marker)
{
    mulib_nb doc = {0};
    char src[4096];
    int n = snprintf(src, sizeof src, "touch -- \"%s\"", marker);
    assert(n > 0 && (size_t)n < sizeof src);
    assert(mulib_nb_init(&doc, 7, "Text-only import"));
    uint64_t id = mulib_nb_insert(&doc, 0, MULIB_NB_CODE, src, "sh", 120);
    assert(id && mulib_nb_store_save(saved, &doc));
    mulib_nb_free(&doc);

    mnb_demo d = {0};
    assert(mnb_demo_init(&d, saved) && mnb_demo_render(&d));
    assert(d.doc.v[0].id == id && !strcmp(d.doc.v[0].source, src));
    assert(d.chrome[0].editor.text.data && d.client_count == 0);
    assert(access(marker, F_OK) < 0 && errno == ENOENT);
    mnb_demo_free(&d);
}

static void test_scene_view_update(void)
{
    mulib_glx_scene scene = {0};
    uint32_t pixel_argb = UINT32_C(0xff112233);
    mulib_glx_clientbuffer b = {0}, ignored = {0};
    assert(mulib_glx_clientbuffer_init(&b, &pixel_argb, 1, 1, 1, 1, NULL, NULL));
    mulib_glx_scenenode view = mulib_glx_scenenode_default();
    uint64_t parent = mulib_glx_scene_add(&scene, 0, &view);
    assert(parent);
    view.buffer = &b;
    uint64_t id = mulib_glx_scene_add(&scene, parent, &view);
    assert(id && b.refs == 2);
    uint64_t rev = scene.revision;
    view.x = NAN;
    assert(!mulib_glx_scene_set_view(&scene, id, &view) && scene.revision == rev);
    view.x = 4;
    view.id = view.parent = UINT64_MAX;
    view.buffer = &ignored;
    assert(mulib_glx_scene_set_view(&scene, id, &view) && scene.revision == rev + 1);
    const mulib_glx_scenenode *n = mulib_glx_scene_find(&scene, id);
    assert(n && n->id == id && n->parent == parent && n->buffer == &b && n->x == 4);
    assert(b.refs == 2);
    mulib_glx_scene_free(&scene);
    assert(b.refs == 1);
    mulib_glx_clientbuffer_unref(&b);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    test_scene_view_update();
    test_pipeline(argv[1]);
    test_source_is_inert(argv[1], argv[2]);
    puts("PASS: notebook M0 viewport, shared surface, native hits, focus, edits and persistence");
    return 0;
}
