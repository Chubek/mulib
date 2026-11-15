# derive_json — proposed PPX, NOT IMPLEMENTED

**Goal:** JSON encoders and decoders. This entry is a specification and development checklist, not an executable processor.

## Dependencies

ADTs, Result errors, ownership-safe JSON bindings. On1x's present PPX wire tree contains functions, statements and expressions, **not** record or variant declarations. The PPX runs before inference, so it cannot assert inferred type, effect, ownership, ABI or lifetime facts.

## Implementation contract

1. Extend On1x grammar/ASDL and the PPX schema only when necessary, with a versioned migration and backward-compatible input validation.
2. Define precise user-facing syntax; write successful and failing examples. Do not allow unregistered extension carriers to reach core HIR.
3. Implement a bounded, pure AST transformation for syntactic work. If typed information is necessary, lower an explicit marker into a compiler-owned post-elaboration pass instead of guessing.
4. Preserve hygiene, evaluation order, source spans, imports and ownership; fail closed if the source feature is unsupported.
5. Validate the resulting AST against `grammar/PPX.schema.json`, perform `on1xc --emit-c` round trips, execute binaries under AddressSanitizer and UndefinedBehaviorSanitizer where applicable, and add negative compiler tests.

## Admission criteria

Do not mark this processor implemented until it has actual source code, a declared protocol/version, passing integration tests, documented type and effect behavior, and a target/backend compatibility statement. Do not treat ExoML contract metadata as a proof of safe FFI behavior.
