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
