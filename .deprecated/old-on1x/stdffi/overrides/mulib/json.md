# JSON — discovery-only: `conditional_tree_ownership`

Unresolved semantics from `kernels/mulib-json.h`, the manual,
`on1x/contracts-exoml/json.exocontract.json` (20 entries) and the JSON section
of `on1x/contracts-exoml/obligations.json`. Not executable or reviewed.

- Constructors and successful parse produce owned roots; destroy only an
  owned root, never an attached/borrowed child. Append/set transfers a child
  only after success; failure preserves ownership and existing tree contents.
  Setting the same already-installed value is a no-op; replacing another
  value destroys the previous subtree and invalidates its borrows.
- Array/object accessors borrow from the owning tree. A missing C pointer and
  a nonnull JSON-null node are different. Key and string spans are
  length-delimited, valid UTF-8 and may contain NUL; not On1x strings. The
  length-delimited constructors/lookup accept null at length zero only where
  documented, never `SIZE_MAX` as a valid extent. A C string constructor
  rejects null. Preserve exact `size_t`, kind enum and `double` semantics;
  exact i64 conversion would need finite, range and integrality checks.
- Stringify returns independent malloc-owned bytes, released with `free`,
  not `mulib_json_destroy`. Parse initializes optional diagnostics; messages
  are static. Scratch/captures/partial tree are released on failure, including
  bounded nesting and object-key lookup work exhaustion.
- Exclude concurrent mutation/destruction and concurrent `LC_NUMERIC` changes
  during numeric parsing/printing. A const pointer does not establish this or
  the C storage size, alignment or borrow region.

Generic transfer-on-status with alias exceptions, tree borrow invalidation,
bounded binary UTF-8, exact conversions and distinct destructor identities
remain unlowered. Do not infer an executable subtree or scalar profile.
