# Intrinsic core-syntax additions: G1/G2 control and structural matching

This implements **statement/value loops and bounded structural matching** from
`../ON1X_CORE_SYNTAX_ADDITIONS.md`, not the entire specification. It is intrinsic
compiler syntax, requiring no extension activation, macro, PPX, interpreter or
new scheduler. Existing Category work is independent.

The subsequent [nominal-data increment](nominal-adts.md) implements section 11's
initial nonrecursive named-record/parameterized-enum profile through this same
matching and C11 path. It is not recursive ownership or the remaining features.

## Statement loops

```on1x
loop { work(); if finished() { break; } }
outer: while condition() {
  for (var i = 0; i < 10; i = i + 1) {
    if skip(i) { continue; }        # executes the step exactly once
    if stop(i) { break outer; }     # skips inner latch and outer condition
  }
}
for i in 0..10 { println(i); }      # 0 through 9
for i in 0..=10 { println(i); }     # 0 through 10
```

- Statement `loop` is endless unless exited. Expression `loop` and typed value
  exits are documented below. `while` and `for` stay statement-only.
- Three-clause `for` has a lexical initializer scope covering condition, body
  and step. Initialization is one `let`/`var`, assignment or expression clause;
  the condition is `bool`; the step is one assignment or expression, not a
  declaration/control statement. All clauses can be omitted (`for (;;)`);
  omitted condition means true. Body locals are not visible in the step.
  The initializer runs once even if the loop performs zero iterations.
- `continue` goes to the target's latch, executes its step once, then checks the
  condition. `break` goes directly to its exit. An outer continue skips inner
  latches; an outer break executes none of the abandoned steps.
- Both range bounds must be **i64**, without implicit coercions. They evaluate
  **left-to-right, once**, before introducing the immutable induction variable.
  Ranges ascend by one; reversed intervals are empty. The body gets a fresh
  immutable binding each iteration and can shadow it in a nested scope. No array,
  owning list or iterator handle is allocated.
- Inclusive completion is tested **before increment**: `MAX..=MAX` visits
  `9223372036854775807` once, including with `continue`. Half-open ranges stop
  before the bound. No overflowing sentinel, wrapping or saturating increment
  is used. Explicit for-C steps retain ordinary checked arithmetic/trap behavior.
- Labels form a separate lexical namespace, applying only to loops. Duplicate
  active labels in the same syntax context reject. Exits cannot cross functions,
  lambdas or staging worlds. Bare exits target the nearest loop in their own
  syntax context. Following an unconditional exit is unreachable, like return.
- Existing lexical cleanup runs before each transfer: body-local closures,
  tuples containing closures and JSON owners/loans leave scope exactly once.
  A for initializer survives continue and leaves scope on loop exit. Return
  retains its normal cleanup. **This is not `defer`, `using`, error unwinding or
  cancellation.** Existing terminal arithmetic/OOM traps do not gain unwinding.

## Compatibility and hygiene

New loop words are contextual, not globally reserved. Ordinary `loop()`, `for(x)`,
`break()` and `continue()` keep their meaning. Bare `break;`/`continue;` resolving
to an existing ordinary value/function remain expression statements, also in
imported modules. Use labeled exits when such a binding exists. Labels do not
shadow values. Previously valid identifier programs are not silently reinterpreted.

Macro-introduced loops/labels/exits retain syntax contexts. A captured user
block's exit cannot be captured by a template-introduced loop. Repeated template
expansions can reuse label spellings without collision. Implicit template exits
to invocation-site loops reject rather than acquiring control by accidental
capture. Definition-site label capture through locally defined macros and
captured label-name templates are not supported in this increment. Block
captures inside labeled macro loops are tested, including after identity PPX.

`..`/`..=` are longest-match tokens; existing macro `...` is unchanged. Both
generated Aurocks and fallback scanners agree on bytes/locations. Matching adds
`|` and recognizes `::` to diagnose unsupported list-cons patterns. Shell
literals and additional comment syntax remain unsupported.

## Value-producing loops

```on1x
let answer = search: loop {
  for n in 0..10 {
    if n*n >= 40 { break search with n*n; }
  }
  break with -1;
};
println(answer);                         # 49
(loop { break with println("done"); }); # discard a void result
```

`loop` expressions have intrinsic typing and lowering, not an implicit lambda
or PPX. `break with expr;` and `break label with expr;` evaluate the operand once,
before releasing the scopes being exited. Normally completing value operands
must share an exact type. A bare break makes the loop result void; the other
value operands are still checked and evaluated on their selected paths. No
normally completing value exits also means void, not a general `never` type.
Return/continue/outer exits retain the enclosing source function/loop target.

Existing statement `loop`, `while` and `for` retain bare-exit semantics. Value
exits must target an **expression** loop; use `(loop { ... });` to discard one.
`break with;` still targets the legal label `with`, while `break with value;`
is unambiguously a value exit even if an ordinary `break` binding exists.
The delimiter `with` takes priority in payload syntax; use a different label
spelling when an explicit labeled **value** exit is needed.
Only lexical reachability is used; constant conditions do not exempt an arm
from type checking. Return analysis for loops without value exits remains
conservative. There is no loop-exhaustion `else` or bottom-type coercion.

Existing bounded by-value and RC/shared types can be results, including closures
and tuples of closures: result references are acquired before inner cleanup.
Unique JSON owners/views cannot be transferred or discarded as value exits;
arm/loop-local JSON operations keep existing lexical lifetime rules. Fatal
arithmetic/allocation failures still do not unwind. See [the design boundary,
example and tests](value-loops.md).

## Structural match statements and expressions

```on1x
let result = match input with {
  Pair(n, true) if n > 0 => { let doubled = n * 2; doubled },
  Pair(n, _)            => n,
  Empty()               => 0
};
match let snapshot = make_array() with {
  [a, b] => println(a + b),
  rest   => println(snapshot[])
};
```

- Scrutinees evaluate once, before any pattern binder; a binding discriminant
  introduces one immutable arm-wide name, not a statement-expression facility.
- Patterns share the existing `Pattern` representation: names, `_`, final-arm
  `rest`, scalar/string literals, tuples, closed-sum constructors, exact fixed
  arrays/snapshot slices, `record{field:pattern,..}`, `p as name`, and `p | q`.
  Constructor arguments destructure a tuple payload, or one pattern matches the
  entire payload. `Empty()` requires the empty-record unit payload. Arrays must
  have the exact static extent; slices test their runtime length first.
- An ordinary name catches any value. Names bind at most once per alternative;
  or-patterns must bind exactly the same names/types on both sides. Alternatives
  backtrack through pure structural tests; a guard runs **once per matched arm**,
  after all its binders are initialized, not once per overlapping alternative.
- Arms run top-to-bottom. Guards must be bool. Literal `true` (including an
  omitted guard) contributes coverage; nonconstant guards do not. Literal-false
  guards, provably redundant arms and non-exhaustive matches reject. A bounded
  constructor-matrix analysis handles booleans, sums and nested products/array
  lengths. Numeric/string domains require a catch-all. Exhaustion is never proof.
- A match expression unifies all normally completing arm results. An arm block
  yields its final **unterminated expression**; a semicolon-ended/empty block is
  void. Statement matches discard ordinary arm results and accept heterogeneous
  arm types. Bare expression statements elsewhere still require semicolons.
- Patterns and guards get an arm scope; block locals get a nested scope shared
  with the block's final expression. Bindings cannot leak to another arm or out
  of the match. `return`/lexical loop exits in new arm blocks target the enclosing
  source function/loop, not an implicit lambda.
- Trivially copied values and existing managed/shared values may be matched and
  returned. Existing RC/JSON lexical cleanup runs on arm completion, early return
  and loop transfers, including abandonment of earlier enclosing operands.
  Unique JSON owners/views **cannot be scrutinees or match results**; they can be
  created/borrowed inside an arm under the existing rules. Fatal guard/arithmetic/
  allocation failures keep their existing status and do not acquire unwinding.
- Legacy `match(value){Tag(payload_pattern)=>expr,...}` retains its existing
  payload-lambda semantics, completeness rules and trivial-result restriction.
  Both spellings disappear before backend emission. `_` remains an ordinary
  binding name in old name/tuple parameter/let patterns; it is a wildcard only
  in the new match-pattern grammar. Contextual `match`, `with`, `as` and `rest`
  do not globally reserve ordinary identifiers.

Owning lists/list-cons patterns, generalized iterator protocols, refutable
let/parameter patterns, nominal/parametric/recursive ADTs and a general bottom
(`never`) type are not added. Statement matches with all arms returning can
satisfy definite return; a void match expression cannot coerce to a value merely
because all its arms transfer control.

## Pipeline and independent verification

The token-arena ASDL remains the scanner's generated representation; the existing
C++ `Stmt`/`Expr` source bridge is extended, not replaced by a second parser.
`src/parser.cpp` implements productions summarized in `grammar/On1x.g`.
Generated scanner outputs stay build artifacts, as before.

1. Parse `loop` as a true-condition loop and for-C as a lexical preheader plus
   condition/body/latch loop. A source `Range` retains both bounds/inclusive bit.
2. Loader resolution, hygienic macros and PPX operate on source syntax. Additive
   v1 nodes are `ControlWhile`, `RangeLoop`, `LoopExit`. Existing `While` fields
   retain their meaning. Syntax contexts are naming data, not type/effect proofs.
   **Canonical target IDs are not wire fields.** Invalid operations, labels or
   range shapes reject. Rebuild native processors with the additive schema;
   older schemas may reject the new constructors, never reinterpret them.
3. Staging separates function worlds. `src/canonical/control.cpp` expands ranges
   to typed i64/bool preheaders, condition/body/latch blocks and checked increment;
   resolves lexical targets; and recomputes identities after specialization.
   Private unlexable names are introduced only after source transforms. Failed
   normalization publishes no partial tree.
4. Ordinary inference/checking validates types, mutability, scopes, captures and
   returns. The independent bounded control verifier checks unique identities,
   active targets, exit shapes, source points, function boundaries and absence of surface
   ranges. Emitters and optional independent adapters invoke it again.
5. C11 uses explicit labels/jumps with ownership cleanup on exited scopes.
   Existing MIR uses `Jump`/`Branch` and header/body/latch/exit CFG blocks. Its
   verifier checks reachability, dominance, slot initialization, types and effect
   sequencing. Serialization, evaluation and transactional passes need no new
   MIR version/opcode. Non-scalar programs retain direct HIR→C11 or reject MIR
    with the existing unsupported-profile diagnostic.

Matching follows ordinary type/scope checking with transactional
`src/canonical/values.cpp` lowering. A scrutinee binding, explicit decision
regions/branches, checked projections, single-initialization slots and lexical
leave edges replace surface matches. Or-pattern alternatives share the arm
guard/body rather than cloning side effects. The same typed HIR and existing
MIR are used; no competing interpreter/IR is introduced.

`src/canonical/verify_values.cpp` independently checks lexical local identities,
definite initialization at joins, exactly-once slot initialization, dominating
tag tests for union payload access, projection types, completion metadata,
private-node shapes/source points and function-local target boundaries. C11/MIR
and the optional adapters invoke the verifiers again. PPX carries only additive
**source** nodes `SourceMatch`, `MatchArm`, `StructuralPattern`, `UnitValue`;
canonical regions, slots, payload proofs and target IDs are not wire constructors.
Structural patterns in old let/parameter positions reject during reconstruction.

Expression loops and value breaks add v1 source constructors `LoopValue` and
`ValueBreak`. Their syntax-context fields carry names, never resolved targets or
result/ownership proofs. Control normalization assigns expression-loop identities
and checks payload targets. After ordinary inference/checking, value lowering
uses the same HIR regions/while/initialize/leave operations as matches. Nonvoid
loops have a fatal impossible normal edge, so only initialized value exits reach
the result. Void loops preserve the conservative existing while-flow model.
The verifier also tracks possible initialization at joins and forbids initializing
enclosing single-assignment storage on a repeatable backedge or condition.

Scalar/numeric tuple matches execute through MIR v1–v3 without a format change.
Records/sums/arrays/slices and managed values still explicitly reject scalar MIR
and unimplemented independent adapters; their C11 support is not broader MIR or
SDK admission. Chisel models admitted scalar/JSON arm control using its real CFG;
fatal fallback edges are non-returning sinks, **not invented successful drops**.

This is a **canonical structured-control bridge**, not the specification's
complete typed/effectful cleanup/suspension Core. No second MIR was added and
the direct HIR→C11 backend is retained.

Bounds: existing 2 MiB input, parser 256-level and macro/PPX budgets; control
normalization/verification each have 262,144 nodes and a combined 520-level
statement/expression traversal cap. The parser bounds these depths separately,
so this preserves previously accepted deep lambda trees. Range growth is linear
   and charged. Already-canonical rechecking preserves existing AST addresses.
Matching additionally bounds arms/width at 256, pattern analysis at 262,144
work units, expanded alternatives at 4,096, syntax-pattern depth at 256 and
matrix/canonical decision traversal at 520. Value lowering/copying/verification
have 262,144-node budgets and verification has 4,194,304 flow-state work units.
Pattern analysis/planning also have 8 MiB copied-text budgets.
These limits may reject large otherwise well-typed patterns; they never authorize
partial emission or approximate coverage.

## Run and test

```sh
cmake --build build/on1x --parallel 4
python3 on1x/tools/on1x-build.py on1x/examples/statement_loops.on1x \
  --on1xc build/on1x/on1xc -o build/statement-loops
build/statement-loops
ctest --test-dir build/on1x -R 'on1x.control' --output-on-failure
```

Output: `8`, `3`, `9223372036854775807`. Add `--via-mir --optimize-ir` for the
scalar-MIR path. `--run-ir` is a bounded reference evaluator, not a native backend.

Tests cover strict C11, MIR execution/round-trip/optimization, seeded oracles,
empty/extreme ranges, steps, labels, scope/capture negatives, macro/PPX/staging/
generic compatibility, closure allocator accounting, JSON exits, malformed
HIR/wire and output preservation. Optional Chisel/Nerotypical checks exercise
generated HIR/cleanup. Exact configurations/results: `../VERIFICATION.md`.

Run `examples/structural_match.on1x` through the build helper for `42`, `8`, `4`.
Run `ctest --test-dir build/on1x -R 'on1x.pattern' --output-on-failure` for source,
wire/HIR mutations, ordered/guarded structural cases, seeded differential oracles,
closure allocator/fault injection, JSON exits and unsupported-profile negatives.

## Per-feature specification status

Statuses follow the source specification; the capability matrix separately uses
its canonical status vocabulary. No incomplete feature is marked VERIFIED.

| Requested feature | Status | Remaining boundary |
|---|---|---|
| Statement loop/for-C/labeled while/break/continue | EXECUTABLE | Conservative return analysis still requires a returning tail in value functions |
| i64 half-open/inclusive ranges | EXECUTABLE | Ascending unit step only; no unsigned/float ranges or general iterators |
| Loop values / `break with` | EXECUTABLE bounded profile | Expression-loop targets, by-value/RC results; no unique-resource transfer or general bottom type; managed/aggregate MIR remains unsupported |
| Iterable loops/comprehensions/list patterns | Not implemented | Owning lists, iterator/result/drop protocol |
| General `match with` / structural patterns | EXECUTABLE bounded profile | Includes declared nonrecursive enum catalogs; no owning lists or foreign resources; aggregate/managed MIR and SDK support remain closed |
| Function-scoped defer / nominal Result `?` | EXECUTABLE bounded C11 profile | [Go-style contract](defer-errors.md) supersedes the old lexical-defer proposal; no resource-bearing results, MIR/SDK defer models or trap/cancellation unwind |
| Using/try/catch/throw/finally | Not implemented | General owned-resource/effect/handler semantics |
| Cooperative fibers/channels/taskgroups and bounded atomic `poll` | EXECUTABLE bounded C11 profile | [Contract](concurrency.md): i64 uni channels, mandatory closed arms, no deadline; effectful ownership MIR and broader scheduling are missing |
| External process cancellation | EXECUTABLE bounded POSIX profile | Consuming `cancel xfiber` sends SIGTERM, escalates to SIGKILL and reaps; no typed cancellation result or internal-fiber unwind; [contract](concurrency.md) |
| Portable process/shell fibers | PARTIAL | Direct absolute-path exec and scoped join are executable; portable command AST and typed process streams still absent |
| Internal-fiber cancellation | EXECUTABLE pre-dispatch only | Immediately following standalone `cancel handle;` consumes and joins an unstarted fiber; running/blocked cancellation requires interruptible channel waits and generated cleanup/unwind edges |
| Async/generators/atomics/lock/shared | Not implemented | Frames, ownership and memory model |
| Named records/parameterized enums | EXECUTABLE bounded C11 profile | Explicit typed constructors, exact nominal/argument identity; no managed/recursive fields or aggregate MIR |
| Recursive ADTs/native contracts | Not implemented | Boxed ownership/drop and purity/contracts |

Unsupported operations diagnose; none are silently discarded or replaced with a
no-op. The full source-additions specification remains **incomplete**.
