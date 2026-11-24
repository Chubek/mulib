/* MuLIB graphical extension: input-seat. C11, header-only. */
#ifndef MULIB_INPUT_SEAT_H
#define MULIB_INPUT_SEAT_H
#include "mulib-input-keymap.h"
#include "mulib-input-mouse.h"
#include "mulib-input-touch.h"
#include "mulib-input.h"
#include "mulib-glx-event.h"
/* Local state for ONE caller-owned keyboard/pointer stream, not seat/logind
 * ownership. Do not combine independent devices without per-device state. */
typedef struct {
    mulib_input_keyboard_t keyboard;
    mulib_input_mouse_t mouse;
    mulib_input_touch_t touch;
    mulib_input_keymap_t keymap;
    int caps;
    int sync_lost, resync_ready;
    uint64_t time_us;
} mulib_input_seat_t;

static inline void mulib_input_seat_init(mulib_input_seat_t *s)
{
    if (s) { memset(s, 0, sizeof *s); mulib_input_keymap_us(&s->keymap); }
}

static inline void mulib_input_seat_frame(mulib_input_seat_t *s)
{
    if (!s) return;
    mulib_input_keyboard_frame(&s->keyboard);
    mulib_input_mouse_frame(&s->mouse);
    mulib_input_touch_frame(&s->touch);
}

static inline void mulib_input_seat_cancel(mulib_input_seat_t *s)
{
    if (!s) return;
    mulib_input_keyboard_cancel(&s->keyboard);
    mulib_input_mouse_cancel(&s->mouse);
    mulib_input_touch_cancel(&s->touch);
}

/* SYN_DROPPED: cancel held input and ignore through SYN_REPORT AND until an
 * explicit snapshot is supplied. No false claim of automatic EVIOCG* recovery.
 * Supported raw types: SYN_REPORT/SYN_DROPPED, EV_KEY, REL_X/Y/WHEEL/HWHEEL.
 * Buttons outside BTN_MOUSE..BTN_TASK are unsupported, not keyboard keys.
 * Returns 1 handled, 0 invalid/unsupported/awaiting-resync; errno distinguishes. */
static inline int mulib_input_seat_evdev(mulib_input_seat_t *s, unsigned type,
                                        unsigned code, int value, int w, int h)
{
    if (!s || type > 31 || code > UINT16_MAX) { errno = EINVAL; return 0; }
    if (type == MULIB_INPUT_SYN) {
        if (value != 0) { errno = EINVAL; return 0; }
        if (code == 3) {
            mulib_input_seat_cancel(s);
            s->sync_lost = 1; s->resync_ready = 0;
            return 1;
        }
        if (code == 0) {
            if (s->sync_lost) s->resync_ready = 1;
            return 1;
        }
        errno = ENOTSUP;
        return 0;
    }
    if (s->sync_lost) { errno = EAGAIN; return 0; }
    if (type == MULIB_INPUT_KEY) {
        if (code >= 768 || value < 0 || value > 2) { errno = EINVAL; return 0; }
        if (code >= 272 && code <= 279)
            return mulib_input_mouse_button_checked(&s->mouse, code - 272, value);
        if (code >= 256) { errno = ENOTSUP; return 0; }
        if (value == 2 && !s->keyboard.down[code]) { errno = EINVAL; return 0; }
        if (code == 58 && value == 1 && !s->keyboard.down[58]) s->caps = !s->caps;
        return mulib_input_keyboard_update(&s->keyboard, code, value);
    }
    if (type == MULIB_INPUT_REL) {
        if (code == 0 || code == 1)
            return mulib_input_mouse_motion_checked(&s->mouse, code == 0 ? value : 0,
                                                     code == 1 ? value : 0, w, h, 1);
        if (code == 8) return mulib_input_mouse_scroll(&s->mouse, 0, value);
        if (code == 6) return mulib_input_mouse_scroll(&s->mouse, value, 0);
    }
    errno = ENOTSUP;
    return 0;
}

/* Valid only after the report following SYN_DROPPED. Caller obtains the true
 * snapshot using its own trusted backend. Snapshot is a baseline, not presses;
 * cancellation edges from the loss remain until the next frame. Touch is
 * cancelled (this raw adapter does not implement EV_ABS multitouch). */
static inline int mulib_input_seat_resync(mulib_input_seat_t *s,
                                          const unsigned char keys[768],
                                          unsigned buttons, int caps)
{
    if (!s || !keys || !s->sync_lost || !s->resync_ready
        || (caps != 0 && caps != 1) || (buttons & ~255u)) {
        errno = EINVAL;
        return 0;
    }
    for (size_t i = 0; i < 768; ++i) {
        if (keys[i] > 1 || (i >= 256 && keys[i])) { errno = EINVAL; return 0; }
    }
    memcpy(s->keyboard.down, keys, sizeof s->keyboard.down);
    s->keyboard.mods = mulib_input_keyboard_mods(&s->keyboard);
    s->mouse.buttons = buttons;
    s->caps = caps;
    s->sync_lost = s->resync_ready = 0;
    return 1;
}

/* Normalize one trusted record without reading/opening any descriptor.
 * 1: output event, 0: synchronization control (output unchanged), -1: failure.
 * Timestamp regressions clamp within this stream; clock domains aren't mixed.
 * SYN_DROPPED is a control result: inspect sync_lost and cancel GUI captures. */
static inline int mulib_input_seat_feed(mulib_input_seat_t *s,
                                        const mulib_input_event_t *input,
                                        int w, int h, mgx_event *out)
{
    mgx_event e = {0};
    double x, y;
    if (!s || !input || !out) { errno = EINVAL; return -1; }
    x = s->mouse.x; y = s->mouse.y;
    if (!mulib_input_seat_evdev(s, input->type, input->code, input->value, w, h))
        return -1;
    if (input->time_us > s->time_us) s->time_us = input->time_us;
    if (input->type == MULIB_INPUT_SYN) return 0;
    e.time_us = s->time_us; e.code = input->code; e.mods = s->keyboard.mods;
    e.down = input->value; e.x = s->mouse.x; e.y = s->mouse.y;
    if (input->type == MULIB_INPUT_KEY) {
        if (input->code >= 272) { e.type = MGX_EVENT_BUTTON; e.code -= 272; }
        else e.type = MGX_EVENT_KEY;
    } else if (input->code == 0 || input->code == 1) {
        e.type = MGX_EVENT_MOTION; e.dx = e.x - x; e.dy = e.y - y;
        e.down = 0;
    } else {
        e.type = MGX_EVENT_SCROLL; e.down = 0;
        e.dx = input->code == 6 ? input->value : 0;
        e.dy = input->code == 8 ? input->value : 0;
    }
    *out = e;
    return 1;
}
#endif
