# MIR grammar and specialization policy

`MIR.llsynth` describes canonical serialized MIR v1–v4 and the build-time
specialization policy. The wire grammar describes record shapes and tokens;
the C++ decoder and verifier remain authoritative for bounds, version-specific
type and opcode admission, directives, CFG, and effect ordering. The
`on1x.mir.wire_grammar` test parses real `on1xc --emit-ir` output with the
generated parser. Compiler tools do not use it at runtime.

LLSynth generates the LR/DFA parser; `rewrite/compile.py` validates its policy
parse tree and emits a static C++ database from `rewrite/core.mirp`. The compiler
rejects rules outside its small inventory of value-, trap-, and effect-
preserving theorems. A `requires` clause cannot introduce a new theorem.
No parser, Python, or Perl is used when specializing a module.

For a build without the vendored LLSynth tool, the checked-in database is used
only if its grammar, policy and compiler fingerprints still match. Regenerate
it with `cmake --build <build-dir> --target on1x_mir_policy_regenerate`; the
`on1x.mir.policy_compiler` test compares real generation against that snapshot.

## Runtime behavior

`mir::specialize` verifies its input and returns a verified copy. Empty MIR
selectors and `*` policy selectors match any product; all other selectors
match exactly, with no host inference or target aliasing. The request must
match both selectors, and its function ID limits coverage unless it is zero.
Policy activations select groups of applicable patterns, each with its own
selectors. Overlapping requests are combined and pattern order is the order
in the policy source. Consumed directives do not appear in the result, which
uses the executable MIR version needed by its remaining types and opcodes
(already unannotated v1–v3 inputs retain their version).

| Directive | Optimization group |
| --- | --- |
| `rewrite fold-identity` | Checked integer identities and reflexive scalar equality |
| `hint kernel-hot` | Checked integer identities |
| `hint canonicalized` | Reflexive scalar equality |
| `hint local-memory` | Same-block forwarding from the most recent store to the same i64 or bool slot |

The matcher binds SSA IDs, recognizes exact literals, and does not erase
operand producers, stores, assertions, calls or I/O. When it proves that a
checked instruction cannot trap, or that a local load reads the preceding
store's exact scalar value, it reconnects that block's effect chain. It does
not move instructions or evaluate code on paths that did not execute before.
Mechanical costs such as evaluator instruction counts can change.

Speculative requests and speculative patterns are disabled by default.
Enabling them is not permission to erase a possibly failing division.
For example, the `x/x` pattern requires nonzero evidence from a verified
constant or an earlier `assert(x != 0)` in the same block. It retains the
assertion and does not infer facts from hints, unexecuted branches, or
backedges.
