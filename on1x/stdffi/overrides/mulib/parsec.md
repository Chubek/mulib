# Parsec — discovery-only: `transitive_descriptor_borrows`

Unresolved semantics from `include/mulib-parsec.h`, the manual,
`on1x/contracts-exoml/parsec.exocontract.json` (18 combinator entries) and
the Parsec section of `on1x/contracts-exoml/obligations.json`. Not executable
or reviewed.

- Constructors return by-value C descriptors, not owning boxes. They save
  borrowed child arrays/descriptors, literal bytes, bitmaps, label strings,
  reference slots and callback/context; transitive referents must remain live
  through every run, and grammar/reference slots must not mutate concurrently.
  `satisfy`/`named` have multiple borrowed field origins.
- Predicate effects are not expressed in `run`'s signature. Backtracking can
  invoke the predicate repeatedly, so the sidecar requires pure predicates.
  Neither the callback nor the non-owning descriptor has an unregister call.
  Do not treat this as the framed IPC registration lifecycle.
- Run requires fresh or released output storage. Status 1 commits captures,
  0 mismatches, and -1 is fatal/invalid/limit/allocation; failures commit no
  captures or capture allocation. Result owns captures, not input bytes or
  diagnostic labels. `result_free` only takes initialized/zeroed storage and
  resets it. Bounded run charges literal comparisons; zero work limit uses
  16,000,000. Whole-input success requires the explicit end recognizer.
- Combinator-only recipe does **not** enable optional `MULIB_IMPL_PARSER`.
  Other owned grammar/AST entry points have separate recipes and obligations.

Aggregate ABI with field-sensitive transitive regions, indirect callback
effects and status-aware result lifecycle remain unlowered. No blanket
export of projected by-value descriptors.
