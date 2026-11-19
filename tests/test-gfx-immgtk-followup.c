/* Production native-window path; event batches, real pixels, allocation faults.
 * Dependency implementations are included before the scoped fault injector. */
#include "mulib-gui-core.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>

static size_t fail_after = SIZE_MAX;
static void *window_realloc(void *p, size_t n) {
    if (!fail_after) return NULL;
    if (fail_after != SIZE_MAX) --fail_after;
    return realloc(p, n);
}
#define realloc window_realloc
#include "mulib-immgtk-window.h"
#include "mulib-immgtk-popup.h"
#undef realloc

static mgx_event motion(double x, double y) {
    mgx_event e = {0};
    e.type = MGX_EVENT_MOTION;
    e.x = x;
    e.y = y;
    return e;
}
static mgx_event button(int down) {
    mgx_event e = {0};
    e.type = MGX_EVENT_BUTTON;
    e.down = down;
    return e;
}
static mgx_event key(unsigned code) {
    mgx_event e = {0};
    e.type = MGX_EVENT_KEY;
    e.code = code;
    e.down = 1;
    return e;
}
static int same(mgx_rect a, mgx_rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
static const mig_item *item(const mulib_immgtk_native_t *g, const char *name) {
    mulib_immgtk_native_t ids = {0};
    assert(mig_push_id(&ids, "window"));
    return mig_previous(g, mig_id(&ids, name));
}
static void window_frame(mulib_immgtk_native_t *g, mgx_surface *surface,
        const mgx_event *events, size_t count, mgx_rect *bounds, unsigned flags,
        int *collapsed) {
    mulib_immgtk_window_scope_t scope;
    mgx_rect clip = surface->clip;
    assert(mulib_immgtk_native_begin_events(g, surface, events, count));
    assert(mulib_immgtk_window_begin_ex(g, "window", bounds, flags, collapsed,
        60, 60, &scope));
    /* Oversized child drawing/hits must be constrained by the content clip. */
    (void)mig_button(g, "child", NULL, (mgx_rect){0, 0, 256, 192}, 1);
    mulib_immgtk_window_end(g, &scope);
    assert(g->depth == 0 && same(g->clip, clip));
    mulib_immgtk_native_end(g);
    assert(!g->error && same(surface->clip, clip));
}
static void test_drag(mgx_surface *surface) {
    mulib_immgtk_native_t g;
    mulib_immgtk_native_init(&g);
    mgx_rect bounds = {20, 20, 100, 100};
    window_frame(&g, surface, NULL, 0, &bounds, 0, NULL);
    mgx_event complete[] = {motion(30, 30), button(1), motion(70, 55), button(0),
        motion(240, 180)};
    window_frame(&g, surface, complete, 5, &bounds, 0, NULL);
    assert(same(bounds, (mgx_rect){60, 45, 100, 100})); /* Release, not final motion. */
    const mig_item *title = item(&g, "#title");
    assert(title && title->bounds.x == 60 && title->bounds.y == 45);
    mgx_event press[] = {motion(70, 55), button(1), motion(100, 85)};
    window_frame(&g, surface, press, 3, &bounds, 0, NULL);
    assert(same(bounds, (mgx_rect){90, 75, 100, 100}) && g.active);
    /* Original press is now outside current title: capture must not be lost. */
    mgx_event held = motion(120, 95);
    window_frame(&g, surface, &held, 1, &bounds, 0, NULL);
    assert(same(bounds, (mgx_rect){110, 85, 100, 100}) && g.active);
    mgx_event release[] = {motion(130, 100), button(0), motion(0, 0)};
    window_frame(&g, surface, release, 3, &bounds, 0, NULL);
    assert(same(bounds, (mgx_rect){120, 90, 100, 100}) && !g.active);
    /* Two completed gestures on one handle preserve their individual origins. */
    mgx_event twice[] = {motion(130, 100), button(1), motion(135, 100), button(0),
        motion(140, 100), button(1), motion(145, 100), button(0)};
    window_frame(&g, surface, twice, 8, &bounds, 0, NULL);
    assert(bounds.x == 130 && bounds.y == 90);
    /* Switching a live title to no-move cancels rather than resurrects capture. */
    mgx_event start[] = {motion(140, 100), button(1), motion(145, 100)};
    window_frame(&g, surface, start, 3, &bounds, MIG_WINDOW_NO_MOVE, NULL);
    assert(bounds.x == 130 && !g.active);
    window_frame(&g, surface, NULL, 0, &bounds, 0, NULL);
    assert(bounds.x == 130);
    mulib_immgtk_native_free(&g);
}
static void test_resize_and_clip(mgx_surface *surface) {
    mulib_immgtk_native_t g;
    mulib_immgtk_native_init(&g);
    mgx_rect bounds = {20, 20, 100, 100};
    unsigned flags = MIG_WINDOW_RESIZABLE | MIG_WINDOW_NO_MOVE;
    window_frame(&g, surface, NULL, 0, &bounds, flags, NULL);
    mgx_event resize[] = {motion(114, 114), button(1), motion(154, 144), button(0),
        motion(250, 180)};
    mgx_clear(surface, 0xff010203);
    window_frame(&g, surface, resize, 5, &bounds, flags, NULL);
    assert(same(bounds, (mgx_rect){20, 20, 140, 130}));
    assert(surface->pixels[149 * surface->stride + 159] == g.theme.accent);
    assert(surface->pixels[150 * surface->stride + 159] == 0xff010203);
    assert(surface->pixels[55 * surface->stride + 21] == g.theme.panel);
    assert(surface->pixels[55 * surface->stride + 30] == g.theme.normal);
    const mig_item *title = item(&g, "#title"), *handle = item(&g, "#resize");
    assert(title && title->bounds.w == 140 && handle && handle->z == title->z);
    /* A later submitted child geometrically covers the grip but cannot steal it. */
    assert(mig_hit(&g, 154, 144) == handle->id);
    mgx_event smaller[] = {motion(154, 144), button(1), motion(-1000, -1000), button(0)};
    window_frame(&g, surface, smaller, 4, &bounds, flags, NULL);
    assert(same(bounds, (mgx_rect){20, 20, 60, 60}));
    /* Surface clips constrain both grip drawing and its old hit rectangle. */
    surface->clip = (mgx_rect){0, 0, 50, 50};
    mgx_rect before = bounds;
    mgx_event clipped[] = {motion(74, 74), button(1), motion(100, 100), button(0)};
    window_frame(&g, surface, clipped, 4, &bounds, flags, NULL);
    assert(same(bounds, before) && !g.active);
    surface->clip = (mgx_rect){0, 0, surface->w, surface->h};
    mulib_immgtk_native_free(&g);
}
static void test_collapse_and_modal(mgx_surface *surface) {
    mulib_immgtk_native_t g;
    mulib_immgtk_native_init(&g);
    mgx_rect bounds = {20, 20, 100, 100};
    unsigned flags = MIG_WINDOW_COLLAPSIBLE | MIG_WINDOW_RESIZABLE;
    int collapsed = 0;
    window_frame(&g, surface, NULL, 0, &bounds, flags, &collapsed);
    mgx_event fold[] = {motion(30, 30), button(1), button(0)};
    mgx_clear(surface, 0xff010203);
    window_frame(&g, surface, fold, 3, &bounds, flags, &collapsed);
    assert(collapsed && bounds.h == 100 && !item(&g, "#resize"));
    assert(surface->pixels[60 * surface->stride + 30] == 0xff010203);
    const mig_item *background = item(&g, "#background");
    assert(background && background->bounds.h == 24);
    /* Collapse is a real keyboard control, and content is not navigable folded. */
    const mig_item *collapse = item(&g, "#collapse");
    assert(collapse && g.focus == collapse->id);
    uint64_t title_id = item(&g, "#title")->id;
    assert(mig_nav_step(&g, 0) == title_id);
    assert(mig_nav_step(&g, 0) == collapse->id);
    mgx_event enter = key(28);
    window_frame(&g, surface, &enter, 1, &bounds, flags, &collapsed);
    assert(!collapsed && item(&g, "#resize"));
    mulib_immgtk_popup_t popup = {0};
    assert(mulib_immgtk_popup_open(&popup, 123));
    mulib_immgtk_popup_prepare(&g, &popup);
    mgx_rect before = bounds;
    mgx_event blocked[] = {motion(114, 114), button(1), motion(150, 150), button(0)};
    window_frame(&g, surface, blocked, 4, &bounds, flags, &collapsed);
    assert(same(bounds, before) && !g.active && !g.focus);
    mulib_immgtk_popup_close(&popup);
    mulib_immgtk_popup_prepare(&g, &popup);
    mulib_immgtk_native_free(&g);
}
static void test_order_and_mutation(mgx_surface *surface) {
    mulib_immgtk_native_t g;
    mulib_immgtk_window_scope_t scope;
    mulib_immgtk_native_init(&g);
    mgx_rect bounds = {20, 20, 100, 100}, overlay = {80, 80, 80, 80};
    for (int frame = 0; frame < 2; ++frame) {
        mgx_event drag[] = {motion(114, 114), button(1), motion(150, 150), button(0)};
        assert(mulib_immgtk_native_begin_events(&g, surface, frame ? drag : NULL,
            frame ? 4 : 0));
        assert(mulib_immgtk_window_begin_ex(&g, "window", &bounds, MIG_WINDOW_RESIZABLE,
            NULL, 60, 60, &scope));
        mulib_immgtk_window_end(&g, &scope);
        assert(mulib_immgtk_window_begin(&g, "overlay", &overlay, &scope));
        mulib_immgtk_window_end(&g, &scope); /* Empty later window hides old grip. */
        assert(bounds.w == 100 && bounds.h == 100);
        mulib_immgtk_native_end(&g);
        assert(!g.error && !g.active);
    }
    mulib_immgtk_native_invalidate(&g);
    window_frame(&g, surface, NULL, 0, &bounds, 0, NULL);
    mgx_event stale[] = {motion(30, 30), button(1), motion(40, 40)};
    assert(mulib_immgtk_native_begin_events(&g, surface, stale, 3));
    bounds.x = bounds.y = 100; /* Host moved current geometry after dispatch. */
    assert(mulib_immgtk_window_begin(&g, "window", &bounds, &scope));
    mulib_immgtk_window_end(&g, &scope);
    assert(bounds.x == 100 && bounds.y == 100 && !g.active);
    mulib_immgtk_native_end(&g);
    mgx_event up = button(0);
    window_frame(&g, surface, &up, 1, &bounds, 0, NULL);
    mgx_event escape[] = {motion(110, 110), button(1), motion(130, 130), key(1)};
    window_frame(&g, surface, escape, 4, &bounds, 0, NULL);
    assert(bounds.x == 100 && bounds.y == 100 && !g.active);
    /* Focus loss also drops any pending drag update, not just click actions. */
    window_frame(&g, surface, &up, 1, &bounds, 0, NULL);
    mgx_event focus[] = {motion(110, 110), button(1), motion(130, 130), {0}};
    focus[3].type = MGX_EVENT_FOCUS;
    window_frame(&g, surface, focus, 4, &bounds, 0, NULL);
    assert(bounds.x == 100 && bounds.y == 100 && !g.active && !g.down);
    mulib_immgtk_native_free(&g);
}
static void test_snapshots_and_extremes(mgx_surface *surface) {
    mulib_immgtk_native_t g;
    mulib_immgtk_window_scope_t scope;
    mulib_immgtk_native_init(&g);
    mgx_rect bounds = {20, 20, 100, 100};
    for (int frame = 0; frame < 3; ++frame) {
        mulib_immgtk_native_begin(&g, surface, frame == 2 ? 70 : 30,
            frame == 2 ? 50 : 30, frame == 1, 0);
        assert(mulib_immgtk_window_begin(&g, "window", &bounds, &scope));
        mulib_immgtk_window_end(&g, &scope);
        mulib_immgtk_native_end(&g);
        assert(!g.error);
    }
    assert(same(bounds, (mgx_rect){60, 40, 100, 100}));
    mulib_immgtk_native_invalidate(&g);
    bounds = (mgx_rect){20, 20, 100, 100};
    window_frame(&g, surface, NULL, 0, &bounds, MIG_WINDOW_RESIZABLE, NULL);
    mgx_event huge[] = {motion(114, 114), button(1), motion(DBL_MAX, DBL_MAX), button(0)};
    window_frame(&g, surface, huge, 4, &bounds, MIG_WINDOW_RESIZABLE, NULL);
    assert(bounds.x == 20 && bounds.y == 20 && bounds.w == INT_MAX - 20 &&
        bounds.h == INT_MAX - 20);
    mulib_immgtk_native_invalidate(&g);
    bounds = (mgx_rect){20, 20, 100, 100};
    window_frame(&g, surface, NULL, 0, &bounds, 0, NULL);
    mgx_event negative[] = {motion(30, 30), button(1), motion(-DBL_MAX, -DBL_MAX),
        button(0)};
    window_frame(&g, surface, negative, 4, &bounds, 0, NULL);
    assert(bounds.x == INT_MIN && bounds.y == INT_MIN && bounds.w == 100 && bounds.h == 100);
    mulib_immgtk_native_free(&g);
}
static void test_failure_before_effects(mgx_surface *surface) {
    for (size_t allocation = 0; allocation < 3; ++allocation) {
        mulib_immgtk_native_t g;
        mulib_immgtk_window_scope_t scope;
        mulib_immgtk_native_init(&g);
        assert(mulib_immgtk_native_begin_events(&g, surface, NULL, 0));
        mgx_rect bounds = {20, 20, 100, 100}, before = bounds, clip = g.clip;
        int collapsed = 0;
        fail_after = allocation;
        assert(!mulib_immgtk_window_begin_ex(&g, "window", &bounds,
            MIG_WINDOW_RESIZABLE | MIG_WINDOW_COLLAPSIBLE, &collapsed, 60, 60, &scope));
        fail_after = SIZE_MAX;
        assert((g.error & MIG_ERROR_MEMORY) && !scope.opened && !g.n && !g.sn &&
            !g.draws.n && !g.depth && same(g.clip, clip) && same(bounds, before) &&
            !collapsed);
        mulib_immgtk_native_end(&g);
        mulib_immgtk_native_free(&g);
    }
    mulib_immgtk_native_t g;
    mulib_immgtk_window_scope_t scope;
    mulib_immgtk_native_init(&g);
    mgx_rect bounds = {20, 20, 100, 100}, before = bounds;
    window_frame(&g, surface, NULL, 0, &bounds, MIG_WINDOW_RESIZABLE, NULL);
    /* A malformed event after a valid drag press cannot resize the window. */
    mgx_event bad[] = {motion(114, 114), button(1), motion(NAN, 100)};
    assert(!mulib_immgtk_native_begin_events(&g, surface, bad, 3));
    assert(!g.an && !g.active && !g.down);
    assert(!mulib_immgtk_window_begin_ex(&g, "window", &bounds, MIG_WINDOW_RESIZABLE,
        NULL, 60, 60, &scope) && same(bounds, before) && !scope.opened && !g.depth);
    mulib_immgtk_native_end(&g);
    assert(mulib_immgtk_native_begin_events(&g, surface, NULL, 0));
    size_t states = g.sn;
    assert(!mulib_immgtk_window_begin_ex(&g, "window", &bounds, 128,
        NULL, 60, 60, &scope));
    assert(!g.n && g.sn == states && !g.draws.n && !g.depth && same(bounds, before));
    assert(mulib_immgtk_window_begin(&g, "window", &bounds, &scope));
    mulib_immgtk_window_end(&g, &scope);
    size_t items = g.n, draws = g.draws.n;
    assert(!mulib_immgtk_window_begin(&g, "window", &bounds, &scope));
    assert((g.error & MIG_ERROR_ID) && g.n == items && g.draws.n == draws && !g.depth);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_free(&g);
    /* A gesture-queue allocation failure cannot apply a partial movement. */
    mulib_immgtk_native_init(&g);
    bounds = (mgx_rect){20, 20, 100, 100};
    window_frame(&g, surface, NULL, 0, &bounds, 0, NULL);
    mgx_event drag[] = {motion(30, 30), button(1), motion(60, 60), button(0)};
    fail_after = 0;
    assert(!mulib_immgtk_native_begin_events(&g, surface, drag, 4));
    fail_after = SIZE_MAX;
    assert((g.error & MIG_ERROR_MEMORY) && !g.an && !g.active && !g.down);
    assert(!mulib_immgtk_window_begin(&g, "window", &bounds, &scope));
    assert(bounds.x == 20 && bounds.y == 20);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_free(&g);
}
int main(void) {
    mgx_surface surface = {0};
    assert(mgx_surface_init(&surface, 256, 192));
    test_drag(&surface);
    test_resize_and_clip(&surface);
    test_collapse_and_modal(&surface);
    test_order_and_mutation(&surface);
    test_snapshots_and_extremes(&surface);
    test_failure_before_effects(&surface);
    mgx_surface_free(&surface);
    puts("PASS ImmGTK windows: captured drag batches, resize, collapse, clips, faults");
    return 0;
}
