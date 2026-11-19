/* Native headless behavior; no DRM/input devices or external GUI runtimes. */
#include "mulib-immgtk-extensions.h"
#define MULIB_IMPL_IMMGTK_DRAWLIST
#include "mulib-immgtk-drawlist-bridge.h"
#include <assert.h>
#include <stdio.h>

static const mgx_rect box = {4, 4, 24, 20};
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
static mgx_event key(unsigned code, unsigned mods) {
    mgx_event e = {0};
    e.type = MGX_EVENT_KEY;
    e.code = code;
    e.down = 1;
    e.mods = mods;
    return e;
}
static void register_button(mulib_immgtk_native_t *g, mgx_surface *s) {
    mulib_immgtk_native_begin(g, s, 10, 10, 0, 0);
    assert(!mig_button(g, "a", NULL, box, 1));
    mulib_immgtk_native_end(g);
    assert(!g->error);
}
static void test_snapshot(mgx_surface *s) {
    mulib_immgtk_native_t g;
    mulib_immgtk_native_init(&g);
    mulib_immgtk_native_begin(&g, s, 10, 10, 1, 0);
    assert(!mig_button(&g, "a", NULL, box, 1)); /* First frame is registration only. */
    assert(!g.active && !g.focus);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, s, 10, 10, 0, 0);
    assert(!mig_button(&g, "a", NULL, box, 1)); /* No first-frame capture. */
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, s, 10, 10, 1, 0);
    assert(!mig_button(&g, "a", NULL, box, 1));
    assert(g.active == mig_id(&g, "a") && g.pressed);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, s, 10, 10, 0, 0);
    assert(mig_button(&g, "a", NULL, box, 1) && g.released);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, s, 60, 60, 0, 1);
    assert(mig_button(&g, "a", NULL, box, 1)); /* Focused keyboard pulse. */
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_free(&g);
    mulib_immgtk_native_free(&g);
}
static void test_ordered(mgx_surface *s) {
    mulib_immgtk_native_t g;
    mulib_immgtk_io_t io = {0};
    mulib_immgtk_native_init(&g);
    register_button(&g, s);
    mgx_event events[] = {motion(10, 10), button(1), button(0), motion(60, 60)};
    for (size_t i = 0; i < sizeof events / sizeof *events; ++i)
        assert(mulib_immgtk_io_feed(&io, events + i));
    mulib_immgtk_io_begin(&g, s, &io);
    assert(g.pressed && g.released && !g.down && !g.hot);
    assert(mig_button(&g, "a", NULL, box, 1)); /* Uses release coords, not final motion. */
    assert(!mig_button(&g, "a", NULL, box, 1) && (g.error & MIG_ERROR_ID));
    mulib_immgtk_native_end(&g);
    mulib_immgtk_io_reset(&io);
    mgx_event outside[] = {motion(10, 10), button(1), motion(40, 40), button(0)};
    assert(mulib_immgtk_native_begin_events(&g, s, outside, 4));
    assert(!mig_button(&g, "a", NULL, box, 1));
    mulib_immgtk_native_end(&g);
    /* Press and release at different controls must not transfer capture. */
    mulib_immgtk_native_begin(&g, s, 0, 0, 0, 0);
    assert(!mig_button(&g, "a", NULL, box, 1));
    assert(!mig_button(&g, "b", NULL, (mgx_rect){40, 4, 24, 20}, 1));
    mulib_immgtk_native_end(&g);
    mgx_event two[] = {motion(10, 10), button(1), button(0),
        motion(45, 10), button(1), button(0)};
    assert(mulib_immgtk_native_begin_events(&g, s, two, 6));
    assert(mig_button(&g, "a", NULL, box, 1));
    assert(mig_button(&g, "b", NULL, (mgx_rect){40, 4, 24, 20}, 1));
    mulib_immgtk_native_end(&g);
    mulib_immgtk_io_reset(&io);
    io.mouse.x = io.mouse.y = 10;
    io.mouse.pressed = io.mouse.released = 1;
    io.mouse.buttons = 0; /* Legacy snapshots still keep both edges. */
    mulib_immgtk_io_begin(&g, s, &io);
    assert(mig_button(&g, "a", NULL, box, 1));
    mulib_immgtk_native_end(&g);
    mulib_immgtk_io_free(&io);
    mulib_immgtk_native_free(&g);
}
static void test_clips_and_stale(mgx_surface *s) {
    mulib_immgtk_native_t g;
    mulib_immgtk_native_init(&g);
    mulib_immgtk_native_begin(&g, s, 0, 0, 0, 0);
    assert(mulib_immgtk_clip_push(&g, (mgx_rect){4, 4, 4, 4}));
    assert(!mig_button(&g, "a", NULL, box, 1));
    assert(mulib_immgtk_clip_pop(&g));
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, s, 10, 10, 1, 0);
    assert(!g.hot && !g.active);
    assert(!mig_button(&g, "a", NULL, box, 1));
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, s, 10, 10, 0, 0);
    assert(!mig_button(&g, "a", NULL, box, 1));
    mulib_immgtk_native_end(&g);
    mgx_event click[] = {motion(10, 10), button(1), button(0)};
    assert(mulib_immgtk_native_begin_events(&g, s, click, 3));
    assert(!mig_button(&g, "a", NULL, (mgx_rect){40, 40, 24, 20}, 1));
    mulib_immgtk_native_end(&g); /* Moved current geometry cannot accept stale click. */
    register_button(&g, s);
    assert(mulib_immgtk_native_begin_events(&g, s, click, 3));
    assert(mulib_immgtk_clip_push(&g, (mgx_rect){0, 0, 8, 8}));
    assert(!mig_button(&g, "a", NULL, box, 1));
    assert(mulib_immgtk_clip_pop(&g));
    mulib_immgtk_native_end(&g);
    register_button(&g, s);
    s->clip = (mgx_rect){0, 0, 8, 8};
    assert(mulib_immgtk_native_begin_events(&g, s, click, 3));
    assert(!g.hot && !mig_button(&g, "a", NULL, box, 1));
    mulib_immgtk_native_end(&g);
    s->clip = (mgx_rect){0, 0, s->w, s->h};
    mulib_immgtk_native_invalidate(&g);
    assert(!g.pn && !g.focus && !g.active);
    mulib_immgtk_native_free(&g);
}
static void test_slider(mgx_surface *s) {
    mulib_immgtk_native_t g;
    double value = 0;
    mulib_immgtk_native_init(&g);
    mulib_immgtk_native_begin(&g, s, 0, 0, 0, 0);
    assert(!mig_slider(&g, "slider", box, &value, 0, 1));
    mulib_immgtk_native_end(&g);
    mgx_event click[] = {motion(10, 10), button(1), motion(22, 10), button(0),
        motion(60, 60)};
    assert(mulib_immgtk_native_begin_events(&g, s, click, 5));
    assert(mig_slider(&g, "slider", box, &value, 0, 1));
    assert(value == .75 && g.last_action.x == 22);
    mulib_immgtk_native_end(&g);
    double before = value;
    assert(!mig_slider(&g, "slider", box, &value, 0, 1) && value == before);
    mulib_immgtk_native_free(&g);
}
static void test_nav_and_focus(mgx_surface *s) {
    mulib_immgtk_native_t g;
    mulib_immgtk_native_init(&g);
    mulib_immgtk_native_begin(&g, s, 0, 0, 0, 0);
    (void)mig_button(&g, "a", NULL, box, 1);
    (void)mig_button(&g, "disabled", NULL, box, 0);
    assert(mulib_immgtk_clip_push(&g, (mgx_rect){0, 0, 0, 0}));
    (void)mig_button(&g, "clipped", NULL, box, 1);
    assert(mulib_immgtk_clip_pop(&g));
    (void)mig_button(&g, "b", NULL, (mgx_rect){40, 4, 24, 20}, 1);
    mulib_immgtk_native_end(&g);
    assert(mulib_immgtk_nav_step(&g, 0) == mig_id(&g, "a"));
    assert(mulib_immgtk_nav_step(&g, 0) == mig_id(&g, "b"));
    assert(mulib_immgtk_nav_step(&g, 1) == mig_id(&g, "a"));
    mgx_event keys[] = {key(15, 0), key(28, 0)};
    assert(mulib_immgtk_native_begin_events(&g, s, keys, 2));
    assert(!mig_button(&g, "a", NULL, box, 1));
    assert(mig_button(&g, "b", NULL, box, 1));
    mulib_immgtk_native_end(&g);
    mgx_event enter = key(28, 0);
    assert(mulib_immgtk_native_begin_events(&g, s, &enter, 1));
    assert(!mig_button(&g, "b", NULL, box, 0) && !g.focus);
    mulib_immgtk_native_end(&g);
    mgx_event lost = {0};
    lost.type = MGX_EVENT_FOCUS;
    assert(mulib_immgtk_native_begin_events(&g, s, &lost, 1));
    assert(!g.focus && !g.active);
    (void)mig_button(&g, "a", NULL, box, 1);
    mulib_immgtk_native_end(&g);
    mgx_event click[] = {motion(10, 10), button(1), button(0)};
    assert(mulib_immgtk_native_begin_events(&g, s, click, 3));
    assert(!mig_button(&g, "a", NULL, box, 1) && !g.focus);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_free(&g);
}
static void popup_widgets(mulib_immgtk_native_t *g, mulib_immgtk_popup_t *p,
        int *background, int *popup) {
    mulib_immgtk_popup_scope_t scope;
    *background = mig_button(g, "back", NULL, box, 1);
    *popup = 0;
    if (mulib_immgtk_popup_begin(g, p, 123, (mgx_rect){0, 0, 40, 40}, &scope)) {
        *popup = mig_button(g, "front", NULL, box, 1);
        mulib_immgtk_popup_end(g, &scope);
    }
}
static void test_popup(mgx_surface *s) {
    mulib_immgtk_native_t g;
    mulib_immgtk_popup_t p = {0};
    int back, front;
    mulib_immgtk_native_init(&g);
    assert(mulib_immgtk_popup_open(&p, 123));
    mulib_immgtk_popup_prepare(&g, &p); /* Must precede dispatch. */
    mulib_immgtk_native_begin(&g, s, 0, 0, 0, 0);
    popup_widgets(&g, &p, &back, &front);
    mulib_immgtk_native_end(&g);
    assert(mulib_immgtk_nav_step(&g, 0) == mig_id(&g, "front"));
    mgx_event click[] = {motion(10, 10), button(1), button(0)};
    assert(mulib_immgtk_native_begin_events(&g, s, click, 3));
    popup_widgets(&g, &p, &back, &front);
    assert(!back && front);
    mulib_immgtk_native_end(&g);
    assert(mulib_immgtk_popup_open(&p, 456));
    mulib_immgtk_popup_prepare(&g, &p);
    mulib_immgtk_native_begin(&g, s, 0, 0, 0, 0);
    popup_widgets(&g, &p, &back, &front);
    mulib_immgtk_popup_scope_t nested;
    assert(mulib_immgtk_popup_begin(&g, &p, 456, (mgx_rect){0, 0, 40, 40}, &nested));
    assert(!mig_button(&g, "nested", NULL, box, 1));
    mulib_immgtk_popup_end(&g, &nested);
    mulib_immgtk_native_end(&g);
    assert(mulib_immgtk_nav_step(&g, 0) == mig_id(&g, "nested"));
    mgx_event enter = key(28, 0);
    assert(mulib_immgtk_native_begin_events(&g, s, &enter, 1));
    popup_widgets(&g, &p, &back, &front);
    assert(!back && !front);
    assert(mulib_immgtk_popup_begin(&g, &p, 456, (mgx_rect){0, 0, 40, 40}, &nested));
    assert(mig_button(&g, "nested", NULL, box, 1));
    mulib_immgtk_popup_end(&g, &nested);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_popup_close(&p);
    mulib_immgtk_popup_prepare(&g, &p);
    mgx_event outside[] = {motion(60, 60), button(1), button(0)};
    assert(mulib_immgtk_native_begin_events(&g, s, outside, 3));
    assert(!g.focus && !g.active && !g.hot);
    assert(mulib_immgtk_popup_dismiss(&g, &p, (mgx_rect){0, 0, 40, 40}, 1, 1));
    popup_widgets(&g, &p, &back, &front);
    assert(!back && !front && !g.blocker);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_free(&g);
}
static void test_window_occlusion(mgx_surface *s) {
    mulib_immgtk_native_t g;
    mulib_immgtk_native_init(&g);
    mgx_rect a = {0, 0, 90, 90}, b = {0, 0, 90, 90};
    mulib_immgtk_window_scope_t w;
    for (int frame = 0; frame < 2; ++frame) {
        mgx_event click[] = {motion(15, 40), button(1), button(0)};
        assert(mulib_immgtk_native_begin_events(&g, s, frame ? click : NULL, frame ? 3 : 0));
        assert(mulib_immgtk_window_begin(&g, "first", &a, &w));
        assert(!mig_button(&g, "under", NULL, (mgx_rect){8, 32, 40, 24}, 1));
        mulib_immgtk_window_end(&g, &w);
        assert(mulib_immgtk_window_begin(&g, "second", &b, &w));
        mulib_immgtk_window_end(&g, &w); /* Empty content must occlude first window. */
        if (frame) assert(!g.focus && !g.active);
        mulib_immgtk_native_end(&g);
        assert(!g.error);
    }
    mulib_immgtk_native_free(&g);
}
static void test_limits_and_ids(mgx_surface *s) {
    mulib_immgtk_native_t g;
    mulib_immgtk_native_init(&g);
    assert(mulib_immgtk_native_limits(&g, 2, 2, 2, 2));
    assert(mig_push_id(&g, "ab"));
    uint64_t a = mig_id(&g, "c");
    assert(mig_pop_id(&g) && mig_push_id(&g, "a"));
    assert(a != mig_id(&g, "bc"));
    assert(mig_pop_id(&g));
    for (int i = 0; i < 64; ++i) assert(mig_push_id(&g, "x"));
    assert(!mig_push_id(&g, "overflow") && g.depth == 64);
    for (int i = 0; i < 64; ++i) assert(mig_pop_id(&g));
    mulib_immgtk_native_begin(&g, s, 0, 0, 0, 0);
    uint64_t frame = g.frame;
    mulib_immgtk_native_begin(&g, s, 0, 0, 0, 0);
    assert(g.frame == frame && (g.error & MIG_ERROR_LIFETIME));
    (void)mig_interact(&g, 1, box, 1);
    (void)mig_interact(&g, 2, box, 1);
    assert(!mig_interact(&g, 3, box, 1) && g.n == 2 && g.cap == 2);
    assert(mig_state_get(&g, 1) && mig_state_get(&g, 2));
    assert(!mig_state_get(&g, 3) && g.sn == 2 && g.sc == 2);
    mig_rect(&g, box, 0);
    mig_rect(&g, box, 0);
    mig_rect(&g, box, 0);
    assert(g.draws.n == 2 && g.draws.cap == 2 && (g.error & MIG_ERROR_LIMIT));
    g.state_ttl = 1;
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, s, 0, 0, 0, 0);
    mulib_immgtk_native_end(&g);
    assert(g.sn == 0);
    mgx_event too_many[3] = {{0}, {0}, {0}};
    assert(!mulib_immgtk_native_begin_events(&g, s, too_many, 3));
    assert(!g.active && !g.focus && !g.down);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_free(&g);
    mulib_immgtk_io_t io = {0};
    io.max_events = 1;
    mgx_event e = button(1);
    assert(mulib_immgtk_io_feed(&io, &e));
    e.down = 0;
    assert(!mulib_immgtk_io_feed(&io, &e) && io.error);
    mulib_immgtk_native_init(&g);
    mulib_immgtk_io_begin(&g, s, &io);
    assert(g.error && !g.down);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_io_reset(&io);
    assert(!io.error && !io.mouse.buttons && !io.n);
    mgx_event text = {0};
    text.type = MGX_EVENT_TEXT;
    text.text[0] = (char)0xff;
    assert(!mulib_immgtk_io_feed(&io, &text) && !io.text.n);
    mulib_immgtk_io_reset(&io);
    io.max_text = 2;
    memcpy(text.text, "abc", 4);
    assert(!mulib_immgtk_io_feed(&io, &text) && !io.text.n);
    mulib_immgtk_io_reset(&io);
    mgx_event focus = {0};
    focus.type = MGX_EVENT_FOCUS;
    assert(mulib_immgtk_io_feed(&io, &focus));
    mulib_immgtk_io_begin(&g, s, &io);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_io_reset(&io);
    assert(io.unfocused);
    mulib_immgtk_io_begin(&g, s, &io);
    assert(g.unfocused && !g.focus && !g.active);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_io_free(&io);
    mulib_immgtk_native_free(&g);
}
static void test_adapter(mgx_surface *s) {
    mulib_immgtk_drawlist_t old = {0};
    mulib_glx_drawlist_t shared = {0};
    mgx_clear(s, 0xff000000);
    assert(mulib_immgtk_drawlist_push(&old, (mulib_immgtk_drawcmd_t){
        MULIB_IMMGTK_RECT, 0, 0, 20, 20, 0, 0xffff0000}));
    assert(mulib_immgtk_drawlist_push(&old, (mulib_immgtk_drawcmd_t){
        MULIB_IMMGTK_CIRCLE, 10, 10, 0, 0, 3, 0xff00ff00}));
    assert(mulib_immgtk_drawlist_push(&old, (mulib_immgtk_drawcmd_t){
        MULIB_IMMGTK_LINE, 0, 10, 20, 10, 2, 0xff0000ff}));
    assert(mulib_immgtk_drawlist_append(&old, &shared, (mgx_rect){5, 5, 10, 10}));
    assert(shared.n == 3 && shared.v[0].kind == MGX_RECT &&
        shared.v[1].kind == MGX_CIRCLE && shared.v[2].kind == MGX_LINE);
    mulib_glx_drawlist_render(&shared, s);
    assert(s->pixels[0] == 0xff000000);
    assert(s->pixels[6 * s->stride + 6] == 0xffff0000);
    assert(s->pixels[8 * s->stride + 10] == 0xff00ff00);
    assert(s->pixels[10 * s->stride + 10] == 0xff0000ff);
    old.cmds[0].x2 = NAN;
    assert(!mulib_immgtk_drawlist_append(&old, &shared, s->clip) && shared.n == 3);
    mulib_immgtk_drawlist_free(&old);
    mulib_glx_drawlist_release(&shared);
}
int main(void) {
    mgx_surface s = {0};
    assert(mgx_surface_init(&s, 96, 96));
    test_snapshot(&s);
    test_ordered(&s);
    test_clips_and_stale(&s);
    test_slider(&s);
    test_nav_and_focus(&s);
    test_popup(&s);
    test_window_occlusion(&s);
    test_limits_and_ids(&s);
    test_adapter(&s);
    mgx_surface_free(&s);
    puts("PASS native ImmGTK: ordered input, clipping, focus, modal, limits, adapter");
    return 0;
}
