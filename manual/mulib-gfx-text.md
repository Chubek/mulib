# MuLIB graphical text editing

The C11 header-only text kernels are shared by the native ImmGTK and RetGTK
editors. Include `mulib-texteditor.h` for plain editing, `mulib-richtext.h` for
style spans, or `mulib-gui-undo.h` for application-owned undo commands.
No implementation switch or third-party runtime is required.

## Storage, UTF-8 and offsets

`mulib_textbuffer_t` owns contiguous storage. Initialize buffers, histories,
rich text and command stacks to zero; do not copy owning objects. Editor
`init` initializes an empty/uninitialized editor; free an existing editor before
calling `init` again. All owning objects have a corresponding `_free` function,
which also accepts NULL. Contexts are single-thread owned.

`data` contains `n` bytes followed by a NUL terminator; `cap` includes that
terminator. A zero buffer with NULL `data` is valid empty text.
Length-bearing inputs need not be NUL-terminated, and embedded U+0000 is valid.
GUI wrappers using C strings may display only the prefix before an embedded NUL.
Callers must supply readable input for the stated length and must not mutate
owned fields or storage behind the APIs.

- `mgx_utf8_decode` accepts only well-formed Unicode scalar encodings: no
  overlong encodings, isolated continuations, surrogates, truncation or values
  above U+10FFFF. Failure leaves `cp` and `used` unchanged; NULL output pointers
  are rejected. Noncharacters are valid Unicode scalars.
- `mgx_utf8_valid` validates a complete length-bearing input. NULL is accepted
  only with zero length.
- `mgx_utf8_boundary` is a cheap byte-boundary predicate **for already validated
  UTF-8**, not a replacement for validation.
- `mulib_textbuffer_valid` checks storage metadata, the terminator and UTF-8.
  As with any C API, it cannot prove that an arbitrary non-NULL pointer or forged
  capacity actually denotes accessible memory.

Replacement ranges are half-open byte intervals `[start,end)`. Both endpoints
must be Unicode scalar boundaries in `[0,n]`, not character counts. Invalid
offsets are rejected, not silently rounded. Buffer replacements support aliased
input, including interior slices of the destination, even if allocation moves.
Validation, overflow or allocation failure leaves the buffer unchanged.

## Cursor and selection

Selections retain the public `anchor` and `caret` byte-offset members.
`mulib_textselection_valid` checks both scalar boundaries.
`mulib_textselection_set` installs a selection only if valid.
`mulib_textselection_move` does nothing for an invalid selection, NULL arguments
or zero direction. Negative/positive direction means previous/next;
non-extended motion collapses a nonempty selection to its directional edge.

Cursor movement uses a **limited grapheme approximation**, covering selected
combining ranges, variation selectors, emoji modifiers, ZWJ chains, CRLF and
regional-indicator pairs. It is **not full UAX #29, Unicode shaping, or visual
BiDi navigation**. Byte offsets inside a cluster move to its enclosing next or
previous approximate cluster edge. Offsets past the end clamp to the end;
invalid buffers return zero. Selections may deliberately split an approximate
cluster at a scalar boundary, but may never split an encoded scalar.
Navigation scans from the beginning to preserve consistent RI pairing and
next/previous edges; it is linear, not a large-document indexing solution.

## Editing and history budgets

Existing editor members (`text`, `selection`, `history`) and functions remain.
`mulib_texteditor_replace` additionally accepts an explicit byte range.
Initialize an editor before editing it; a zero object alone has no initial undo
snapshot. `mulib_texteditor_valid` checks the live text against the current
history snapshot as well as checking its selection. Directly replacing
`editor.text` would desynchronize history and is rejected by subsequent editor
edits/undo; use the editor API, or free and initialize a new document instead.
Insert replaces the current selection; delete removes the selection or moves
one approximate cluster backward/forward. Empty insertion and deletion at an
already-reached boundary succeed without adding history or discarding redo.

An edit stages replacement text and its history snapshot before committing.
Invalid selection/input, allocation failure, overflow and an over-budget new
snapshot preserve the text, selection, history position and redo branch.
Undo/redo allocation failure also preserves all those states. Selection motion
before a successful edit is stored with the outgoing snapshot; undo restores
that selection, including anchor direction. Motion before undo/redo is saved
for a subsequent reverse step. Ordinary selection motion does not itself create
an undo entry.

`mulib_texthistory_t` adds:

| Member | Meaning |
| --- | --- |
| `limit` | Maximum snapshots, including the current one; zero selects 64. |
| `byte_limit` | Maximum allocated snapshot text bytes; zero selects 8 MiB. |
| `bytes` | Current snapshot text allocation total, including all terminators. |

Defaults can be overridden before inclusion with
`MULIB_TEXTHISTORY_DEFAULT_LIMIT` and
`MULIB_TEXTHISTORY_DEFAULT_BYTE_LIMIT`. Effective limits are at least one.
Each snapshot allocates exactly `text.n + 1` bytes, so even empty snapshots cost
one byte. Metadata vector capacity, live editor storage and temporary
transaction allocations are **not** included in this snapshot-text byte cap;
it is not a whole-process or peak-memory bound.

A successful record drops redo, then evicts as many oldest undo snapshots as
needed to meet **both** caps. If the new state cannot fit alone, recording and
the editor edit fail unchanged; undoability is never silently abandoned.
Use `mulib_texteditor_init_with_limits` to initialize an editor with custom
budgets, including documents larger than the default byte budget.

`mulib_texthistory_set_limits(history,count,bytes)` applies new caps immediately
without allocating. It preserves the current snapshot, trimming farthest redo
states first and oldest undo states second. If the current snapshot cannot fit,
the entire limit change fails unchanged. Setting either argument to zero selects
its default. Direct field changes are enforced on the next successful record;
prefer the setter for immediate enforcement and accurate budget accounting.
Snapshot contents and `bytes` are implementation-owned. History-step destinations
must be independent buffers, not elements of the history's own snapshot array.

The implementation still uses full contiguous snapshots. It does not implement
a rope/piece tree, edit coalescing, full rich-text undo or an IME.

## Rich text and command undo

Rich spans use nonempty, scalar-aligned half-open byte ranges.
Invalid spans and allocation failures do not change the span list.
Last-added matching span wins. Invalid/non-boundary lookup offsets return the
default white style. `mulib_richtext_set` changes text and clears spans only on
success; use it instead of directly editing `rich.text`, which can stale ranges.
This is a style-span container, not a complete styled-document editor.

GUI command undo transfers command ownership only after successful `push`.
Allocation or `apply` failure preserves the stack and redo branch and does not
invoke `release` on the rejected command. `release` runs exactly once per
accepted entry when discarded or freed. Undo/redo advances the position only
when its callback succeeds. Callbacks must themselves be failure-atomic: the
stack cannot roll back arbitrary external side effects. Reentrant push/step
operations are rejected; freeing the same stack inside a callback is a no-op.
This guard does not provide cross-thread synchronization.

## Regression test

```sh
cc -std=c11 -pedantic -Wall -Wextra -Werror -Wno-misleading-indentation \
  -Ikernels tests/test-gfx-text.c -lm -o /tmp/opencode/test-gfx-text
/tmp/opencode/test-gfx-text
cc -std=c11 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra -Werror -Wno-misleading-indentation \
  -Ikernels tests/test-gfx-text.c -lm -o /tmp/opencode/test-gfx-text-asan
ASAN_OPTIONS=detect_leaks=1 /tmp/opencode/test-gfx-text-asan
```

The test checks every Unicode scalar's encoding, malformed UTF-8, interior and
oversized offsets, embedded NUL, aliased edits with always-moving realloc,
approximate cluster edges, reversed selections, undo/redo branching, count and
byte-cap eviction, transactional limit changes, callback reentrancy and
failure-atomic commands. Allocation failures are injected at every allocation
point in representative buffer/editor/history/rich-text operations, with exact
pointer/capacity/content comparisons and leak accounting. A deterministic
2,000-operation model test mixes UTF-8 edits, malformed input, selection changes,
undo/redo and budget changes while checking every retained snapshot invariant.
