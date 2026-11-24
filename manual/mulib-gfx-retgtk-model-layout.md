# RetGTK model, layout, and document subset

This is a bounded C11/header-only overlay, not GTK parity. It shares the existing
widget tree and CPU painter; none of these tests accesses DRM or input devices.

## Measure and arrange

`mulib_retgtk_box()` and `mulib_retgtk_flex()` now measure visible children,
including nested containers, before allocating their main axis. Their intrinsic
size is the sum of child sizes plus inter-child gaps and outer padding; the
cross axis is the largest child. Parent constraints clamp the reported size.
Children without a measure callback use their existing `desired` size. There
is **no new font-dependent intrinsic measurement** in this change.

Arrangement starts with each child's measured size, not an equal-sized slot:

- Positive `weight` divides surplus space; capped children redistribute surplus.
- Zero/nonfinite weights do not grow, but retain their intrinsic size.
- Shortage shrinks in proportion to the space above each child's minimum.
- Minima take precedence over fitting an undersized parent. Overflow is clipped
  by the ordinary ancestor painting/hit-test bounds.
- Hidden children consume neither space nor gaps, and are not measured/arranged.
- Integer rounding preserves the available pixel count when growth/shrink can
  fill it. Coordinates and measured totals saturate instead of overflowing.

Use an explicit size-limiting container for per-child limits:

```c
mulib_retgtk_widget_t *row = mulib_retgtk_box(1, 0, 4, 8);
mulib_retgtk_widget_t *fixed = mulib_retgtk_constraint_box(
    2, (mulib_gui_constraints_t){120, 24, 120, 48});
mulib_retgtk_widget_t *button = mulib_retgtk_button(3, "Apply");
/* Check all three allocations before using them. */
if (row && fixed && button &&
    mulib_retgtk_widget_add(fixed, button) &&
    mulib_retgtk_widget_add(row, fixed)) {
    mulib_retgtk_widget_arrange(row, (mgx_rect){0, 0, 320, 80});
}
/* add() retains, it does not consume the caller's reference. */
mulib_retgtk_widget_unref(button);
mulib_retgtk_widget_unref(fixed);
mulib_retgtk_widget_unref(row);
```

Include `mulib-retgtk-box.h` and `mulib-retgtk-button.h` for this example.
Constraint maxima are explicit: use `INT_MAX` for an unbounded dimension.
All-zero limits mean a fixed zero-sized container, not “unbounded.” Its children
overlay one another. Do not change a constraint container's `kind` or `data`.
Linear layout uses temporary scratch allocation; failure leaves child bounds
unchanged because the existing arrange API cannot report an error.
Callback-capable box/grid/constraint traversals use the core's retained child
snapshots. Removed/reparented children are skipped safely; newly appended
children participate on the next traversal. Use the visibility/enabled setters
when changing live widgets so the core immediately invalidates router state.

Grid tracks remain uniform, with per-cell padding. Grid measurement uses the
largest visible child's intrinsic size. Hidden children no longer leave empty
tracks; configured column count is retained. Grid spans, intrinsic track sizing,
baseline alignment, wrapping flex, per-widget measurement caching and
incremental per-subtree layout remain unfinished. Incremental scene repaint
now skips layout on clean frames and retries built-in layout scratch failures;
see [retained damage and persistent targets](mulib-gfx-retgtk-core.md#retained-damage-and-persistent-targets).
The new limits are integrated with linear allocation; other
container policies are not upgraded to a general constraint solver.

## Models, proxies, and views

`mulib_retgtk_proxymodel_build()` filters and performs stable bottom-up merge
sorting. Equal string keys retain **source order in either direction**. The
build costs O(rows + matches log matches) comparisons/work and O(rows) auxiliary
storage. Source cell strings are borrowed and must remain stable throughout the
build; null cells are filtered out. A failed build leaves the previous snapshot
intact.

`mulib_retgtk_proxymodel_source_row()` returns `SIZE_MAX` for an invalid or stale
row. `mulib_retgtk_proxymodel_cell()` returns `NULL`. The read-only
`mulib_retgtk_proxymodel_interface()` can be passed to list/table views. A stale
interface exposes zero rows and columns.

The source model **and its interface object** must outlive the proxy. A snapshot
captures the interface revision and source row/column counts. Mutate using
`mulib_retgtk_model_set()` on that same interface, or increment its revision
after external modifications, then rebuild. List-model append/remove operations
change the underlying container, not every previously returned interface copy.
Shape checks detect count changes, but cannot detect a remove/append restoring
the same count, or a reorder with unchanged count. Those edits require an
explicit revision bump. The same rule applies to composing/rebuilding proxies.
There are no automatic mutation subscriptions or durable row IDs yet.

`mulib_retgtk_model_set()` validates indices independently of whether a cell
currently contains text, rejects null input, and increments the revision only
after successful mutation. List models now support replacement through that
interface and checked removal. Table initialization requires an empty,
zero-initialized object and rejects reinitialization rather than leaking it.

List/table views clamp the first row and clear out-of-range selection after
source shrinkage. Painting includes the clipped partial final row and does not
divide by invalid row heights. Selection validates the primary pointer button;
disabled/hidden views reject direct events. Nonfinite scroll deltas are ignored.
Table painting is bounded by visible pixel columns, omitting zero-width uniform
columns rather than walking an unbounded model column count. Tree ancestor
walks reject corrupt/forward/cyclic parent indices.

## Documents, builder, and reload ownership

UIML remains an XML-like subset: nested named tags, decimal positive `id`, quoted
literal `text`, and self-closing tags. It rejects entities, text nodes, duplicate
attributes/IDs, missing attribute whitespace, overflowed IDs, and malformed
nesting. Missing IDs use the one-based node ordinal; collisions with explicit
IDs are rejected. Limits default to 1 MiB of source, 100,000 nodes and 128 levels:

- `MULIB_RETGTK_UIML_MAX_BYTES` (inspected byte budget including the required
  NUL terminator; an unterminated buffer of exactly this size is safely rejected)
- `MULIB_RETGTK_BUILDER_MAX_NODES`
- `MULIB_RETGTK_BUILDER_MAX_DEPTH`

A parse destination must be zero-initialized or hold a previous parse. Failure
leaves it unchanged. Successful replacement releases the previous document.
ID uniqueness is checked with sorting, not a quadratic scan.

The builder validates topology, unique IDs, registered names, duplicate factory
registrations and depth **before calling a factory**. A factory returns one
owned reference to a fresh, unparented widget with the requested ID. Factories
must not return aliases or mutate an existing tree. On failure the builder
releases all temporary and tree-owned references. On success the returned root
has one caller-owned reference; descendants are parent-owned. External factory
side effects cannot be rolled back.

`mulib_retgtk_hotreload()` builds and transfers state into a separate candidate
tree before replacing the scene. Parse, factory, or entry-state allocation
failure leaves both the old scene root and its generation unchanged.

State matching uses widget IDs and explicit, nonzero stable kinds from
`mulib-retgtk-kind.h`, **not translation-unit-local ops addresses**. Typed
buttons, toggles, labels, entries, lists/tables and constraint containers have
stable kinds. Legacy orientation/column values are not type identities;
untyped widgets keep the new document's state. Custom factories can reserve
their own kinds at or above `MRET_KIND_USER`.

Matching typed widgets transfer `value` and visibility. Entries also clone
editor text and current selection into independent storage, starting a fresh
undo baseline. Models, signal connections, focus/capture and arbitrary widget
data are not copied. The checked transfer helper reports allocation failure;
the legacy void helper cannot, and callers needing transactional replacement
should use the checked helper on a disposable candidate or use hot reload.
Tree matching is still a repeated ID lookup, not a scalable indexed reconciler.
Scene replacement publishes the new root before releasing the old tree.

## Regression commands

Run from the repository root:

```sh
cc -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -Wpedantic -Ikernels \
  tests/test-gfx-retgtk-model-layout.c tests/gfx-retgtk-model-layout-peer.c \
  -lm -ldl -o /tmp/opencode/gfx-retgtk-model-layout
/tmp/opencode/gfx-retgtk-model-layout

cc -std=c11 -D_GNU_SOURCE -O1 -Wall -Wextra -Werror -Wpedantic \
  -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Ikernels \
  tests/test-gfx-retgtk-model-layout.c tests/gfx-retgtk-model-layout-peer.c \
  -lm -ldl -o /tmp/opencode/gfx-retgtk-model-layout-asan
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /tmp/opencode/gfx-retgtk-model-layout-asan
```

The suite exercises nested/hidden intrinsic layout, min/max shrink/grow and
1,500 deterministic randomized pixel distributions; stable sorting of 16,384
duplicate-key rows in both directions; filtering/stale snapshots/model edits;
view bounds and corrupt tree indices; sibling removal/visibility changes and
child append from measure/arrange callbacks; malformed/truncated/mutated markup;
all explicit allocation-failure positions in parsing, building and reload;
actual retained parent/child reference counts and teardown; and state transfer
between controls constructed in different translation units. A final headless
frame paints a measured box with a button, toggle and entry using the built-in
font, checking actual surface and text pixels. It is not
hardware, Unicode layout, full XML or GUI protocol conformance testing.
