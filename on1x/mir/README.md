# MIR grammar and specialization policy

`MIR.llsynth` describes canonical serialized MIR v1–v4 and the build-time
specialization policy. The wire grammar describes record shapes and tokens;
the C++ decoder and verifier remain authoritative for bounds, version-specific
type and opcode admission, directives, CFG, and effect ordering. The
`on1x.mir.wire_grammar` test parses real `on1xc --emit-ir` output with the
generated parser. Compiler tools do not use it at runtime.

LLSynth generates the LR/DFA parser; `rewrite/compile.py` validates the activation
policy in `rewrite/core.mirp` and all ten pattern banks in `rewrite/banks/`, then
emits a static C++ database. It separately compiles the scoped MEMGraph and
On1xMETA banks in `rewrite/memgraph/` and `rewrite/meta/`. The compiler rejects
missing, empty or unactivated core
banks, duplicate pattern names and matches, and rules outside each bank's allowlist
of value-, trap-, and effect-preserving theorems. A `requires` clause cannot
introduce a new theorem. No parser, Python, or Perl runs during specialization.

For a build without the vendored LLSynth tool, the checked-in database is used
only if its grammar, activation policy, banks and compiler fingerprints still
match. Regenerate it with `cmake --build <build-dir> --target on1x_mir_policy_regenerate`;
the `on1x.mir.policy_compiler` test compares real generation against that snapshot.

## Runtime behavior

`mir::specialize` verifies its input and returns a verified copy. Empty MIR
selectors and `*` policy selectors match any product; all other selectors
match exactly, with no host inference or target aliasing. The request must
match both selectors, and its function ID limits coverage unless it is zero.
Policy activations select banks of patterns, and both activations and patterns
may narrow that selection with their own selectors. Overlapping requests are
combined. Pattern order follows the bank order below and the declaration order
within each bank. Consumed directives do not appear in the result, which
uses the executable MIR version needed by its remaining types and opcodes
(already unannotated v1–v3 inputs retain their version).

### Scoped rewrite actions for HIR producers

| Producer | Wire kind | Wire action | Pattern bank |
| --- | --- | --- | --- |
| MEMGraph | `rewrite` | `memgraph-local` | `rewrite/memgraph/memgraph-local.mirp` |
| On1xMETA | `rewrite` | `meta-canonical` | `rewrite/meta/meta-canonical.mirp` |

Both actions require the nonzero ID of an existing MIR function. Neither is
admitted as a `hint`, and neither extends foreign recipe admission. They use the
existing v4 field order:
`(directive "mir-c11" "x86_64-unknown-linux-gnu" "rewrite" "memgraph-local" 42 false)`.
The wire encodes a wildcard as `""`, not `"*"`. Nonempty selectors are
case-sensitive; `"x86_64-linux-gnu"` is not `"x86_64-unknown-linux-gnu"`.
Rule selectors may independently narrow a matching directive.

The existing speculation bit is **mode-exact for these two actions**: `false`
requests only safe patterns; `true` requests only speculative patterns, and only
with separate host opt-in. A producer can request both modes with two clauses.
No part of the directive establishes a proof. Verification applies even to
unselected directives, and specialization verifies the result after removing
the directives from its private copy. Older v4 readers may reject the new
action names. Legacy hints and `fold-identity` keep their previous semantics.

The banks own independent `bootstrap.hpp` files and regeneration targets
`on1x_mir_policy_regenerate_memgraph` and `on1x_mir_policy_regenerate_meta`.
Content changes to one bank do not invalidate the other's bootstrap or core's.
Each directory can add a `test_specialize.cpp`, automatically discovered by
CMake; the two generated-policy tests check their respective bootstraps.

The MEMGraph profile accepts `load()` with `replace $stored` (latest same-block
store) or `replace $loaded` (earlier same-block load with no intervening store
to that slot). The proof names are `local-store-*` / `local-load-*` for `word`
(u8/u16/u32/u64), `f32`, `f64` and `tuple`. The first three type families are
safe rules; tuple reuse is restricted to the speculative mode. A fact carries
the exact value, including floating-point bits or the immutable tuple, not a
claim about storage placement or ownership. The speculative `redundant-store`
rule drops a store only when a retained store in the same block already wrote
the exact same SSA value to that slot.

The META profile admits safe checked negation of zero, boolean `eq`/`ne`
against the result of a same-block `not` (`bool-complement`), and `lt` against
the i64 minimum, `gt` against the i64 maximum, and `ge` against the i64 minimum.
Its speculative proofs are
`double-checked-neg` (keep the checked inner negation, substitute `$original`
for its outer inverse) and `nonzero-comparison` (i64 `eq`/`ne` against zero with
constant or prior same-block assertion evidence). Neither a speculative
directive nor a past HIR rewrite can substitute for these checks.

## The ten core pattern banks

`rewrite fold-identity` enables both the arithmetic and predicate banks;
`hint kernel-hot` enables arithmetic, `hint canonicalized` enables predicates,
and `hint local-memory` enables only memory.

| Bank | Family | Rewrites |
| --- | --- | --- |
| `additive-identities` | Arithmetic | Checked `x+0`, `0+x`, `x-0`, `x-x` |
| `multiplicative-identities` | Arithmetic | Checked multiplication by zero or one, on either side |
| `division-units` | Arithmetic | Checked division by one; remainder by one for `mir-archon` on `x86_64-unknown-linux-gnu` |
| `guarded-division` | Arithmetic | Checked division and remainder with equal operands or a zero numerator, only with nonzero denominator evidence and speculation enabled |
| `scalar-equality` | Predicate | Reflexive `eq` and `ne` on i64/bool |
| `ordered-reflexivity` | Predicate | Reflexive `lt`, `le`, `gt` and `ge` on i64 |
| `boolean-negation` | Predicate | Negation of a known boolean and same-block double negation |
| `boolean-comparisons` | Predicate | Boolean-only equality to true and inequality to false, either operand order |
| `store-forwarding` | Memory | Forward the value of the latest same-block store to an i64/bool slot |
| `load-reuse` | Memory | Reuse a preceding same-block i64/bool load if that slot wasn't written in between |

The matcher binds SSA IDs, recognizes exact literals, and does not erase
operand producers, assertions, calls or I/O. When it proves that a
checked instruction cannot trap, or that a local load reads the same exact
scalar value already stored or loaded locally, it reconnects that block's
effect chain. It does not move instructions or evaluate code on paths that
did not execute before.
Mechanical costs such as evaluator instruction counts can change.

Speculative requests and speculative patterns are disabled by default.
Enabling them is not permission to erase a possibly failing division.
For example, the `x/x` pattern requires nonzero evidence from a verified
constant or an earlier `assert(x != 0)` in the same block. It retains the
assertion and does not infer facts from hints, unexecuted branches, or
backedges. No bank rewrites floating-point comparisons or CFG edges (apart
from substituting equivalent SSA values in their arguments). The legacy core
memory banks are limited to i64/bool; the scoped MEMGraph bank can additionally
admit exact local loads of typed numeric and tuple values.
