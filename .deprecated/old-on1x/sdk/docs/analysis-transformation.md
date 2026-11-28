# SDK 2 verified-MIR analysis and transformation subset

This Stage C API consumes only host-created `ON1X_SDK_VERIFIED_MIR` programs.
`observe` independently decodes, verifies, and canonicalizes the existing On1x
MIR serialization before caching it. It does not publish compiler revisions or
turn external bytes into trusted IR.

## Revision and lifetime contract

An analysis key contains owner, representation identity, revision, target, and
options identity. A manager keeps one current revision for each complete key.
Observing a newer revision validates the full candidate before atomically
replacing the prior observation and invalidating matching cache entries. An
older query returns `ANALYSIS_STALE_REVISION`; conflicting bytes at the same
revision return `ANALYSIS_REVISION_CONFLICT`.

Managers use caller-owned aligned storage. Their internal allocations never
cross the C boundary. Results and candidate bytes are immutable borrowed views
valid only until the next mutating manager operation. Every public descriptor
has the ABI-2 `abi_version`/`struct_size` prefix. Existing ABI-v1 analyzer and
source-transformer layouts, callbacks, statuses `0..7`, and behavior are
unchanged. Managers are thread-confined. A synchronous callback may inspect
immutable state, but re-entering a mutating manager operation returns `BUSY`.

Queries are deterministic and bounded by separate work and record limits. A
cache hit still enforces the caller's current limits and polls cancellation.
Limit exhaustion is `ON1X_SDK_LIMIT`, not a deadline. Cancellation and host
deadline decisions use distinct ABI-2 values. A failed control or
instrumentation callback is `ON1X_SDK_ERROR`; malformed verified MIR is
`ANALYSIS_VERIFICATION_FAILED`.

## Implemented analyses

The canonical adapter supports MIR v1-v3 without narrowing IDs, integer
payloads, or float bit payloads:

- ordered CFG blocks and edge ordinals;
- immediate dominators and depths;
- natural loops plus explicitly marked irreducible strongly connected regions;
- block-parameter/instruction definitions, exact operand/terminator/edge uses,
  and SSA liveness with simultaneous block-argument transfer (block parameters
  are definitions, not live-in values);
- exact instruction effects and conservative transitive function effect masks.

General alias/points-to claims are unsupported. Current scalar MIR slots are
function-local and non-addressable, but that narrow fact is not promoted into a
general source, foreign, resource, or ownership proof.

The optional Novostone adapter uses the actual relational `Reachable`,
`LiveIn`/`LiveOut`, and `MayEffect` queries for CFG, liveness/use-def, and
effects when the exact Novostone target is linked. The selector rejects in other
builds. Its existing mapping is MIR-v1 only and is never used to reinterpret
MIR v2/v3 numeric payloads. Wetstone's existing adapter consumes checked HIR,
not verified scalar MIR, so the MIR manager rejects that selector instead of
inventing a translation. Projection and relational-query work are charged to
the same finite query ceiling.

## Pass transactions

The pipeline parser accepts only a bounded comma-separated list of the four
built-ins: `constant-fold`, `simplify-cfg`, `local-slots`, and `dead-pure`. It
does not load code or resolve arbitrary names. Each attempt runs on a private
copy through the canonical pass implementation, canonical verifier, and checked
preservation declarations. Instrumentation is synchronous and borrowed.

`LIMIT`, cancellation, deadline, callback failure, stale source, and verifier
failure return no candidate from the ABI-2 API. The observed source remains
unchanged. On success, candidate bytes remain untrusted until the host performs
its mandatory checks and publishes a new revision.

Pass work is bounded by both the pipeline work limit and the generic control
work limit; the smaller ceiling wins. Passes have dedicated rewrite and
verifier limits, so the generic control `result_limit` must be zero.

`on1x_sdk_translation_test` compares one explicitly named scalar invocation in
the canonical reference evaluator. Its result says only
`EVIDENCED_EQUAL`/`EVIDENCED_DIFFERENT` for that one test and reports
`is_equivalence_certificate = 0`. It is not a general transformation proof.
