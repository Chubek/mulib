# Chisel resource-usage checking

The additional [immutable string FFI profile](ffi-strings.md) uses directional
v4 admission evidence for call-scoped immutable inputs and process-lifetime
immutable results. Legacy evidence cannot authorize strings. This extends
use-site admission, not Chisel's ability to prove foreign implementations safe.

On1x uses the real `Moonstone::Chisel` target and
`moonstone::chisel::analyze` from the supplied LibStone checkout. The integration
checks modeled client-side ownership and borrow lifetimes. It does not inspect
foreign function bodies or make arbitrary C implementations memory-safe.

## Build and run

```sh
cmake -S on1x -B /tmp/opencode/on1x-memory \
  -DON1X_WITH_CHISEL=ON -DBUILD_TESTING=ON
cmake --build /tmp/opencode/on1x-memory --parallel 4
ctest --test-dir /tmp/opencode/on1x-memory --output-on-failure
/tmp/opencode/on1x-memory/on1xc --check-memory on1x/examples/memory.on1x
python3 on1x/tools/on1x-build.py on1x/examples/memory.on1x --memory-check \
  --on1xc /tmp/opencode/on1x-memory/on1xc -o /tmp/opencode/memory-example
/tmp/opencode/memory-example
```

The example prints `2, 1, 3, 3, 3` on separate lines. `--check-memory` reports
deterministic function/block/event counts. `--memory-check` requires successful
admission before ordinary source checking/emission/execution continues. A failed
check leaves existing output intact. Input/ownership rejection returns 1;
unsupported flag combinations or a disabled feature return 2; budget exhaustion
returns 75. Inspection modes and saved MIR do not accept this HIR check.

The CMake option defaults OFF and is independent of ExoML, Novostone and scanner
selection. It requires the supplied source SDK; installed-package consumption is
not implemented here. ExoML and Chisel share one Moonstone subdirectory when both
are enabled. SDK includes are system includes; On1x's own warnings remain errors.
Generated C11 programs link only their existing runtime and selected kernels.
No Chisel, GC, MemTKX or C++ dependency is added to those programs.

## Executable HIR subset

| Supported | Model |
|---|---|
| Scalar/string literals, locals, operators | No ownership transfer |
| Numeric scalar literals, parameters, arithmetic and numeric/bit/classification intrinsics | Trivially copied i64/u8/u16/u32/u64/f32/f64; no resource events; checked numeric runtime semantics |
| Nested numeric/bool tuples and shared destructuring patterns | Trivially copied fields only; no owner/loan crosses the aggregate; bounded 64-level memory-model traversal |
| Direct ordinary calls, including recursion | All concrete function bodies checked; no resource arguments/results |
| Blocks, branches, loops, early returns | Explicit CFG and lexical cleanup on each normal exit |
| `json_parse_or_die` in an immutable local | Unique owner creation on successful continuation |
| `json_get_or_die` from a named owner/view | Shared loan of the original root, including nested views |
| `json_size`, `json_number`, `json_print` | Use through a live owner or active shared loan |
| Scope exit / return | End loans and drop owners in reverse lexical order |
| `println`, `assert`, checked arithmetic | Existing scalar/terminal-failure semantics |

Intrinsic scalar/numeric-tuple matches now use checked decision/initialization
regions and the same public Chisel CFG. Arm-local JSON creates/loans/cleanup and
return/break/continue edges are modeled, including lexical exits nested inside
value expressions and short-circuit paths. A fatal unmatched decision is a
non-returning sink, not a normal exit with invented drops. Sum/record/array/slice
and managed-value patterns remain outside this adapter; C11 support does not
claim Chisel proof for them.

The JSON operations are the existing **trusted handwritten facade** over canonical
MuLIB. Its implementation and C11 cleanup are tested independently; Chisel checks
the client protocol. JSON mutation is not exposed by this facade. Fatal `or_die`
and arithmetic failures terminate the process; general error unwinding/cleanup
is not claimed. Short-circuit operands cannot change admitted ownership state;
both possible resource-use paths are checked conservatively.

Raw external declarations/calls reject unless they have exact reviewed
scalar-only admission evidence as described below. Indirect calls, closures,
non-numeric tuples, staged/managed values and unmodeled intrinsics reject in checked mode. Existing frontend rules
also reject copying/reassigning resources, resource capture/return, and dangling
temporary borrows. Checking is additional admission, not permission to bypass
those rules. Syntax and the default compiler path are preserved.

## Reviewed scalar-only external evidence

`on1x-bind emit-safe` emits a bounded `on1x.admitted-extern/v1`, `/v2` or `/v3` companion after
all four [safe-admission gates](contracts.md#reviewed-scalar-only-safe-admission)
pass. Checked consumers opt into that exact evidence:

```sh
on1xc --check-memory --admitted-extern bindings.admitted-extern.txt app.on1x
on1xc --emit-c --memory-check \
  --admitted-extern bindings.admitted-extern.txt app.on1x -o app.c
```

Each entry names an external function and its exact scalar shape. v1 allows
only `i64` parameters and `i64`/`void` results. v2 additionally allows `bool` and
`f64` parameters/results; v3 additionally allows `u8/u16/u32/u64/f32`.
`void` parameters and all other types reject. Chisel treats
such calls as scalar operations because no pointer or owned/borrowed resource can
cross this profile. Source declarations and callsites must agree in name, arity,
each parameter type and result;
unused/absent entries, malformed/duplicate evidence and every external omitted
from evidence reject. `--admitted-extern` is invalid without a memory-checking
mode. It is contract-derived build evidence, not a signature, proof of a C body,
or permission to admit a pointer/resource API.

For example, a v2 entry may read `extern on1x_ffi_<fingerprint>_scale
(bool,f64,i64) -> f64` on one line, following the schema and `identity` lines.
The full 64-hex-digit CDecl fingerprint replaces `<fingerprint>` in both the
identity and symbol. Integer-only v1 evidence retains its original meaning;
changing only its schema does not authorize differently typed source calls.

## Other external libraries: resource protocol API

Link the compiler-side `on1x_safety` target and include `on1x/safety.hpp`.
`safety::verify(chisel_function, limits)` consumes Chisel's actual public CFG;
`examples/external-memory.cpp` is a compiled/runnable example. A binding adapter
can model an external handle using these events:

| Contract operation | Chisel event |
|---|---|
| Acquire ownership after successful construction | `Create` |
| Read through owner | `Use` with token zero |
| Obtain borrowed view | `BorrowShared` or `BorrowUnique` with a unique static token |
| Read through that view | `Use` with the matching nonzero token |
| End the view's lifetime | `EndBorrow` |
| Invalidate/mutate storage (e.g. vector growth) | `Mutate` after conflicting loans end |
| Transfer ownership to external code | `Move` on the transfer-success edge |
| Release through the descriptor's destructor | `Drop` |
| Reuse a dead resource place | `Reinitialize` |

Use separate CFG successors for success/failure ownership outcomes. The API
checks resource events, not the C call's ABI, destructor identity, allocation
extent, pointer arithmetic or truth of a supplied contract. External adapters
must derive these events from the full admitted ExoML descriptor and actual
callsite; an independently supplied event trace cannot certify an On1x program.
There is no CLI that imports arbitrary traces as proof. General external-handle
types and their On1x ABI lowering remain future work; `emit-safe` rejects them
and admits only the scalar-only profile above.

The event API is library-independent: modeled MuLIB growth/invalidation and
non-MuLIB acquire/borrow/transfer/release follow the same rules. It supports
exclusive loans for reads; mutation through an exclusive token is not yet
modeled. Escape and unknown operations reject. A normal exit must have no live
owned resources or active loans. A `Move` means the admitted callee assumes
ownership, not that the verifier discovered a matching deallocator.

## Strengthening the upstream checker

Chisel's current checker is small and has conservative/limited joins. On1x adds
admission checks rather than modifying the dependency:

1. Reject malformed/unreachable CFGs, entry backedges, unknown operations,
   invalid event identities, and multiple static definitions of a loan token.
2. Split events into single-instruction blocks and run the real Chisel solver.
   Nonconvergence is exhaustion, never successful checking.
3. Require equal incoming loan maps at joins. Chisel's union of tokens alone
   does not prove a loan active on every incoming path.
4. Check token-bearing uses against the converged active loan and its owner.
   Reject direct owner access during an exclusive loan.
5. Run an independent monotone may-live set analysis to reject potentially-live
   reinitialization and normal-exit leaks. This closes the upstream permissive
   `MaybeInvalid` reinitialization case without inventing a second borrow solver.

These restrictions can reject safe programs with path-dependent loans; they do
not silently widen admission. Source file/line/byte-column points survive mapping;
Chisel's legacy `SourceSpan` does not carry On1x byte offsets or expansion stacks.
Structural CFG errors use a synthetic CFG diagnostic point.

Default bounds are 4,096 nodes and 4,194,304 work units. HIR lowering has a shared
unit budget; verification uses independent per-function limits. The work limit
reserves a conservative state-size-weighted solver/exit-analysis envelope, not a
wall-clock or measured-step claim. API callers can lower both limits. No incomplete
report or executable artifact is published on failure. The report is informational
and is never accepted as a cached admission certificate.

## Verification

`on1x.safety_hir` covers use-after-drop, double drop, borrowed move/drop, conflicting
mutation, exclusive-owner access, expired-token use, conditional loans/transfers,
possibly-live reinitialization, leaks, loops, malformed graphs and independent
budgets. `on1x.safety` checks actual JSON execution, nested loans, early cleanup,
loops, deterministic output and closed rejection. The external protocol example
has its own CTest. Existing JSON allocator-failure/runtime tests also run in the
full suites. See [measured builds](../VERIFICATION.md).
