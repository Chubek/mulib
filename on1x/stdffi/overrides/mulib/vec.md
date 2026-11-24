# Vec — discovery-only: `stride_region_invalidation`

Unresolved semantics from `kernels/mulib-vec.h`, the manual,
`on1x/contracts-exoml/vec.exocontract.json` (7 entries) and the Vec section
of `on1x/contracts-exoml/obligations.json`. Not executable or reviewed.

- The record owns a byte buffer, not the storage holding the record.
  Initialize only fresh/freed storage; init overwrites without releasing a
  previous buffer. Require live initialized record, nonzero stride and
  checked `length <= capacity` and `capacity * stride`. Element/output spans
  need at least `stride` bytes, not a guessed pointer/length pair.
- `reserve`/`push` return 1 on success, 0 on failure; failed growth leaves
  existing buffer/length intact. `push` can deliberately rebase a live
  internal element across realloc; this is not proof arbitrary input aliases
  survive growth.
- `at` borrows a logical element. Growth can invalidate every view; pop/clear
  invalidate removed elements. Null pop output discards bytes; failure does
  not initialize output. Clear preserves capacity. Free releases only the
  byte buffer, resets length/capacity, retains stride and permits reuse.
- Byte copies do not clone, retain, move or destroy pointed-to resources.
  No ownership-bearing element type can be admitted by raw byte copy.

Stride-bound storage, internal alias rebasing, status-driven out initialization
and realloc/logical-length borrow regions remain unlowered. The selected
Cephyr libc profile also lacks the header's `<math.h>`; this is a projection
failure, not an ABI mismatch or a reason to rewrite the header.
