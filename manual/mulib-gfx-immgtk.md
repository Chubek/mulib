# Native ImmGTK overlay

The C11 `mulib-immgtk-native.h` overlay is additive. It does not replace
`mulib-immgtk.h`, the legacy Mune host, or its Lua widget modules. Native controls
and retained controls use the same GLX software surface and draw-command renderer.
This is a bounded baseline, not Dear ImGui/ImPlot parity.

## Lifetime and limits

Call `mulib_immgtk_native_init`, then pair each begin with end, and finally call
`mulib_immgtk_native_free`. Do not copy a context, use it concurrently, initialize
over live allocations, or retain `mig_state_get` pointers across another state
insertion or frame end. The context borrows the surface during the frame and
borrows its font; it never frees either. End renders commands and drops the
surface reference. Labels currently flush preceding commands and draw immediately,
preserving paint order without retaining a string/font pointer in a command.

Before allocations, `mulib_immgtk_native_limits(g, items, states, actions, draws)`
can override defaults: 65,536 items per frame, 65,536 state entries, 4,096 input
events/actions per frame, and 65,536 simultaneously queued drawing commands.
These also bound native array capacities. Two item arrays are retained for
current/previous geometry. State is collected after `state_ttl` frames without
access (default 600); capacity is retained until free. Limits are not a CPU-time
budget: item, state and duplicate-ID lookups are still linear.

Check `g->error` after building/end. Its bit flags distinguish invalid lifetime,
limits, allocation failure, duplicate/invalid IDs, unbalanced scopes and input
errors. The legacy void begin/end signatures remain; a second begin does not
discard an open frame. Failed allocations preserve owning pointers and counts.
No whole-frame transaction is promised: drawing or a widget return preceding
an error cannot be rolled back.

## Input and first-frame behavior

`mulib_immgtk_native_begin` preserves the snapshot API. Prefer
`mulib_immgtk_native_begin_events`, or feed events to a zero-initialized
`mulib_immgtk_io_t` with `mulib_immgtk_io_feed` and call `mulib_immgtk_io_begin`.
After building/end, call `mulib_immgtk_io_reset`; finally call
`mulib_immgtk_io_free`. Do not replay an unreset queue.

Input is routed **only against the previous completed frame's geometry**.
The first frame registers geometry but cannot receive a pointer click or
keyboard focus. There is no guessed current-frame hit target. Press and release
inside one batch are preserved, as are motion between transitions and clicks
on different controls. A widget's boolean return coalesces repeated successful
clicks on that same widget within one frame.
`g->last_action` records the successful action most recently returned by
`mig_interact`; sliders use its release coordinates for an entire click delivered
in one batch, even if a later motion leaves the control.
Draggable window handles also retain captured gesture endpoints in that bounded
action queue. Those records are not clicks and are never returned by
`mig_interact`. Release outside the original handle completes a drag, while
later motion after release cannot change its endpoint. Escape, focus loss,
modal-scope changes, malformed input and action allocation failure cancel pending
drag delivery.

Button codes are normalized (primary is 0), key codes use Linux evdev numbers,
and modifier bit 0 is Shift. Feed MOTION before BUTTON; button coordinates come
from the last motion, not the optional event `x/y` fields. Nonprimary buttons and
unhandled application events do not interact with native controls. Touch/multiple
pointers are not implemented. Focus loss cancels capture, focus and pending
clicks, and suppresses input until focus returns. Queue overflow, malformed
supported input or allocation failure fails closed rather than delivering half
a gesture. IO defaults to 4,096 events and 65,536 text bytes; set `max_events` and
`max_text` before first use to change these.

A release activates only its captured previous-frame target, and both press
and release coordinates must still lie in the widget's **current** bounds and
effective clip. Disabled, removed, fully clipped or changed-scope widgets cannot
retain focus/capture. Changed root-surface clips also constrain old hits.
If structural layout/z-order changes make the old routing map inappropriate,
call `mulib_immgtk_native_invalidate` before dispatch and collect a new frame.
New occluders cannot retroactively reroute already-dispatched events.

The IO text buffer and text-edit helper still use per-frame snapshots, not a
full per-event text/IME routing engine. Avoid changing text focus in a batch
containing text intended for a different field.

## IDs, clipping and navigation

`mig_push_id`/`mig_pop_id` delimit hierarchical string components (64 levels).
For example, `["ab","c"]` and `["a","bc"]` differ. Zero/null IDs are invalid;
duplicate numeric IDs in one frame are reported, never silently registered
twice. Hashes remain 64-bit and are not a cryptographic collision-free registry.
Applications must not deliberately reuse an ID for different live widgets.
Do not persist the numerical hash as a stable on-disk ABI.

`mulib_immgtk_clip_push` intersects with the enclosing clip and
`mulib_immgtk_clip_pop` restores it. Both drawing and interaction obey nested
half-open rectangles. Clip and ID scopes must be balanced before end.

Tab/Shift-Tab traverse enabled, nonempty-clipped previous-frame items in registration
order, wrapping within the active popup/modal scope. Enter and Space activate
the focused item. `mulib_immgtk_nav_step` is the equivalent explicit navigation
API. Navigation does not compute pixel occlusion by other windows. There is no
spatial/gamepad navigation or focus-tab-index API yet.

`mulib_immgtk_window_begin/end` register passive background occlusion, so an empty
later equal-z window blocks hits to earlier content without adding a keyboard
tab stop. Title dragging uses the original press position, including an entire
press/move/release batch or a capture held across frames after the title moves
away from the press point. A host-moved handle cannot accept a stale new press.
The original begin/end signatures retain their move-only behavior.

### Resizable/collapsible windows

Use the additive `mulib_immgtk_window_begin_ex` for bottom-right resizing and a
collapse control:

```c
/* Retain these in the host, not in a per-frame temporary. */
mgx_rect bounds = {20, 20, 320, 240};
int collapsed = 0;
/* ... inside a native frame ... */
mulib_immgtk_window_scope_t window;
if (mulib_immgtk_window_begin_ex(g, "Inspector", &bounds,
        MIG_WINDOW_RESIZABLE | MIG_WINDOW_COLLAPSIBLE, &collapsed,
        120, 80, &window)) {
    if (window.content.h > 0) {
        /* Submit content using window.content; both hits and drawing are clipped. */
    }
    mulib_immgtk_window_end(g, &window);
}
```

Minimum dimensions must both be at least 40, and initial bounds must meet them.
The names `#background`, `#title`, `#collapse` and `#resize` are reserved within
the window's ID scope; use different content IDs.
Resizing clamps to those minima and representable geometry without changing the
origin. `MIG_WINDOW_NO_MOVE` disables title dragging; changing to it cancels an
existing title capture. The 12-pixel resize grip is painted/hit-tested after its
own content, but remains below a later equal-z window. Item-array growth by
content does not invalidate grip bookkeeping.

The collapse control supports pointer activation and the normal Tab/Enter/Space
navigation. Collapsing retains the expanded bounds, draws/hits only the
24-pixel title bar and sets `window.content.h` to zero. A successful begin
**always** needs end, including collapsed windows. Prefer skipping collapsed
content rather than executing arbitrary application-side widget code.

Invalid geometry/options, duplicate window IDs and window setup allocation
failure are rejected before consuming actions, changing bounds/collapse,
registering items or drawing. Allocation capacity may grow on failure; an entire
frame is still not transactional. Windows are not automatically raised/reordered
or persisted. The host must submit them in painter order, and should invalidate
previous routing before external structural/z-order changes as described above.

## Popups and modals

`mulib_immgtk_popup_t` retains its existing open/is-open/close helpers.
For actual input blocking:

1. Call `mulib_immgtk_popup_prepare(g, popup)` **before input dispatch** and after
   any open/close operation. The top popup ID blocks every other scope across
   the entire surface, including outside its bounds.
2. Build its widgets between `mulib_immgtk_popup_begin/end` with that same ID.
   Background widgets can still be drawn/registered but cannot activate or
   receive Tab focus. Newly opened popup geometry has the same one-frame
   registration latency.
3. Optionally call `mulib_immgtk_popup_dismiss` after input and before widgets to
   close on outside press/Escape. Omit it for a non-dismissible modal.

Opening/closing cancels queued activation, preventing click-through. Closing
the last popup attempts to restore pre-popup focus only if that item remains
eligible in the routing map. Keep the same stack/context pair and do not copy
an owning context. Popup placement, native windows and platform modality are
not implemented.

## Legacy draw-list adapter

Include `mulib-immgtk-drawlist-bridge.h` (C11), or opt in with
`MULIB_GFX_ENABLE_IMMGTK_DRAWLIST_ADAPTER` before including the legacy drawlist.
`mulib_immgtk_drawlist_append(old, shared, clip)` validates then appends ordered
canonical commands without altering legacy storage or requiring a second
renderer. Rectangle coordinates are opposite half-open corners, rounded
outward; circles use `(x1,y1)` as center and `width` as radius; lines retain both
endpoints and stroke width. Colors pass unchanged as straight `0xAARRGGBB`.
Failure leaves the canonical command count unchanged. Legacy functions,
members and declaration-only C/C++ inclusion remain available without opting in.

## Optional Mune bridge

Define `MULIB_GFX_ENABLE_MUNE`, include `mulib-immgtk-mune-bridge.h`, and compile
the real Mune dependency implementations as described by Mune's manual.
Create a new VM and bind the bridge, not a VM already owned by a legacy ImmGTK
host. On bind failure discard the partially bound VM. Destroy the VM before
freeing its bridge/context.

`mulib_immgtk_mune_bridge_run` wraps `mulib_mune_run_source` and restores bridge
clip scopes on success and runtime error. Direct Mune execution still requires
an explicit `mulib_immgtk_mune_bridge_reset` after every script. Neither helper
rolls back earlier script drawing or Lua side effects. Numeric interaction IDs
remain exact integers in `[1, 2^53-1]` and must not collide with native IDs.
The bridge exposes the four original primitive names plus line and clip hooks;
it is not a complete native widget, keyboard, clipboard or IME binding.

## Reproducible validation

From the repository root (executables may instead be placed in `build/`):

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/test-gfx-immgtk.c -lm -ldl -o /tmp/opencode/test-gfx-immgtk
/tmp/opencode/test-gfx-immgtk
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/test-gfx-immgtk-followup.c -lm -ldl -o /tmp/opencode/test-gfx-immgtk-followup
/tmp/opencode/test-gfx-immgtk-followup
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/gfx-immgtk-alloc.c -lm -o /tmp/opencode/gfx-immgtk-alloc
/tmp/opencode/gfx-immgtk-alloc
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/gfx-immgtk-link.c tests/gfx-immgtk-link-peer.c -lm -o /tmp/opencode/gfx-immgtk-link
/tmp/opencode/gfx-immgtk-link
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/gfx-immgtk-mune.c -pthread -lm -ldl -o /tmp/opencode/gfx-immgtk-mune
/tmp/opencode/gfx-immgtk-mune
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/gfx-immgtk-legacy-cxx.cpp -lm -o /tmp/opencode/gfx-immgtk-legacy-cxx
/tmp/opencode/gfx-immgtk-legacy-cxx
```

Add `-fsanitize=address,undefined -fno-omit-frame-pointer -g` for sanitizer runs.
The Mune executable exercises actual repository callbacks, drawing, interaction
and clip cleanup after script errors; it is not exhaustive Mune compatibility
certification. Existing `test_immgtk.c` (including `immgtk-widgets.lua`),
`test_immgtk_drawlist.c`, `test_immgtk_canvas.c` and `test_immgtk_plot.c` remain
separate regression gates. No physical devices or display-server integration
are exercised by these headless tests.
The follow-up window suite checks complete and multi-frame drags, resize minima,
release-time coordinates, collapse keyboard focus, real clipped pixels, modal
blocking, empty-window occlusion, stale geometry, input cancellation and injected
setup/action allocation failures.
