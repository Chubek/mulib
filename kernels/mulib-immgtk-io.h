/* Ordered, bounded input for the native overlay. Zero initialization is valid. */
#ifndef MULIB_IMMGTK_IO_H
#define MULIB_IMMGTK_IO_H
#include "mulib-immgtk-native.h"
#include "mulib-input-keyboard.h"
#include "mulib-input-mouse.h"
#include "mulib-textbuffer.h"
typedef struct {
    mulib_input_keyboard_t keys;
    mulib_input_mouse_t mouse;
    mulib_textbuffer_t text;
    double dt;
    mgx_event *events;
    size_t n, cap, max_events, max_text;
    int error, unfocused;
} mulib_immgtk_io_t;
static inline int mulib_immgtk_io_feed(mulib_immgtk_io_t *io, const mgx_event *e) {
    if (!io || !e || io->error) return 0;
    if (io->unfocused && e->type != MGX_EVENT_FOCUS) return 1;
    size_t text_n = 0;
    switch (e->type) {
    case MGX_EVENT_KEY:
        if (e->code >= 768 || e->down < 0 || e->down > 2) goto invalid;
        break;
    case MGX_EVENT_MOTION:
        if (!isfinite(e->x) || !isfinite(e->y)) goto invalid;
        break;
    case MGX_EVENT_BUTTON:
        if (e->code >= 32 || (e->down != 0 && e->down != 1)) goto invalid;
        break;
    case MGX_EVENT_SCROLL:
        if (!isfinite(e->dx) || !isfinite(e->dy) ||
            !isfinite(io->mouse.scroll_x + e->dx) ||
            !isfinite(io->mouse.scroll_y + e->dy)) goto invalid;
        break;
    case MGX_EVENT_TEXT:
        while (text_n < sizeof e->text && e->text[text_n]) ++text_n;
        if (!mgx_utf8_valid(e->text, text_n) ||
            text_n > (io->max_text ? io->max_text : 65536) ||
            io->text.n > (io->max_text ? io->max_text : 65536) - text_n) goto invalid;
        break;
    case MGX_EVENT_FOCUS:
        if (e->down != 0 && e->down != 1) goto invalid;
        break;
    default:
        return 0;
    }
    {
        size_t limit = io->max_events ? io->max_events : 4096;
        if (io->n >= limit || io->n >= SIZE_MAX / sizeof *io->events) goto invalid;
        if (io->n == io->cap) {
            size_t next = io->cap ? (io->cap > limit / 2 ? limit : io->cap * 2) :
                (limit < 8 ? limit : 8);
            if (next > SIZE_MAX / sizeof *io->events) next = io->n + 1;
            void *p = realloc(io->events, next * sizeof *io->events);
            if (!p) goto invalid;
            io->events = p;
            io->cap = next;
        }
    }
    mgx_event queued = *e;
    switch (e->type) {
    case MGX_EVENT_KEY:
        if (!mulib_input_keyboard_update(&io->keys, e->code, e->down)) goto invalid;
        queued.mods = io->keys.mods;
        break;
    case MGX_EVENT_MOTION:
        io->mouse.x = e->x;
        io->mouse.y = e->y;
        break;
    case MGX_EVENT_BUTTON:
        mulib_input_mouse_button(&io->mouse, e->code, e->down);
        break;
    case MGX_EVENT_SCROLL:
        io->mouse.scroll_x += e->dx;
        io->mouse.scroll_y += e->dy;
        break;
    case MGX_EVENT_TEXT:
        if (!mulib_textbuffer_replace(&io->text, io->text.n, io->text.n, e->text, text_n))
            goto invalid;
        break;
    case MGX_EVENT_FOCUS:
        io->unfocused = !e->down;
        if (!e->down) {
            memset(&io->keys, 0, sizeof io->keys);
            io->mouse.buttons = io->mouse.pressed = io->mouse.released = 0;
            (void)mulib_textbuffer_set(&io->text, NULL, 0);
        }
        break;
    default: break;
    }
    io->events[io->n++] = queued;
    return 1;
invalid:
    io->error = 1; /* Until reset: do not deliver a truncated gesture. */
    return 0;
}
static inline void mulib_immgtk_io_begin(mulib_immgtk_native_t *g, mgx_surface *s,
        mulib_immgtk_io_t *io) {
    if (!g || !io) return;
    if (io->error) {
        (void)mulib_immgtk_native_begin_events(g, s, NULL, 1);
    } else if (io->n || io->unfocused) {
        (void)mulib_immgtk_native_begin_events(g, s, io->events, io->n);
    } else if ((io->mouse.pressed | io->mouse.released) & 1) {
        /* Snapshot compatibility: infer the order from the final button state.
         * Feeding events is required to preserve event-time coordinates. */
        mgx_event events[6] = {{0}};
        size_t n = 0;
        events[n].type = MGX_EVENT_MOTION;
        events[n].x = io->mouse.x;
        events[n++].y = io->mouse.y;
        unsigned pressed = io->mouse.pressed & 1, released = io->mouse.released & 1;
        if (pressed && released) {
            events[n].type = MGX_EVENT_BUTTON;
            events[n++].down = !(io->mouse.buttons & 1);
        }
        events[n].type = MGX_EVENT_BUTTON;
        events[n++].down = pressed && !released ? 1 :
            released && !pressed ? 0 : !!(io->mouse.buttons & 1);
        if (io->keys.pressed[15]) {
            events[n].type = MGX_EVENT_KEY;
            events[n].code = 15;
            events[n].mods = io->keys.mods;
            events[n++].down = 1;
        }
        if (io->keys.pressed[28] || io->keys.pressed[57]) {
            events[n].type = MGX_EVENT_KEY;
            events[n].code = 28;
            events[n++].down = 1;
        }
        if (io->keys.pressed[1]) {
            events[n].type = MGX_EVENT_KEY;
            events[n].code = 1;
            events[n++].down = 1;
        }
        (void)mulib_immgtk_native_begin_events(g, s, events, n);
        g->scroll = io->mouse.scroll_y;
    } else {
        /* Compatibility for callers that directly fill the public snapshots. */
        mulib_immgtk_native_begin(g, s, io->mouse.x, io->mouse.y,
            !!(io->mouse.buttons & 1), 0);
        if (io->keys.pressed[15]) (void)mig_nav_step(g, !!(io->keys.mods & 1));
        if (io->keys.pressed[28] || io->keys.pressed[57]) {
            g->activate = 1;
            (void)mig_action_add(g, g->focus, 1);
        }
        g->escape = !!io->keys.pressed[1];
        g->scroll = io->mouse.scroll_y;
    }
}
static inline void mulib_immgtk_io_reset(mulib_immgtk_io_t *io) {
    if (!io) return;
    if (io->error) {
        memset(&io->keys, 0, sizeof io->keys);
        io->mouse.buttons = 0;
    }
    mulib_input_keyboard_frame(&io->keys);
    mulib_input_mouse_frame(&io->mouse);
    (void)mulib_textbuffer_set(&io->text, NULL, 0);
    io->n = 0;
    io->error = 0;
}
static inline void mulib_immgtk_io_free(mulib_immgtk_io_t *io) {
    if (!io) return;
    free(io->events);
    mulib_textbuffer_free(&io->text);
    memset(io, 0, sizeof *io);
}
#endif
