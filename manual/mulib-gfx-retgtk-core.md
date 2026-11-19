# RetGTK core lifetime and event safety

These C11 single-header APIs cover local retained-widget ownership, synchronous
signals, properties, event routing, named type descriptors and numeric handles.
They are not full GTK parity, a Mune binding implementation, a concurrent object
system, a modal manager or an accessibility backend.

## Ownership and callbacks

Initialize objects, signals, properties, bindings, routers, weak links and
registries into zero/empty storage. Keep them on the UI thread. Do not copy or
move owning structs or linked weak refs, bindings and routers. A widget returned
by a constructor has one caller reference. `widget_add(parent, child)` adds a
tree reference; it does not consume the caller reference. `widget_remove`
releases tree ownership and immediately cancels all registered capture, focus
and hover routes in the removed subtree.

`mulib_retgtk_object_ref` cannot resurrect an object during finalization.
`mulib_retgtk_weak_set`, `weak_lock`, and `weak_clear` provide non-owning links.
`weak_lock` returns an owned reference or NULL. Weak links become NULL before
the finalizer runs. Clear a live weak link before releasing its storage.

Widget signals bind their owner automatically. `signal_emit` pins that owner
through the complete emission, including nested emissions. Generic embedded
signals can use `mulib_retgtk_signal_bind_owner(signal, containing_object)`;
do not bind an unrelated object. A standalone signal's storage, callback user
data and unowned sender must otherwise outlive all active emissions.

Signal rules:

* Disconnecting a not-yet-called slot suppresses it in the current emission.
* Connecting during emission does not extend that emission's slot list. A nested
  emission sees new slots. Reallocation does not invalidate the outer iterator.
* `signal_free` during emission cancels remaining slots and defers storage
  release until the outermost emission exits. Resetting subscriptions retains
  a containing-object owner binding; explicitly bind NULL to remove it.
* `signal_connect_object` weakly tracks a receiver and pins it for each callback.
  Dead receivers are skipped and removed during compaction. Ordinary `connect`
  leaves callback user-data lifetime with the caller.
* Nesting beyond 32 emissions is ignored to bound recursive stack growth.

The measure, arrange and paint wrappers also pin their widget. Default child
arrangement and painting snapshot child references, skip removed/reparented
children, and restore the paint clip. Custom layout/model callbacks should use
`mulib_retgtk_widget_children_snapshot(w, &n)` and
`mulib_retgtk_widget_children_release(v, n)` when recursively invoking user code.
Check `v[i]->parent == w` before visiting a snapshot child. A failed snapshot
allocation returns NULL and n=0; void paint/arrange APIs skip that descent.
The built-in painter and default/linear/grid/constraint/scroll arrangement
paths keep a repaint/layout retry pending rather than losing the skipped work.
Custom void layout callbacks can use `mulib_retgtk_widget_layout_retry(w)` on
scratch-allocation failure.
The snapshot is independently owned, not a replacement for `w->children`.

Finalization first detaches the child array, cancels descendant routes and
disconnects signals, then invokes `ops->destroy`. The destructor sees an empty
child array and cannot add children to the finalizing object. It must not free
the widget allocation itself. Custom `data` ownership remains the destructor's
responsibility.

## Event router

A zero-initialized `mulib_retgtk_event_router_t` owns its capture, focus and hover
references and registers intrusive detach links in the widgets. Use
`mulib_retgtk_event_router_free` before releasing router storage, including when
all its slots appear NULL. The router storage must outlive in-flight dispatch;
calling router_free inside dispatch is supported, freeing the allocation is not.
Use one active router per tree: widget visual flags are not multi-router counts.
Do not write router slots directly. Legacy `mret_router_set` retains its owning
slot behavior, but it cannot register removal notifications; dispatch/validate
adopts those legacy slots.

`mulib_retgtk_event_dispatch`:

* excludes hidden/disabled nodes **and their descendants**, including focused
  key/text destinations;
* pins the root and original bubble path and cancels obsolete-path delivery if
  callbacks detach or reparent that path;
* captures the initial button target for subsequent motion/button events even
  outside its bounds; a different button release cannot cancel that capture;
* focuses the nearest focusable ancestor of a button target;
* sends key/text events to focus, not pointer capture;
* cancels capture, focus and hover on a native focus-lost event;
* handles evdev Tab (15), Shift+Tab (`mods & 1`) in depth-first order, wrapping
  and skipping hidden/disabled subtrees.

Use `mulib_retgtk_event_focus`, `event_focus_next`, `event_capture`, and
`event_release_capture` for explicit routing changes. Explicit capture is not
released by button-up: release it explicitly or remove/disable its subtree.
Scroll uses hit testing, not pointer capture. The primary-control click contract
still requires release inside the control; capture does not imply a click.
Capture routes received events locally; it does not install a platform input grab.

Use `widget_set_visible` and `widget_set_enabled` for immediate route cancellation.
Legacy direct member writes are supported, but cancellation then occurs on
`event_router_validate` or the next dispatch. Reattaching a subtree never restores
old focus/capture. Destroying a parent also invalidates descendant routes.
There is no native multi-pointer capture or modal event scope in this patch.

## Properties and bindings

`mulib_retgtk_property_set` copies string inputs, borrows pointer inputs,
normalizes booleans, and emits signal 1 only on a value change. Equal string
contents, normalized booleans and repeated NaNs compare equal. Success includes
unchanged values. `mulib_retgtk_value_copy`, `value_equal` and `value_clear` expose
the same ownership rules for standalone values; copy's destination must be empty.

Each notification receives an independent value snapshot valid through that
emission. A recursive set/free cannot invalidate an earlier callback's string or
change the payload later listeners receive. Recursive notification order is
synchronous (a nested notification completes before the outer one resumes).
`property_free` is idempotent, cancels remaining notifications, and marks the
property disposed. Do not reuse it without reinitializing empty storage after
all in-flight calls finish. As with standalone signals, property storage must
outlive active setters; an embedded property's signal can bind its containing
reference-counted object to pin that storage.

Bindings are caller-owned, zero-initialized and immovable. They watch endpoint
signal lifetime, so `property_free(endpoint)` may precede freeing that endpoint's
storage and later `binding_free`. Bindings stop propagating when either endpoint
is disposed. Always call `binding_free` before releasing binding storage.
Freeing/reinitializing a binding during propagation is supported if its storage
remains alive until that callback returns. Property equality suppresses redundant
binding notifications; a per-binding reentrancy guard breaks immediate two-way
feedback. This is not a general dependency/cycle solver.

## Stable named types and handles

`mulib_retgtk_type_equal` compares immutable qualified names, instance sizes and
ancestry (bounded to 128 levels), **not descriptor/operation-table addresses**.
`type_is` follows the same identity across translation units.
Caller-owned `mulib_retgtk_type_registry_t` returns a canonical descriptor and
rejects conflicting definitions with the same name. Descriptors and their names
must outlive the registry and typed instances. Independent modules must agree on
unique qualified names and semantics; this is not dynamic ABI verification.

`widget_set_type` / `widget_is` opt widgets into named identity (descriptor size
must equal `sizeof(mulib_retgtk_widget_t)`). Existing constructors and arbitrary
custom ops are not automatically assigned a new semantic type. Legacy code
outside these core headers that still compares `w->ops == &some_ops` remains
TU-local and must migrate explicitly; operation table addresses are not made
globally canonical by this mechanism.

`mulib_retgtk_mune_get` is borrowed. Native glue should use `mune_acquire` and
`widget_unref` across callbacks that can drop handles. `mune_drop` removes a
handle before unref/finalization, making table reentrancy safe. Table free blocks
new additions while releasing references and preserves the monotonically
increasing handle sequence, preventing stale numeric handles from aliasing reuse.
Handles remain below the exact Lua-number integer bound. This table alone does
not bind actual Mune functions.

The existing arithmetic reactive graph remains a small backwards-reference DAG,
not an observable typed store. It rejects nonfinite inputs/invalid dependency
indices and treats repeated NaN arithmetic results as unchanged.

## Retained damage and persistent targets

`mulib_retgtk_scene_repaint(scene, painter, bounds)` uses the existing widget
tree and CPU painter on a **persistent, otherwise unmodified** `mgx_surface`.
Return values are `1` repainted, `0` idle/no visible damage, `-1` invalid target
or recursive scene drawing. Initialize a scene to zero, use `scene_set` to
retain its root, and release that root with `scene_set(scene, NULL)`.

```c
#include "mulib-retgtk-scene.h"
#include "mulib-retgtk-button.h"

/* surface is initialized and has its full clip; the host owns its lifetime. */
mulib_retgtk_scene_t scene = {0};
mulib_retgtk_widget_t *button = mulib_retgtk_button(1, "Apply");
if (button) {
    mulib_retgtk_scene_set(&scene, button);
    mret_paint paint = {&surface, font, mgui_default_theme()};
    mulib_retgtk_scene_repaint(&scene, &paint, (mgx_rect){0,0,surface.w,surface.h});
    /* This mutation records damage; the next repaint updates just that area. */
    mulib_retgtk_widget_text(button, "Applied");
    mulib_retgtk_scene_repaint(&scene, &paint, (mgx_rect){0,0,surface.w,surface.h});
    mulib_retgtk_widget_unref(button);
}
mulib_retgtk_scene_set(&scene, NULL);
```

Invalidation records one conservative, ancestor-clipped screen-space bounding
rectangle at the root without allocating. Ancestors retain their ordinary
`dirty` hints without automatically expanding every invalidation to the root
rectangle. Moving/shrinking through `widget_arrange` records old and new bounds;
visibility changes and removal erase old pixels. Coordinate union overflow
falls back to full-target damage. `invalidate_collect` observes root damage
without consuming it and preserves pending state on region-allocation failure.

Repaint source-clears the damaged background, then paints **all intersecting
layers in normal order**, including clean overlapping/translucent widgets.
It is not correct to paint only dirty widgets onto a previously composed image.
Nonintersecting branches do not invoke paint callbacks. Pending damage is
consumed before callbacks, so paint-time invalidation/removal queues another
frame. Root references remain pinned if a callback replaces the scene; the new
root is painted on the next call. Re-entering either scene draw path is rejected.
Scene/painter/target storage must remain alive and immovable through callbacks.

First use, root replacement/removal, target/buffer/size changes, bounds changes,
and font/theme value changes repaint the complete target. Call
`mulib_retgtk_scene_invalidate()` after external target clearing/loss or
in-place font/image/theme-asset mutations that retain the same pointer.
The scene root must be unparented, and the target must have its **full surface clip**;
parented roots and partial external clips are
rejected to avoid silently consuming damage outside that clip.
Use one incremental scene owner per widget root/target.

`mulib_retgtk_scene_draw` remains the unconditional compatibility path; its
caller still owns clearing the background. Mixing it with incremental repaint
requires a full baseline on the next repaint. Direct writes to widget fields
must be followed by `widget_invalidate(w)`; merely writing `dirty` on a
descendant is not a notification API. Model mutations also still require
explicit invalidation of their views.

Clean incremental frames skip arrangement as well as painting. Dirty frames
still arrange the whole root; **per-widget measure caches, per-subtree layout,
general model subscriptions and variable-height collection indexing remain
unfinished**. Damage uses a bounding rectangle, not a canonical sparse region.
Callbacks may therefore repaint more than the exact changed pixels.

`mulib_retgtk_window_draw` keeps one additional CPU image and copies that image
to each acquired native swapchain buffer. This prevents alternating/async
buffers from showing stale pixels without assuming backend buffer-age support.
The extra image uses the existing surface allocator and follows window size.
Failed allocation/presentation cancels acquisition so a subsequent draw can
retry; resizing preserves the previous retained image if replacement allocation
fails. Presentation still copies a complete image and is not a GPU/bandwidth
optimization. Complete outstanding native presentations before window teardown,
as required by the underlying GLX window.

Retained scrolling now uses snapshot-safe arrangement. `scroll_to(w, offset)`
clamps finite offsets and immediately updates child geometry for hit testing;
it returns zero for invalid/no-change requests. `scroll_content(w, height)`
updates nonnegative content height and clamps/arranges again. Wheel input ignores
nonfinite deltas and disabled/hidden ancestry, and bubbles when already clamped
at a boundary. These APIs use stable `MRET_KIND_SCROLL` identity, not ops addresses.
Content remains one overlaid child area; this is not variable-height row
virtualization or a native input grab.

## Validation

Run the three translation units together:

```sh
cc -std=c11 -Wall -Wextra -Werror -Wpedantic -Iinclude \
  tests/test-gfx-retgtk-core.c tests/gfx-retgtk-core-types-a.c \
  tests/gfx-retgtk-core-types-b.c -lm -o /tmp/opencode/test-gfx-retgtk-core
/tmp/opencode/test-gfx-retgtk-core

cc -std=c11 -Wall -Wextra -Werror -Wpedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Iinclude \
  tests/test-gfx-retgtk-core.c tests/gfx-retgtk-core-types-a.c \
  tests/gfx-retgtk-core-types-b.c -lm -o /tmp/opencode/test-gfx-retgtk-core-sanitize
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /tmp/opencode/test-gfx-retgtk-core-sanitize
```

Tests exercise callback connection/disconnection/reallocation and nested emission,
owner/receiver destruction, free-during-emission, string ownership and notification
snapshots, reentrant/two-way binding teardown, endpoint destruction, capture outside
bounds, matching-button release, focus traversal, hidden/disabled ancestors,
subtree removal and parent destruction, event/click/paint/measure callback mutation,
separate-TU type identity/collision rejection, DAG validation and pinned numeric
handles. These headless tests do not validate compositor input, actual Mune glue,
all widget-specific callbacks, thread safety or complete toolkit parity.

The retained follow-up has no extra translation-unit peer:

```sh
cc -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/test-gfx-retgtk-followup.c -lm -ldl -o /tmp/opencode/retgtk-followup
/tmp/opencode/retgtk-followup
clang -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  tests/test-gfx-retgtk-followup.c -lm -ldl -o /tmp/opencode/retgtk-followup-clang
/tmp/opencode/retgtk-followup-clang
cc -std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Wpedantic -Werror -Iinclude \
  -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all \
  tests/test-gfx-retgtk-followup.c -lm -ldl -o /tmp/opencode/retgtk-followup-asan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /tmp/opencode/retgtk-followup-asan
lua tests/test-gfx-retgtk-lua.lua
make check-graphics
```

`test-gfx-retgtk-followup.c` checks idle pruning, narrow damage, offscreen pruning,
source-clear with translucent themes/overlap, 500 deterministic randomized
incremental-vs-full image comparisons, move/hide/remove cleanup, callback
invalidation/root replacement and reentrancy, snapshot/layout allocation
failures and retries, arrange-callback append scheduling, scroll mutation/clamping, resize/present allocation
failures, alternating swapchain images and simulated async completion. It uses
no devices, real display connection, toolkit or desktop service.
