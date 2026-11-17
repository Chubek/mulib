# Category authoring libraries (native On1x)

- `syntax.on1x`: bounded grammar over located tokens; one `Self` binder, scalar
  signatures, concrete implementations and named witnesses.
- `witness.on1x`: local declaration identities, concrete substitution,
  duplicate-head rejection and ordinary typed function/selector generation.
- Canonical transport/schema/sexpr/visitor code remains in `../ppx.on1x`; generic
  token, diagnostic and naming helpers are one directory up.

Processor: `../../../stdppx/native/protocols.on1x`. Shared API tests and real
clients execute through the C11 bootstrap. Expanded functions must pass normal
compiler checks. Metadata certifies no typing, ownership, law, foreign safety or
concurrency property. Full API/budgets/limits: `../../../docs/category.md`.
No unimplemented API is represented by a stub.
