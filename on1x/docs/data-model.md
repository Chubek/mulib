# M1 bounded value data model

Scoped task: executable structural records, closed algebraic sums, fixed arrays
and **owning snapshot slices** in the canonical frontend and C11 backend.
This deliberately precedes general borrowed/mutable storage and nominal recursive
ADTs. No foreign layout or ownership admission follows from these types.

The later [intrinsic matching increment](core-syntax.md) adds guarded structural
patterns and typed decision lowering over these existing value representations.
The original spelling/profile below remains compatible. New matches can return
ordinary shared/managed values, but do not add them to record/sum/array fields,
admit unique-resource transfer, or widen aggregate MIR/SDK/foreign ABI support.

## Implemented contract (this slice)

- `record{x:i64,y:f64}` is a structural labeled product, constructed with
  `record{x:1,y:2.0}` and projected with `value["x"]`. Labels are canonicalized
  lexicographically for type identity/layout, but initializer effects run in
  source order. No row polymorphism, updates or implicit field copying syntax.
- `sum{None:record{},Some:i64}` is a closed structural tagged union. Construct
  with `variant<sum{None:record{},Some:i64}>(Some,42)`. Every constructor has one
  payload; the empty record is a unit payload. `match(value){None(unit)=>0,
  Some(n)=>n}` evaluates the scrutinee once and only the selected arm. Exactly
  one arm per constructor is required; missing, duplicate/unknown arms reject.
  Arms reuse the canonical name/tuple parameter patterns. No guards, recursive
  nominal definitions or arbitrary nested refutable patterns are admitted.
- `array<T,N>` is a fixed homogeneous immutable value array. `array[1,2,3]`
  infers its extent; `array[]` requires enough element-type context. `a[i]` is
  checked indexing and `a[]` is length. Whole-value `var` replacement preserves
  value snapshots; element mutation is not part of this profile.
- `a[lo:hi]` checks `0 <= lo <= hi <= length` and creates `slice<T,N>`, where N
  is backing capacity. A slice owns a **copy** of the backing value array plus
  offset/length; it can escape/return/capture without dangling pointers. Slicing
  a slice preserves the same capacity and composes offsets. This is not a
  zero-copy borrow, a dynamic/resizable array, or an alias to subsequent writes.
- Data fields/elements and match results must be trivially copied: i64/bool/f64,
  tuples and the new value data types. Managed functions/text/JSON/foreign
  owners are rejected. This needs no new heap allocator/drop path. Source
  arrays/fields have finite width (extent and nesting bounded by 256/32) and
  expanded value layouts are bounded by a 4096-unit width budget checked in the
  frontend, with deterministic located diagnostics.
- Raw aggregate externs reject (`data-valued extern ABI requires reviewed
  aggregate adapters`); existing explicit tuple flattening stays scalar only.
  MIR lowering (`--emit-ir`) rejects the new kinds with `ON1X-MIR-UNSUPPORTED`
  instead of a scalar approximation; the independent structural/Chisel adapters
  likewise fail closed. Bounds violations terminate with a located value error
  (exit 70), matching checked-arithmetic policy.

### ABI and generated code

Records, sums, arrays and slices lower to private versioned C11 structs
(`on1x_data_type_N` with an `ON1X_VALUE_DATA_ABI_VERSION` compile-time guard).
Field/discriminant names are compiler-owned (`element_K`, `tag`, `backing`,
`start`, `length`); user labels never become C identifiers, and label ordering
is canonical, so identical shapes share one layout. Sums store an `int64_t`
discriminant plus a union of payloads. Bounds checks go through
`on1x_bounds_index`/`on1x_bounds_slice`; an out-of-range discriminant reaching a
`match` raises `on1x_invalid_tag`. Value copies are plain C struct assignment
(no refcount or destructor is involved). Both scanners accept identical syntax;
no lexical changes were needed.


All syntax is additive/contextual and uses existing scanner tokens. Existing
module-qualified dotted names and macro bracket syntax remain unchanged.
Bounds errors are terminal failures, like checked arithmetic, not recoverable
errors. Generated type names/field names are compiler-owned; no C names or layout
are inferred from user labels. C11 remains the only execution backend here.
