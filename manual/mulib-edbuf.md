# Editor buffers

[Manuals](README.md) · [Library index](index.md)

**Header:** `mulib-edbuf.h`; define **`MULIB_IMPL_EDBUF`** once. No dependencies.
[Runnable example](../examples/modules/edbuf.c).

## Backends

`mulib_edbuf_create(kind, bytes, length)` copies the initial contents. Choose:

* **GAP**: contiguous allocation with a movable gap. Nearby edits reuse its
  space; distant edits move intervening bytes. Geometric growth is prepared
  before changing the buffer.
* **ROPE**: persistent AVL concatenation tree with leaves of at most 1024 bytes
  and cached subtree byte/newline counts. Split/join edits copy affected paths,
  sharing untouched nodes with internal reference counts. Tree height is
  logarithmic; edits cost O(log(leaves) + inserted/deleted text scanned), rather
  than flattening the document. Line-start lookup uses subtree newline counts.
* **PIECE_TABLE**: ordered vector of slices into immutable original/insertion
  blocks. Edits split/coalesce descriptors, never move surviving text. Descriptor
  edits are O(piece count), and inserted blocks remain allocated until destroy.
  This is a piece **table**, not a balanced piece tree.

These are three principal editor-storage algorithms, not a claim to implement
every published buffer design. All provide the same observable API and failure
semantics. No global allocator, filesystem, encoding, cursor or terminal policy
is hidden in a backend. Buffer/mark mutation requires external synchronization.

## Bytes, lines and searching

Offsets, erase lengths and columns are **bytes**, not Unicode scalars, graphemes,
or display cells. Embedded NULs and arbitrary bytes are accepted; applications
must preserve encoding boundaries if they want UTF-8 text. The maximum document
size is SIZE_MAX-1. Capacity arithmetic is checked before allocations.

`size`, `revision`, and `line_count` inspect state. An empty buffer has one line;
every LF adds a line, including a final empty line. CR is an ordinary byte (a
CRLF sequence therefore has a CR in the preceding line). `line_start` and
`position` use zero-based coordinates; EOF is a valid position.

`read` copies an in-bounds range into caller storage. `copy` returns a malloc-
owned NUL-terminated snapshot, which can contain earlier NULs; free with `free`.
No internal text pointer escapes, so edits cannot invalidate a borrowed slice.
`find` performs a binary literal KMP search at/after a start offset, including
across rope/piece boundaries. Empty patterns match start. Pattern-prefix storage
is O(pattern length); storage traversal costs are backend-dependent. Line/position
queries on gap/piece storage scan the needed prefix; position currently scans
the prefix on all backends.

## Edits, revisions and history

`replace(buffer, offset, removed, inserted, length)` is the fundamental operation;
`insert` and `erase` are conveniences. Inputs are copied. Every successful
nonempty edit increments a monotonic uint64 revision and updates `last_change`:
the previous/current revision and replaced/inserted ranges. Empty replacements
do not change history or revision. Revision exhaustion returns ERANGE.

All edits are transactional, including storage, marks, revision, and undo/redo:
allocation/validation failure preserves them. Gap edits preflight growth/history;
rope edits commit a new root only after all splits/joins succeed; piece tables
commit new descriptors/block together. Deletion may allocate on rope/piece
backends and can therefore fail.

History defaults to 128 **edit entries**, not a byte budget. Each entry owns
copies of removed/inserted bytes. `history_limit` clears both stacks and chooses
a new maximum; zero disables recording. `undo`/`redo` replay one replacement
atomically, advancing the revision rather than restoring an old revision number.
A new edit discards redo history. There is no implicit typing coalescence,
multi-edit transaction, history grouping, persistence or unbounded undo.

## Marks and ownership

`mark_create(buffer, offset, right_gravity)` creates an independently owned mark.
At an insertion boundary, left gravity stays before inserted text, right gravity
moves after it. Marks after a replaced range follow the surviving suffix. Marks
in erased interiors collapse to the replacement boundary; a mark at a nonempty
erased range's right edge follows the suffix. Undo applies these same rules; it
does not recover erased interior positions.

Release marks with `mark_destroy`. Destroying their buffer detaches them but does
not free the mark objects; `mark_offset` then returns SIZE_MAX. Buffer destruction
also frees all text blocks, tree nodes, descriptors and history. NULL destruction
is harmless. Do not copy opaque handles or access them concurrently with mutation.

## Results and integration

Operations return zero on success and negative `EINVAL`, `ENOMEM`, `ERANGE`, or
`ENOTFOUND`. Failed reads/searches/coordinate queries preserve output arguments.
Creation/copy/mark creation return NULL on failure. `termsyn` consumes buffer
snapshots and revisions without depending on its chosen storage algorithm.

Tests compare all three backends against a byte-array model over randomized
edits, validate AVL balance, and exercise binary bytes, empty/EOF lines, history,
marks, range errors and allocation-failure rollback.

Build: `cc -std=c11 -Iinclude examples/modules/edbuf.c -o edbuf-example`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-edbuf.h`](../include/mulib-edbuf.h) |
| Implementation switch | `MULIB_IMPL_EDBUF` |
| Repository-local includes | None |
| Complete example | [`examples/modules/edbuf.c`](../examples/modules/edbuf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/edbuf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
