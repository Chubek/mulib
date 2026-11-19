#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Deterministic allocation failure, including failure of the second snapshot
 * allocation. Count only allocations made by the modules under test. */
static int fail_after = -1;
static size_t live_allocations;

static int allocation_fails(void)
{
    if (fail_after < 0) return 0;
    if (!fail_after) return 1;
    --fail_after;
    return 0;
}

static void *test_malloc(size_t n)
{
    void *p;
    if (allocation_fails()) return NULL;
    p = malloc(n);
    if (p) ++live_allocations;
    return p;
}

static void *test_realloc(void *p, size_t n)
{
    int new_allocation = p == NULL;
    void *q;
    if (allocation_fails()) return NULL;
    q = realloc(p, n);
    if (q && new_allocation) ++live_allocations;
    return q;
}

static void test_free(void *p)
{
    if (p) { assert(live_allocations); --live_allocations; }
    free(p);
}

#define MULIB_GLX_EVENT_REALLOC test_realloc
#define MULIB_GLX_EVENT_FREE test_free
#define MULIB_GLX_EVENTLOOP_MALLOC test_malloc
#define MULIB_GLX_EVENTLOOP_REALLOC test_realloc
#define MULIB_GLX_EVENTLOOP_FREE test_free
#include "mulib-glx-eventloop.h"
#include "mulib-input-compose.h"
#include "mulib-input-gamepad.h"
#include "mulib-input-gesture.h"
#include "mulib-input-seat.h"
#include "mulib-input-tablet.h"

int gfx_input_tu_smoke(void);
int gfx_input_unix_smoke(void);
int gfx_input_evdev_smoke(void);

static mgx_event key_event(unsigned code, int value)
{
    mgx_event e = {0};
    e.type = MGX_EVENT_KEY; e.code = code; e.down = value;
    return e;
}

static void test_queue(void)
{
    mulib_glx_event_queue_t q = {0};
    mgx_event e, sentinel = key_event(99, 1);
    mgx_event *original;
    assert(!mulib_glx_event_push(NULL, sentinel) && errno == EINVAL);
    assert(!mulib_glx_event_pop(NULL, &e));
    assert(!mulib_glx_event_set_limit(&q, 0));
    assert(!mulib_glx_event_set_limit(&q, SIZE_MAX));
    assert(mulib_glx_event_set_limit(&q, 3));
    assert(mulib_glx_event_push(&q, key_event(30, 1)));
    assert(mulib_glx_event_push(&q, key_event(30, 0)));
    assert(mulib_glx_event_push(&q, key_event(31, 1)));
    assert(q.cap == 3 && mulib_glx_event_count(&q) == 3);
    assert(!mulib_glx_event_set_limit(&q, 2));
    assert(!mulib_glx_event_push(&q, key_event(31, 0)) && errno == ENOBUFS);
    assert(q.overflowed && q.dropped == 1);
    assert(mulib_glx_event_pop(&q, &e) && e.code == 30 && e.down == 1);
    /* The consumed prefix supplies capacity without allocation. */
    fail_after = 0;
    assert(mulib_glx_event_push(&q, key_event(31, 0)));
    fail_after = -1;
    assert(mulib_glx_event_pop(&q, &e) && e.code == 30 && e.down == 0);
    assert(mulib_glx_event_pop(&q, &e) && e.code == 31 && e.down == 1);
    assert(mulib_glx_event_pop(&q, &e) && e.code == 31 && e.down == 0);
    e = sentinel;
    assert(!mulib_glx_event_pop(&q, &e) && !memcmp(&e, &sentinel, sizeof e));
    assert(!mulib_glx_event_pop(&q, NULL) && errno == EINVAL);
    assert(q.overflowed && q.dropped == 1);
    mulib_glx_event_acknowledge_loss(&q);
    assert(!q.overflowed && !q.dropped);
    mulib_glx_event_free(&q);

    fail_after = 0;
    assert(!mulib_glx_event_push(&q, sentinel) && errno == ENOMEM);
    assert(!q.v && !q.n && q.dropped == 1 && q.overflowed);
    fail_after = -1;
    for (unsigned i = 0; i < 8; ++i)
        assert(mulib_glx_event_push(&q, key_event(i, 1)));
    original = q.v;
    fail_after = 0;
    assert(!mulib_glx_event_push(&q, sentinel) && errno == ENOMEM);
    assert(q.v == original && q.n == 8 && q.cap == 8 && q.dropped == 2);
    q.dropped = SIZE_MAX;
    assert(!mulib_glx_event_push(&q, sentinel) && q.dropped == SIZE_MAX);
    fail_after = -1;
    for (unsigned i = 0; i < 8; ++i)
        assert(mulib_glx_event_pop(&q, &e) && e.code == i);
    mulib_glx_event_free(&q);

    for (size_t i = 0; i < MULIB_GLX_EVENT_LIMIT; ++i)
        assert(mulib_glx_event_push(&q, key_event((unsigned)(i % 768), 1)));
    assert(!mulib_glx_event_push(&q, sentinel) && errno == ENOBUFS);
    assert(q.cap <= MULIB_GLX_EVENT_LIMIT);
    mulib_glx_event_clear(&q);
    assert(!mulib_glx_event_count(&q) && q.overflowed && q.v);
    mulib_glx_event_acknowledge_loss(&q);
    assert(!q.overflowed);
    mulib_glx_event_free(&q);
    mulib_glx_event_free(&q);
    mulib_glx_event_free(NULL);
}

static void test_event_validation(void)
{
    mulib_glx_event_queue_t q = {0};
    mgx_event e = key_event(768, 1);
    assert(!mulib_glx_event_push(&q, e) && errno == EINVAL);
    assert(!q.overflowed && !q.dropped && !q.v);
    e = key_event(10, 3);
    assert(!mulib_glx_event_valid(&e));
    e = key_event(10, 2);
    assert(mulib_glx_event_valid(&e));
    e.type = MGX_EVENT_BUTTON; e.code = 32; e.down = 1;
    assert(!mulib_glx_event_valid(&e));
    e.code = 31; assert(mulib_glx_event_valid(&e));
    e.down = 2; assert(!mulib_glx_event_valid(&e));
    e.type = MGX_EVENT_FOCUS;
    assert(!mulib_glx_event_valid(&e));
    e.type = MGX_EVENT_TOUCH; e.down = 0; e.code = 31;
    assert(mulib_glx_event_valid(&e));
    e.code = 32; assert(!mulib_glx_event_valid(&e));
    e.type = MGX_EVENT_RESIZE; e.x = 1; e.y = INT_MAX;
    assert(mulib_glx_event_valid(&e));
    e.x = 1.5; assert(!mulib_glx_event_valid(&e));
    e.x = 0; assert(!mulib_glx_event_valid(&e));
    e.x = INFINITY; assert(!mulib_glx_event_valid(&e));
    e = (mgx_event){0}; e.type = (mgx_event_type)100;
    assert(!mulib_glx_event_valid(&e));
    e.type = MGX_EVENT_MOTION; e.dx = NAN;
    assert(!mulib_glx_event_valid(&e));
    e = (mgx_event){0}; e.type = MGX_EVENT_TEXT;
    assert(mulib_glx_event_valid(&e));
    memcpy(e.text, "A\xc3\xa9\xf0\x9f\x98\x80", 8);
    assert(mulib_glx_event_valid(&e));
    memset(e.text, 'x', sizeof e.text);
    assert(!mulib_glx_event_valid(&e));
    e.text[31] = 0; assert(mulib_glx_event_valid(&e));
    memcpy(e.text, "\xc0\x80", 3); assert(!mulib_glx_event_valid(&e));
    memcpy(e.text, "\xed\xa0\x80", 4); assert(!mulib_glx_event_valid(&e));
    memcpy(e.text, "\xf4\x90\x80\x80", 5); assert(!mulib_glx_event_valid(&e));
    memcpy(e.text, "\xe2\x82", 3); assert(!mulib_glx_event_valid(&e));
    memcpy(e.text, "\x80", 2); assert(!mulib_glx_event_valid(&e));
    memset(e.text, 'x', sizeof e.text); e.text[31] = (char)0xc2;
    assert(!mulib_glx_event_valid(&e));
}

static void test_fifo_model(void)
{
    mulib_glx_event_queue_t q = {0};
    mgx_event model[17], e;
    size_t first = 0, n = 0;
    uint32_t random = 0x147b238u;
    assert(mulib_glx_event_set_limit(&q, 17));
    for (unsigned i = 0; i < 20000; ++i) {
        random = random * 1664525u + 1013904223u;
        if (random & 0x1000u) {
            int ok;
            e = key_event((random >> 8) % 768, (int)((random >> 20) % 3));
            fail_after = (random & 8) ? 0 : -1;
            ok = mulib_glx_event_push(&q, e);
            fail_after = -1;
            if (ok) {
                assert(n < 17);
                model[(first + n++) % 17] = e;
            } else assert(errno == ENOBUFS || errno == ENOMEM);
        } else {
            int ok = mulib_glx_event_pop(&q, &e);
            assert(ok == (n != 0));
            if (ok) {
                assert(!memcmp(&e, &model[first], sizeof e));
                first = (first + 1) % 17; --n;
            }
        }
        assert(mulib_glx_event_count(&q) == n && q.cap <= 17);
    }
    mulib_glx_event_free(&q);
}

static void test_keyboard_keymap(void)
{
    mulib_input_keyboard_t k = {0}, saved;
    mulib_input_keymap_t map;
    assert(mulib_input_keyboard_update(&k, 30, 1));
    assert(mulib_input_keyboard_update(&k, 30, 1));
    assert(mulib_input_keyboard_update(&k, 30, 2));
    assert(mulib_input_keyboard_update(&k, 30, 0));
    assert(k.pressed[30] && k.released[30] && k.repeated[30] && !k.down[30]);
    saved = k;
    assert(!mulib_input_keyboard_update(&k, 30, 2));
    assert(!mulib_input_keyboard_update(&k, 768, 1));
    assert(!mulib_input_keyboard_update(&k, UINT_MAX, 1));
    assert(!mulib_input_keyboard_update(&k, 0, -1));
    assert(!memcmp(&saved, &k, sizeof k));
    assert(!mulib_input_keyboard_update(NULL, 0, 1));
    mulib_input_keyboard_frame(&k);
    assert(!k.pressed[30] && !k.released[30] && !k.repeated[30]);
    assert(mulib_input_keyboard_update(&k, 42, 1));
    assert(mulib_input_keyboard_update(&k, 54, 1));
    assert(mulib_input_keyboard_update(&k, 42, 0));
    assert(k.mods == MULIB_INPUT_MOD_SHIFT);
    assert(mulib_input_keyboard_update(&k, 97, 1));
    assert(mulib_input_keyboard_update(&k, 100, 1));
    assert(mulib_input_keyboard_update(&k, 126, 1));
    assert(k.mods == 15);
    mulib_input_keyboard_cancel(&k);
    assert(!k.mods && k.released[54] && k.released[97] && k.released[100]);
    assert(k.pressed[54] && !k.down[54]);
    mulib_input_keymap_us(&map);
    assert(mulib_input_keymap_translate(&map, 30, 0, 0) == 'a');
    assert(mulib_input_keymap_translate(&map, 30, 1, 0) == 'A');
    assert(mulib_input_keymap_translate(&map, 30, 0, 1) == 'A');
    assert(mulib_input_keymap_translate(&map, 30, 1, 1) == 'a');
    assert(mulib_input_keymap_translate(&map, 2, 1, 1) == '!');
    assert(mulib_input_keymap_translate(&map, 41, 0, 0) == '`');
    assert(mulib_input_keymap_translate(&map, 41, 1, 0) == '~');
    assert(!mulib_input_keymap_translate(&map, 42, 1, 0));
    assert(mulib_input_keymap_translate(&map, 43, 1, 0) == '|');
    assert(mulib_input_keymap_translate(&map, 15, 0, 0) == '\t');
    for (unsigned mods = 2; mods < 16; ++mods)
        assert(!mulib_input_keymap_translate(&map, 30, mods, 0));
    assert(!mulib_input_keymap_translate(&map, 768, 0, 0));
    assert(!mulib_input_keymap_translate(NULL, 0, 0, 0));
    map.normal[30] = 0xd800;
    assert(!mulib_input_keymap_translate(&map, 30, 0, 0));
}

static void test_compose(void)
{
    mulib_input_compose_t c = {0};
    uint32_t out[2] = {1234, 5678};
    const uint32_t marks[] = {0x301, 0x300, 0x308, 0x303};
    const uint32_t result[] = {225, 224, 228, 227};
    for (size_t i = 0; i < 4; ++i) {
        assert(mulib_input_compose_feed(&c, marks[i], out) == 0);
        assert(mulib_input_compose_feed(&c, 'a', out) == 1 && out[0] == result[i]);
    }
    assert(mulib_input_compose_feed(&c, 0x303, out) == 0);
    assert(mulib_input_compose_feed(&c, 'N', out) == 1 && out[0] == 209);
    assert(mulib_input_compose_feed(&c, 0x308, out) == 0);
    assert(mulib_input_compose_feed(&c, 'Y', out) == 1 && out[0] == 376);
    assert(mulib_input_compose_feed(&c, 0x301, out) == 0);
    assert(mulib_input_compose_feed(&c, 'x', out) == 2);
    assert(out[0] == 'x' && out[1] == 0x301);
    assert(mulib_input_compose_feed(&c, 0x301, out) == 0);
    assert(mulib_input_compose_feed(&c, ' ', out) == 1 && out[0] == 0xb4);
    assert(mulib_input_compose_feed(&c, 0x303, out) == 0);
    assert(mulib_input_compose_feed(&c, 0x303, out) == 1 && out[0] == '~');
    assert(mulib_input_compose_feed(&c, 0x301, out) == 0);
    out[0] = 1234; out[1] = 5678;
    assert(mulib_input_compose_feed(&c, 0xd800, out) == SIZE_MAX);
    assert(mulib_input_compose_feed(&c, 0x110000, out) == SIZE_MAX);
    assert(mulib_input_compose_feed(&c, 'a', NULL) == SIZE_MAX);
    assert(c.dead == 0x301 && out[0] == 1234 && out[1] == 5678);
    mulib_input_compose_cancel(&c);
    assert(mulib_input_compose_feed(&c, 0x10ffff, out) == 1 && out[0] == 0x10ffff);
    c.dead = 1;
    assert(mulib_input_compose_feed(&c, 'a', out) == SIZE_MAX && c.dead == 1);
}

static void test_pointer_gamepad_tablet(void)
{
    mulib_input_mouse_t m = {0}, saved;
    mulib_input_gamepad_t g = {0}, gsaved;
    mulib_input_tablet_t t = {0}, tsaved;
    double axis = 99;
    assert(mulib_input_mouse_motion_checked(&m, 500, -10, 100, 20, 1));
    assert(m.x == 99 && m.y == 0);
    assert(mulib_input_mouse_motion_checked(&m, DBL_MAX, DBL_MAX, 100, 20, 0));
    saved = m;
    assert(!mulib_input_mouse_motion_checked(&m, DBL_MAX, 1, 100, 20, 2));
    assert(!mulib_input_mouse_motion_checked(&m, NAN, 1, 100, 20, 1));
    assert(!mulib_input_mouse_motion_checked(&m, 1, 1, 0, 20, 1));
    assert(!mulib_input_mouse_motion_checked(&m, 1, 1, 10, 20, -1));
    assert(!memcmp(&m, &saved, sizeof m));
    assert(mulib_input_mouse_button_checked(&m, 31, 1));
    assert(mulib_input_mouse_button_checked(&m, 31, 0));
    assert(!m.buttons && (m.pressed & (1u << 31)) && (m.released & (1u << 31)));
    saved = m;
    assert(!mulib_input_mouse_button_checked(&m, 32, 1));
    assert(!mulib_input_mouse_button_checked(&m, 0, 2));
    assert(!memcmp(&m, &saved, sizeof m));
    assert(mulib_input_mouse_scroll(&m, 1.5, -2.5));
    assert(!mulib_input_mouse_scroll(&m, INFINITY, 0));
    m.scroll_x = DBL_MAX;
    assert(!mulib_input_mouse_scroll(&m, DBL_MAX, 10) && m.scroll_y == -2.5);
    mulib_input_mouse_frame(&m);
    assert(!m.pressed && !m.released && !m.scroll_x && !m.scroll_y);
    assert(mulib_input_mouse_button_checked(&m, 0, 1));
    mulib_input_mouse_cancel(&m);
    assert(!m.buttons && m.pressed == 1 && m.released == 1);
    assert(mulib_input_mouse_motion_checked(&m, -1, 1, 1, 1, 1));
    assert(!m.x && !m.y);

    assert(mulib_input_gamepad_axis(&g, 15, -0.6, 0.2));
    assert(fabs(g.axis[15] + 0.5) < 1e-12);
    assert(mulib_input_gamepad_axis(&g, 0, 0.2, 0.2) && !g.axis[0]);
    assert(mulib_input_gamepad_axis(&g, 0, DBL_MAX, 0.2) && g.axis[0] == 1);
    gsaved = g;
    assert(!mulib_input_gamepad_axis(&g, 16, 0, 0));
    assert(!mulib_input_gamepad_axis(&g, 0, 0, 1));
    assert(!mulib_input_gamepad_axis(&g, 0, NAN, 0));
    assert(!memcmp(&g, &gsaved, sizeof g));
    assert(mulib_input_gamepad_button_checked(&g, 63, 1));
    assert(mulib_input_gamepad_button_checked(&g, 63, 0));
    assert(!g.buttons && g.pressed == (UINT64_C(1) << 63) && g.released == g.pressed);
    assert(!mulib_input_gamepad_button_checked(&g, 64, 1));
    assert(!mulib_input_gamepad_button_checked(&g, 0, -1));
    mulib_input_gamepad_frame(&g);
    assert(!g.pressed && !g.released);
    mulib_input_gamepad_button(&g, 0, 1);
    mulib_input_gamepad_cancel(&g);
    assert(g.released == 1 && !g.buttons && !g.axis[0] && !g.axis[15]);

    assert(mulib_input_tablet_axis_checked(INT_MAX, INT_MIN, INT_MAX, &axis));
    assert(axis == 1);
    assert(mulib_input_tablet_axis_checked(INT_MIN, INT_MIN, INT_MAX, &axis));
    assert(axis == 0);
    axis = 99;
    assert(!mulib_input_tablet_axis_checked(0, 1, 1, &axis) && axis == 99);
    assert(!mulib_input_tablet_axis_checked(0, 2, 1, &axis) && axis == 99);
    assert(mulib_input_tablet_pressure_checked(&t, 100, 0, 100) && !t.pressure);
    assert(mulib_input_tablet_proximity(&t, 1));
    assert(mulib_input_tablet_pressure_checked(&t, 200, 0, 100));
    assert(t.pressure == 1 && t.tip);
    tsaved = t;
    assert(!mulib_input_tablet_pressure_checked(&t, 0, 5, 5));
    assert(!mulib_input_tablet_motion(&t, NAN, 1, 0, 0));
    assert(!mulib_input_tablet_proximity(&t, 2));
    assert(!memcmp(&t, &tsaved, sizeof t));
    assert(mulib_input_tablet_motion(&t, 10, 20, 5, -5));
    assert(t.x == 10 && t.y == 20 && t.tilt_x == 1 && t.tilt_y == -1);
    assert(mulib_input_tablet_proximity(&t, 0) && !t.pressure && !t.tip);
}

static void test_touch_gesture(void)
{
    mulib_input_touch_t t = {0}, saved;
    mulib_input_gesture_t g = {0};
    mgx_gesture_delta d;
    for (int i = 0; i < 32; ++i)
        assert(mulib_input_touch_update(&t, i, i, i, i, 2));
    assert(mulib_input_touch_count(&t) == 32 && t.pressed == UINT32_MAX);
    saved = t;
    assert(!mulib_input_touch_update(&t, 32, 32, 0, 0, 0));
    assert(!mulib_input_touch_update(&t, -1, 32, 0, 0, 0));
    assert(!mulib_input_touch_update(&t, 0, -2, 0, 0, 0));
    assert(!mulib_input_touch_update(&t, 0, 1, 0, 0, 0) && errno == EEXIST);
    assert(!mulib_input_touch_update(&t, 0, 0, NAN, 0, 0));
    assert(!memcmp(&t, &saved, sizeof t));
    mulib_input_touch_cancel(&t);
    assert(!mulib_input_touch_count(&t) && t.released == UINT32_MAX);
    mulib_input_touch_frame(&t);
    assert(!t.pressed && !t.released);
    assert(mulib_input_touch_update(&t, 0, 100, 0, 0, 0.5));
    assert(mulib_input_touch_update(&t, 1, 101, 10, 0, 0.5));
    d = mulib_input_gesture_update(&g, &t);
    assert(!d.active && d.scale == 1);
    assert(mulib_input_touch_update(&t, 1, 101, 20, 0, 0.5));
    d = mulib_input_gesture_update(&g, &t);
    assert(d.active && d.dx == 5 && !d.dy && d.scale == 2 && !d.rotation);
    /* Same tracking ID reused in the same slot still resets the baseline. */
    assert(mulib_input_touch_update(&t, 1, -1, 20, 0, 1));
    assert(!t.v[1].pressure);
    assert(mulib_input_touch_update(&t, 1, 101, 100, 0, -1));
    d = mulib_input_gesture_update(&g, &t);
    assert(!d.active && !t.v[1].pressure);
    assert(mulib_input_touch_update(&t, 1, 102, 20, 0, 0.5));
    assert(!mulib_input_gesture_update(&g, &t).active);
    assert(mulib_input_touch_update(&t, 1, 102, 0, 0, 0.5));
    d = mulib_input_gesture_update(&g, &t);
    assert(d.active && d.scale == 0);
    assert(mulib_input_touch_update(&t, 1, 102, 10, 0, 0.5));
    assert(!mulib_input_gesture_update(&g, &t).active);
    assert(mulib_input_touch_update(&t, 1, 102, -10, 0.1, 0.5));
    (void)mulib_input_gesture_update(&g, &t);
    assert(mulib_input_touch_update(&t, 1, 102, -10, -0.1, 0.5));
    d = mulib_input_gesture_update(&g, &t);
    assert(d.active && d.rotation > 0 && d.rotation < 0.03);
    t.v[1].x = NAN;
    d = mulib_input_gesture_update(&g, &t);
    assert(!d.active && !g.tracking && isfinite(d.scale));
    t.v[1].x = DBL_MAX; t.v[0].x = -DBL_MAX;
    d = mulib_input_gesture_update(&g, &t);
    assert(!d.active && !g.tracking);
    assert(!mulib_input_gesture_update(NULL, &t).active);
}

static void test_seat(void)
{
    mulib_input_seat_t s, saved;
    unsigned char snapshot[768] = {0};
    mgx_event e, sentinel = key_event(90, 1);
    mulib_input_event_t raw = {100, MULIB_INPUT_KEY, 30, 1};
    mulib_input_seat_init(&s);
    mulib_input_seat_frame(&s);
    assert(mulib_input_seat_feed(&s, &raw, 100, 100, &e) == 1);
    assert(e.type == MGX_EVENT_KEY && e.down == 1 && e.code == 30 && e.time_us == 100);
    raw.time_us = 90; raw.value = 0;
    assert(mulib_input_seat_feed(&s, &raw, 100, 100, &e) == 1);
    assert(e.time_us == 100 && !e.down && s.keyboard.pressed[30] && s.keyboard.released[30]);
    assert(mulib_input_seat_evdev(&s, 1, 272, 1, 100, 100));
    assert(mulib_input_seat_evdev(&s, 1, 272, 0, 100, 100));
    assert(s.mouse.pressed == 1 && s.mouse.released == 1 && !s.mouse.buttons);
    saved = s;
    assert(!mulib_input_seat_evdev(&s, 1, 272, 2, 100, 100));
    assert(!mulib_input_seat_evdev(&s, 1, 768, 1, 100, 100));
    assert(!mulib_input_seat_evdev(&s, 1, 30, 3, 100, 100));
    assert(!mulib_input_seat_evdev(&s, 1, 30, 2, 100, 100));
    assert(!mulib_input_seat_evdev(&s, 1, 304, 1, 100, 100) && errno == ENOTSUP);
    assert(!mulib_input_seat_evdev(&s, 3, 0, 1, 100, 100) && errno == ENOTSUP);
    assert(!mulib_input_seat_evdev(&s, 2, 0, 1, 0, 100));
    assert(!mulib_input_seat_evdev(&s, 0, 3, 1, 100, 100));
    assert(!memcmp(&s, &saved, sizeof s));
    assert(mulib_input_seat_evdev(&s, 1, 58, 1, 100, 100) && s.caps);
    assert(mulib_input_seat_evdev(&s, 1, 58, 1, 100, 100) && s.caps);
    assert(mulib_input_seat_evdev(&s, 1, 58, 2, 100, 100) && s.caps);
    assert(mulib_input_seat_evdev(&s, 1, 58, 0, 100, 100) && s.caps);
    assert(mulib_input_seat_evdev(&s, 1, 58, 1, 100, 100) && !s.caps);
    raw = (mulib_input_event_t){101, MULIB_INPUT_REL, 0, 500};
    assert(mulib_input_seat_feed(&s, &raw, 100, 100, &e) == 1);
    assert(e.type == MGX_EVENT_MOTION && e.x == 99 && e.dx == 99);
    raw = (mulib_input_event_t){102, MULIB_INPUT_REL, 8, -2};
    assert(mulib_input_seat_feed(&s, &raw, 100, 100, &e) == 1);
    assert(e.type == MGX_EVENT_SCROLL && e.dy == -2 && s.mouse.scroll_y == -2);
    raw = (mulib_input_event_t){103, MULIB_INPUT_KEY, 273, 1};
    assert(mulib_input_seat_feed(&s, &raw, 100, 100, &e) == 1);
    assert(e.type == MGX_EVENT_BUTTON && e.code == 1 && e.down == 1);
    assert(mulib_input_seat_evdev(&s, 1, 42, 1, 100, 100));
    assert(mulib_input_touch_update(&s.touch, 0, 4, 1, 2, 0.5));
    raw = (mulib_input_event_t){104, MULIB_INPUT_SYN, 3, 0};
    e = sentinel;
    assert(mulib_input_seat_feed(&s, &raw, 100, 100, &e) == 0);
    assert(!memcmp(&e, &sentinel, sizeof e) && s.sync_lost && !s.resync_ready);
    assert(!s.keyboard.down[42] && s.keyboard.released[42] && !s.keyboard.mods);
    assert(!s.mouse.buttons && (s.mouse.released & 2) && !mulib_input_touch_count(&s.touch));
    assert(!mulib_input_seat_resync(&s, snapshot, 0, 0));
    assert(!mulib_input_seat_evdev(&s, 1, 42, 1, 100, 100) && errno == EAGAIN);
    assert(mulib_input_seat_evdev(&s, 0, 0, 0, 100, 100) && s.resync_ready);
    assert(!mulib_input_seat_evdev(&s, 2, 0, 1, 100, 100) && errno == EAGAIN);
    saved = s;
    snapshot[30] = 2;
    assert(!mulib_input_seat_resync(&s, snapshot, 0, 0));
    assert(!memcmp(&s, &saved, sizeof s));
    snapshot[30] = 1; snapshot[42] = 1;
    assert(!mulib_input_seat_resync(&s, snapshot, 256, 0));
    assert(mulib_input_seat_resync(&s, snapshot, 1, 1));
    assert(!s.sync_lost && !s.resync_ready && s.caps && s.keyboard.mods == 1);
    assert(s.keyboard.down[30] && s.keyboard.down[42] && s.mouse.buttons == 1);
    mulib_input_seat_frame(&s);
    assert(!s.keyboard.pressed[30] && !s.keyboard.released[42] && !s.mouse.pressed);
    assert(mulib_input_seat_evdev(&s, 1, 30, 0, 100, 100) && s.keyboard.released[30]);
    mulib_input_seat_cancel(&s);
    assert(!s.keyboard.mods && !s.mouse.buttons && s.mouse.released == 1);
}

static void nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    assert(flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

typedef struct {
    mulib_glx_eventloop_t *loop;
    int a, b, calls, replaced_calls, old_calls;
    int extra[10];
} mutation_context;

static void replacement_callback(void *user, short events)
{
    mutation_context *c = (mutation_context *)user;
    char ch;
    assert(events & POLLIN);
    assert(read(c->b, &ch, 1) == 1);
    ++c->replaced_calls;
}

static void old_callback(void *user, short events)
{
    mutation_context *c = (mutation_context *)user;
    (void)events;
    ++c->old_calls;
}

static void unused_callback(void *user, short events)
{
    (void)user; (void)events;
    assert(!"a non-ready watch was dispatched");
}

static void mutation_callback(void *user, short events)
{
    mutation_context *c = (mutation_context *)user;
    char ch;
    size_t n = c->loop->n;
    assert(events & POLLIN);
    assert(read(c->a, &ch, 1) == 1);
    ++c->calls;
    assert(mulib_glx_eventloop_poll(c->loop, 0) == -1 && errno == EBUSY);
    mulib_glx_eventloop_free(c->loop);
    assert(errno == EBUSY && c->loop->running && c->loop->n == n);
    assert(mulib_glx_eventloop_remove(c->loop, c->b));
    assert(mulib_glx_eventloop_add(c->loop, c->b, POLLIN, replacement_callback, c));
    for (size_t i = 0; i < 10; ++i)
        assert(mulib_glx_eventloop_add(c->loop, c->extra[i], POLLIN, unused_callback, c));
    assert(mulib_glx_eventloop_remove(c->loop, c->a));
}

static void test_loop_mutation(void)
{
    mulib_glx_eventloop_t loop = {0};
    mutation_context c = {0};
    int a[2], b[2], extra[10][2];
    c.loop = &loop;
    assert(pipe(a) == 0 && pipe(b) == 0);
    c.a = a[0]; c.b = b[0];
    nonblocking(a[0]); nonblocking(b[0]);
    for (size_t i = 0; i < 10; ++i) {
        assert(pipe(extra[i]) == 0);
        c.extra[i] = extra[i][0];
    }
    assert(mulib_glx_eventloop_add(&loop, a[0], POLLIN, mutation_callback, &c));
    assert(mulib_glx_eventloop_add(&loop, b[0], POLLIN, old_callback, &c));
    assert(!mulib_glx_eventloop_add(&loop, b[0], POLLIN, old_callback, &c) && errno == EEXIST);
    assert(write(a[1], "a", 1) == 1 && write(b[1], "b", 1) == 1);
    assert(mulib_glx_eventloop_poll(&loop, 100) == 2);
    assert(c.calls == 1 && !c.old_calls && !c.replaced_calls);
    assert(loop.cap >= 12 && loop.n == 11 && !loop.running);
    assert(mulib_glx_eventloop_poll(&loop, 100) == 1);
    assert(c.replaced_calls == 1 && !c.old_calls);
    assert(mulib_glx_eventloop_poll(&loop, 0) == 0);
    mulib_glx_eventloop_free(&loop);
    assert(fcntl(b[0], F_GETFD) >= 0); /* Borrowed fd wasn't closed. */
    assert(close(a[0]) == 0 && close(a[1]) == 0 && close(b[0]) == 0 && close(b[1]) == 0);
    for (size_t i = 0; i < 10; ++i) {
        assert(close(extra[i][0]) == 0 && close(extra[i][1]) == 0);
    }
}

typedef struct { int calls; short events; } terminal_context;
static void terminal_callback(void *user, short events)
{
    terminal_context *c = (terminal_context *)user;
    ++c->calls; c->events = events;
}

typedef struct {
    mulib_glx_eventloop_t *loop;
    int reader, writer, calls;
    terminal_context replacement;
} callback_failure_context;

static void callback_allocation_failure(void *user, short events)
{
    callback_failure_context *c = (callback_failure_context *)user;
    char ch;
    assert(events & POLLIN);
    assert(read(c->reader, &ch, 1) == 1);
    ++c->calls;
    fail_after = 0;
    assert(!mulib_glx_eventloop_add(c->loop, c->writer, POLLOUT,
                                   terminal_callback, &c->replacement));
    assert(errno == ENOMEM && c->loop->n == 1 && c->loop->cap == 1
           && c->loop->next_id == 1 && c->loop->running);
    fail_after = -1;
    assert(mulib_glx_eventloop_remove(c->loop, c->reader));
    assert(mulib_glx_eventloop_add(c->loop, c->writer, POLLOUT,
                                  terminal_callback, &c->replacement));
}

static void test_loop_callback_failure(void)
{
    mulib_glx_eventloop_t loop = {0};
    callback_failure_context c = {0};
    int p[2];
    assert(pipe(p) == 0);
    c.loop = &loop; c.reader = p[0]; c.writer = p[1];
    assert(mulib_glx_eventloop_set_limit(&loop, 1));
    assert(mulib_glx_eventloop_add(&loop, p[0], POLLIN, callback_allocation_failure, &c));
    assert(mulib_glx_eventloop_set_limit(&loop, 2));
    assert(write(p[1], "x", 1) == 1);
    assert(mulib_glx_eventloop_poll(&loop, 100) == 1);
    assert(c.calls == 1 && !c.replacement.calls && loop.n == 1);
    assert(mulib_glx_eventloop_poll(&loop, 100) == 1);
    assert(c.replacement.calls == 1);
    mulib_glx_eventloop_free(&loop);
    assert(close(p[0]) == 0 && close(p[1]) == 0);
}

static double monotonic_seconds(void)
{
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static volatile sig_atomic_t interrupted;
static void alarm_handler(int signal_number)
{
    (void)signal_number;
    interrupted = 1;
}

static void test_loop_terminals_wait(void)
{
    mulib_glx_eventloop_t loop = {0};
    terminal_context c = {0};
    int p[2], sockets[2], closed_fd;
    double start;
    struct sigaction action = {0}, old_action;
    struct itimerval timer = {{0, 0}, {0, 5000}};
    assert(pipe(p) == 0);
    assert(mulib_glx_eventloop_add(&loop, p[0], POLLIN, terminal_callback, &c));
    assert(close(p[1]) == 0);
    assert(mulib_glx_eventloop_poll(&loop, 100) == 1);
    assert(c.calls == 1 && (c.events & POLLHUP) && !loop.n);
    start = monotonic_seconds();
    assert(mulib_glx_eventloop_poll(&loop, 20) == 0);
    assert(monotonic_seconds() - start >= 0.01 && c.calls == 1);
    assert(close(p[0]) == 0);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    nonblocking(sockets[0]);
    assert(mulib_glx_eventloop_add(&loop, sockets[0], POLLIN, terminal_callback, &c));
    assert(close(sockets[1]) == 0);
    assert(mulib_glx_eventloop_poll(&loop, 100) == 1);
    assert(c.calls == 2 && (c.events & POLLHUP) && !loop.n);
    assert(close(sockets[0]) == 0);

    assert(pipe(p) == 0);
    closed_fd = p[0];
    assert(close(p[0]) == 0);
    assert(mulib_glx_eventloop_add(&loop, closed_fd, POLLIN, terminal_callback, &c));
    assert(mulib_glx_eventloop_poll(&loop, 100) == 1);
    assert(c.calls == 3 && (c.events & POLLNVAL) && !loop.n);
    assert(close(p[1]) == 0);

    assert(pipe(p) == 0);
    assert(mulib_glx_eventloop_add(&loop, p[1], POLLOUT, terminal_callback, &c));
    assert(close(p[0]) == 0);
    assert(mulib_glx_eventloop_poll(&loop, 100) == 1);
    assert(c.calls == 4 && (c.events & POLLERR) && !loop.n);
    assert(close(p[1]) == 0);
    assert(mulib_glx_eventloop_poll(&loop, -2) == -1 && errno == EINVAL);
    assert(mulib_glx_eventloop_poll(NULL, 0) == -1 && errno == EINVAL);

    action.sa_handler = alarm_handler;
    assert(sigemptyset(&action.sa_mask) == 0);
    assert(sigaction(SIGALRM, &action, &old_action) == 0);
    assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
    assert(mulib_glx_eventloop_poll(&loop, 1000) == -1 && errno == EINTR);
    assert(interrupted && !loop.running);
    timer.it_value.tv_usec = 0;
    assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
    assert(sigaction(SIGALRM, &old_action, NULL) == 0);
    mulib_glx_eventloop_free(&loop);
}

static void test_loop_failure(void)
{
    mulib_glx_eventloop_t loop = {0};
    terminal_context c = {0};
    int p[2];
    size_t allocations;
    assert(pipe(p) == 0);
    fail_after = 0;
    assert(!mulib_glx_eventloop_add(&loop, p[0], POLLIN, terminal_callback, &c));
    assert(errno == ENOMEM && !loop.n && !loop.next_id && !loop.v);
    fail_after = -1;
    assert(mulib_glx_eventloop_set_limit(&loop, 1));
    assert(mulib_glx_eventloop_add(&loop, p[0], POLLIN, terminal_callback, &c));
    assert(loop.cap == 1);
    assert(!mulib_glx_eventloop_add(&loop, p[1], POLLOUT, terminal_callback, &c)
           && errno == ENOSPC);
    assert(!mulib_glx_eventloop_set_limit(&loop, 0));
    assert(!mulib_glx_eventloop_set_limit(&loop, SIZE_MAX));
    assert(mulib_glx_eventloop_set_limit(&loop, 2));
    fail_after = 0;
    assert(!mulib_glx_eventloop_add(&loop, p[1], POLLOUT, terminal_callback, &c)
           && errno == ENOMEM);
    assert(loop.n == 1 && loop.cap == 1 && loop.next_id == 1);
    assert(write(p[1], "x", 1) == 1);
    allocations = live_allocations;
    for (int fail = 0; fail < 2; ++fail) {
        fail_after = fail;
        assert(mulib_glx_eventloop_poll(&loop, 0) == -1 && errno == ENOMEM);
        assert(!loop.running && loop.n == 1 && !c.calls && live_allocations == allocations);
    }
    fail_after = -1;
    assert(mulib_glx_eventloop_poll(&loop, 0) == 1 && c.calls == 1);
    loop.next_id = UINT64_MAX;
    assert(!mulib_glx_eventloop_add(&loop, p[1], POLLOUT, terminal_callback, &c)
           && errno == EOVERFLOW);
    assert(!mulib_glx_eventloop_add(&loop, -1, POLLIN, terminal_callback, &c));
    assert(!mulib_glx_eventloop_add(&loop, p[1], POLLHUP, terminal_callback, &c));
    assert(!mulib_glx_eventloop_add(&loop, p[1], 0, terminal_callback, &c));
    assert(!mulib_glx_eventloop_add(&loop, p[1], POLLOUT, NULL, &c));
    assert(mulib_glx_eventloop_remove(&loop, p[0]));
    assert(!mulib_glx_eventloop_remove(&loop, p[0]) && errno == ENOENT);
    mulib_glx_eventloop_free(&loop);
    assert(close(p[0]) == 0 && close(p[1]) == 0);
}

int main(void)
{
    test_queue();
    test_event_validation();
    test_fifo_model();
    test_keyboard_keymap();
    test_compose();
    test_pointer_gamepad_tablet();
    test_touch_gesture();
    test_seat();
    test_loop_mutation();
    test_loop_callback_failure();
    test_loop_terminals_wait();
    test_loop_failure();
    assert(gfx_input_tu_smoke());
    assert(gfx_input_unix_smoke());
    assert(gfx_input_evdev_smoke());
    assert(!live_allocations);
    puts("gfx-input: FIFO, normalization, loss recovery, and poll tests passed");
    return 0;
}
