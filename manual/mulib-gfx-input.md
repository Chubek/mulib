# Bounded graphics input and poll loop

These C11, header-only extensions provide local input state, an ordered event
FIFO, and a single-step POSIX `poll` dispatcher. They do **not** provide device
discovery, XKB, logind/seat ownership, or an application event loop. They never
open `/dev/input` or `/dev/dri`. Contexts are single-thread-owned; callers must
serialize access. Initialize owning queues/loops to zero and do not copy them.

## Events and loss

`mulib-glx-event.h` retains `mgx_event`, `v/n/cap/head`, and the original
push/pop/free APIs. `mulib_glx_event_t` aliases `mgx_event`.

- Zero-initialized queues default to 4096 pending events. Set a positive bound
  with `mulib_glx_event_set_limit`; reducing it below the pending count fails.
- Push returns 1 on success, 0 on failure. `ENOBUFS` means the limit was reached;
  `ENOMEM` means allocation failed; `EINVAL` means invalid input. Storage grows
  only up to the selected limit (an already allocated buffer need not shrink).
- On capacity/allocation failure, **reject the newest event**, preserve pending
  events, set sticky `overflowed`, and increment saturating `dropped`.
  Invalid input does not increment loss counters.
- Nothing is coalesced: press/release/repeat order is preserved, even if both
  edges occur between renders. `count()` returns pending count, not `n`.
- Pop returns 0 for empty or invalid arguments; output is unchanged. Invalid
  arguments set `errno = EINVAL`. Empty is not an error.
- `clear()` discards pending events without freeing storage or acknowledging
  loss. `acknowledge_loss()` resets the sticky counters only.

**Do not replay a discontinuous batch blindly.** On loss, discard the pending
batch, cancel GUI capture/held state, obtain any required trusted input snapshot,
then acknowledge loss before resuming. Otherwise a retained press followed by a
lost release can stick. Queue operations alone cannot reconcile widget state.

Event validation rejects nonfinite coordinates/deltas, invalid key/button
codes/values, invalid resize dimensions, and malformed text. Text must be NUL
terminated within 32 bytes and scalar-valid UTF-8, not necessarily normalized.
Key values are 0/1/2 (release/press/repeat); button/focus/touch values are 0/1.
Touch `code` denotes a slot 0..31. Resize dimensions use `x/y`.

Allocation-failure tests can override `MULIB_GLX_EVENT_REALLOC/FREE`. Matching
allocator hooks must be used in every translation unit sharing the owning object.

## Input state

Call each `*_frame()` **before** consuming that frame's events, never between
press and release. Keyboard, pointer, gamepad and touch retain both edge flags
through the frame, even when final held state is zero. Flags record “at least
one edge”; use the FIFO if individual edge count/order matters.

| Header | Implemented behavior |
|---|---|
| `mulib-input-keyboard.h` | 768 bounded key slots; press/release/repeat flags; shift/ctrl/alt/super derived from held modifiers; cancel releases held keys. Repeat without a prior press fails. |
| `mulib-input-keymap.h` | Fixed evdev US mapping; Shift/Caps XOR for letters; suppresses Ctrl/Alt/Super text. Not XKB or an IME. |
| `mulib-input-compose.h` | Limited Latin acute/grave/diaeresis/tilde pairs; unsupported pairs emit base plus combining mark. Space or repeated dead key emits a spacing accent. Returns 0, 1, 2, or `SIZE_MAX` on invalid scalar/state/arguments. No general normalization claim. |
| `mulib-input-mouse.h` | Checked finite motion and scroll; nonnegative speed; clamp pointer to supplied positive surface bounds; 32 button bits. Arithmetic overflow fails without partial state mutation. |
| `mulib-input-gamepad.h` | 16 clamped signed normalized axes with rescaled deadzone; 64 button bits and per-frame edges; cancel clears held buttons/axes. No device mapping or calibration. |
| `mulib-input-touch.h` | 32 bounded contacts; unique active IDs; finite surface coordinates; pressure clamped to [0,1]; `id == -1` ends a contact. Per-slot edges and begin serials detect replacements. No raw multitouch protocol decoder. |
| `mulib-input-gesture.h` | Two lowest active slots produce centroid translation, distance ratio and wrapped rotation. A changed contact identity/serial resets the baseline. Degenerate/nonfinite geometry never emits NaN/Inf deltas. |
| `mulib-input-tablet.h` | Checked integer axis range normalization, finite surface motion, clamped normalized tilt, proximity/pressure-derived tip state. No tool/button/eraser mapping. |

Checked state updates return 1/0 and leave state unchanged on failure.
Legacy void mouse/gamepad/tablet functions remain wrappers around checked APIs;
they cannot return errors, but invalid input sets `errno`. Cancelling input
preserves already recorded same-frame edges. The legacy tablet axis helper
returns 0 for an invalid range with `errno = EINVAL`; prefer the checked form to
distinguish a valid zero.

## Local stream adapter and `SYN_DROPPED`

`mulib-input-seat.h` is a **local single-source state aggregate**, not a Linux
seat service. It accepts classic EV_KEY keyboard codes 0..255,
BTN_MOUSE..BTN_TASK (272..279), REL_X/Y, REL_WHEEL/HWHEEL, SYN_REPORT and
SYN_DROPPED. Other records fail with `ENOTSUP`; malformed records fail with
`EINVAL`. In particular, gamepad/tablet button records are not misclassified as
keyboard input. Extended key codes and EV_ABS/multitouch require another adapter.
Keep independent physical devices in separate states to avoid releasing another
device's held modifiers.

`mulib_input_seat_evdev()` keeps the original numeric record API. The additive
`mulib_input_seat_feed(seat, raw_event, width, height, event_out)` accepts the
existing `mulib_input_event_t` without requiring an implementation switch:

- 1: normalized KEY/BUTTON/MOTION/SCROLL event produced.
- 0: synchronization control handled; output unchanged.
- -1: failure, unsupported input, or recovery pending; output unchanged.

Feed updates local state before returning its event; a later queue failure must
be handled by the loss procedure above. It does not emit text: pass key records
through the keymap/compose helpers explicitly. Event window is initially zero;
the application assigns its destination. Accepted timestamps are clamped
nondecreasing within this stream; source clock domains are **not** converted.

On SYN_DROPPED:

1. Cancel held keys/buttons/contacts, recording release flags.
2. Set `sync_lost`, ignore subsequent data (`EAGAIN`).
3. The following SYN_REPORT sets `resync_ready`, but data remains ignored.
4. Caller supplies a validated trusted keyboard/button/Caps snapshot using
   `mulib_input_seat_resync()`. It establishes a baseline without synthetic
   presses. Touch stays cancelled. Resync before the report fails.

The application must also cancel its GUI captures when `sync_lost` is observed.
There is no automatic EVIOCGKEY/LED/ABS query, and clearing `sync_lost` by hand is
not recovery. Partial raw records are not reconstructed by the existing
`mulib-input.h` reader; short records/EOF are errors.

## Poll dispatcher

`mulib-glx-eventloop.h` borrows descriptors and callback user pointers. Neither
remove nor free closes them. Defaults: 1024 watches, configurable positive
limit, unique nonnegative descriptors, and POLLIN/POLLOUT/POLLPRI interest bits.

- `add`/`remove` return 1/0, including when called by a callback.
- Each poll takes a bounded readiness snapshot. Removing a watch suppresses
  its pending callback. Removing and re-adding the same fd creates a new
  identity; old readiness is never delivered to the replacement. New watches
  start on the next poll, even if callback mutation reallocates watch storage.
- Recursive poll returns -1/`EBUSY`. Free inside a callback is rejected with
  `errno = EBUSY` and leaves the object alive. Free after dispatch returns.
- POLLHUP/POLLERR/POLLNVAL detach the watch **before** its final callback.
  Drain any buffered bytes there as appropriate. Re-add explicitly only when
  the caller has resolved the terminal condition. This avoids repeated EOF or
  invalid-descriptor wakeups when an application repeatedly calls poll.
- Callbacks must use nonblocking I/O and bounded work. Remove a watch before
  closing/reusing its descriptor or destroying its callback data.
- Poll returns the OS readiness count, not necessarily the number of callbacks
  (some watches may have been removed). Errors preserve `errno`.
- Timeout -1 waits indefinitely, 0 is an explicit nonblocking probe, and a
  positive value waits in milliseconds. Values below -1 fail. With no watches,
  positive timeouts still sleep; -1 needs an external signal to interrupt.
  EINTR is returned directly, not internally retried with an extended timeout.

There is no internal busy loop, timer scheduler, cross-thread wakeup, or merge
with the legacy task/timer loop. Applications may watch an explicitly supplied
pipe/socket wakeup source. Allocation hooks are
`MULIB_GLX_EVENTLOOP_MALLOC/REALLOC/FREE`; both snapshot allocation failures are
reported without callbacks or watch mutation.

`mulib-glx-unix.h` remains the existing backend umbrella. Its additive
`mulib_glx_unix_prepare_fd()` sets nonblocking and close-on-exec on an explicitly
supplied borrowed fd while preserving unrelated flags. This is not atomic
CLOEXEC acquisition; serialize with fork/exec or acquire atomically yourself.
If its second flag update fails, CLOEXEC may already be set.

## Headless example

```c
#include "mulib-input-seat.h"
#include <assert.h>

int main(void)
{
    mulib_input_seat_t seat;
    mulib_glx_event_queue_t events = {0};
    mulib_input_event_t press = {1, MULIB_INPUT_KEY, 30, 1};
    mulib_input_event_t release = {2, MULIB_INPUT_KEY, 30, 0};
    mgx_event event;
    mulib_input_seat_init(&seat);
    mulib_input_seat_frame(&seat);
    assert(mulib_input_seat_feed(&seat, &press, 320, 240, &event) == 1);
    assert(mulib_glx_event_push(&events, event));
    assert(mulib_input_seat_feed(&seat, &release, 320, 240, &event) == 1);
    assert(mulib_glx_event_push(&events, event));
    assert(seat.keyboard.pressed[30] && seat.keyboard.released[30]);
    assert(mulib_glx_event_pop(&events, &event) && event.down == 1);
    assert(mulib_glx_event_pop(&events, &event) && event.down == 0);
    mulib_glx_event_free(&events);
    return 0;
}
```

Compile with `cc -std=c11 -Iinclude example.c -lm -o example`.

## Regression tests

From the repository root:

```sh
cc -std=c11 -pedantic -Wall -Wextra -Werror -Iinclude \
  tests/test-gfx-input.c tests/gfx-input-tu.c tests/gfx-input-unix.c \
  tests/gfx-input-evdev.c -lm -ldl -o /tmp/opencode/test-gfx-input
/tmp/opencode/test-gfx-input

cc -std=c11 -pedantic -Wall -Wextra -Werror -Iinclude \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  tests/test-gfx-input.c tests/gfx-input-tu.c tests/gfx-input-unix.c \
  tests/gfx-input-evdev.c -lm -ldl -o /tmp/opencode/test-gfx-input-san
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1 /tmp/opencode/test-gfx-input-san
```

Coverage includes queue limits/compaction/order, a 20,000-operation reference
FIFO, allocator failures and saturating loss counters, scalar/UTF-8 and
nonfinite boundaries, same-frame input edges, snapshot recovery after
SYN_DROPPED, contact replacement and degenerate gestures, callback mutation and
reentrancy, terminal descriptor detachment, blocking empty-loop timeouts, EINTR,
borrowed fd flags, and independent C translation units. Linux tests feed native
evdev records through a pipe into the existing reader, checking EAGAIN, malformed
records, short reads and EOF. All descriptors are pipes/socketpairs; no hardware
or live session protocol is exercised.
