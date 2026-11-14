# Closures: lowering, ownership and runtime manual

## Supported surface

`fn(parameters) [-> type] { statements }` is an expression with the same
structural, monomorphic function type as an ordinary function. It supports
omitted parameter annotations, `_` holes, explicit returns, nesting and postfix
calls. An omitted result annotation is `void`. See [language semantics](language.md)
and the runnable `examples/closures.on1x`.

Free immutable locals/parameters are captured by value. Nested functions forward
transitive captures, preserving lexical shadowing. Captured function values and
functions nested in captured tuples keep
their environments alive. Mutable bindings and JSON owners/views cannot be
captured; make an explicit immutable scalar/function/tuple snapshot where appropriate.
There is no implicit partial application, local `let rec`, capture mutation,
weak reference, unrestricted higher-rank inference, tracing collector or cross-thread handoff.
Named immutable value bindings can use [local let schemes](polymorphism.md),
with concrete closure specializations at the original declaration point.
Explicit `forall` values/parameters use typed bundles with recursive field
ownership; immutable closures can retain these bundles without runtime type erasure.

## Internal C11 representation (version 1)

For each distinct structural signature, emitted C defines a dependency-ordered
type equivalent to:

```c
typedef struct function_i64_to_i64 {
    int64_t (*invoke)(on1x_env *, int64_t);
    on1x_env *environment;
} function_i64_to_i64;
```

The real generated names are deterministic and avoid raw extern declarations.
Code pointers always refer to static-lifetime functions. Top-level functions
use exact-signature adapters with null environments; noncapturing literals use
lifted code with null environments. Capturing literals have a generated factory,
a concrete environment record with the common header as its first member,
and a generated capture-field destructor. A C11 `_Static_assert` checks header
offset zero. All capture fields use their checked concrete C types.

The invocation pointer is never converted to/from `void *`, nor treated as a
foreign C callback. `ON1X_CLOSURE_ABI_VERSION` in `runtime/on1x_runtime.h` must
match the generated-code version; an incompatible header is a compile error.
This is **not** a stable binary/extension ABI. Rebuild generated C and the
runtime together after upgrading the compiler.

## Ownership contract

| Operation | Environment reference behavior |
|---|---|
| Top-level/noncapturing function value | Null environment, no allocation |
| Capturing factory result | Owns a new environment reference |
| Captured function field | Retains its environment until field destruction |
| Function parameter / invocation context | Borrows for the duration of the call |
| Function-valued result | Caller owns one reference |
| `let`/`var` function local | Owns a reference; releases at lexical exit/return |
| Reading a function expression | Retained temporary, or newly owned call/factory result |
| Function assignment | Retains replacement before releasing old reference |
| Discarded/full-expression temporary | Releases when no longer needed |
| Tuple value containing functions | Applies the same ownership rules recursively to its fields |

The callee is materialized before arguments. Arguments evaluate left-to-right;
their owned function temporaries survive until invocation finishes. Short-
circuit operands and completed conditions clean up only their own temporaries,
not previously evaluated enclosing operands. Loop-condition temporaries release
on **each** test, including the final false one. Return values retain before
local cleanup; function parameters themselves are not dropped by the callee.
Tuple locals/results/captures follow the same contract recursively; the tuple
record itself needs no allocation. Generated pointer-based field helpers avoid
aggregate copies while retaining/releasing nested environments. See the
[tuple manual](tuples.md) for destructuring and product-type details.

Environments contain only immutable values and references to existing function
values. Since bindings are nonrecursive and captured fields cannot be changed,
the generated reference graph is acyclic. Mutable function locals may hold
closures but cannot themselves be captured. These restrictions, not a tracing
collector, prevent cycles in this subset.

JSON lexical cleanup is unchanged. A lambda may create local roots/views and
drop roots on normal exit/return, but cannot store a root/view in its environment
or its signature. String fields copy `const char *`, not the character buffer:
literal-derived strings remain static; foreign strings require the raw caller's
lifetime discipline. No ownership/ABI proof for raw foreign calls is implied.

## Runtime entry points

The internal header provides:

- `on1x_env_alloc(size, destroy)`: allocates a header/record with count 1.
- `on1x_env_retain(env)`: increments the count; null is a no-op.
- `on1x_env_release(env)`: decrements; null is a no-op. Zero-count records queue
  for field destruction and freeing. Nested field releases join the queue, so
  deep destruction does not consume recursive C stack space.

Generated `destroy` callbacks release fields only; they must not free their
header or execute arbitrary language code. Contexts, counts and the draining
queue are **single-threaded**, not atomic. Thread migration/concurrent use,
foreign retention of these values and arbitrary native destructors are outside
this contract.

`ON1X_ENV_MALLOC` / `ON1X_ENV_FREE` optionally replace the matching allocation
pair when compiling `on1x_runtime.c`. They must preserve C `malloc` alignment
and allocation/free semantics. Defaults are C `malloc`/`free`; tests inject
counting/failing hooks through a declaration header.

Allocation failure, invalid allocation size and reference overflow/underflow
print `On1x closure error: ...` and terminate with status **72**. These are fatal
operations, not `Result` values or unwinding points. Live locals are not
guaranteed to clean up on any fatal error, including existing arithmetic/assert
and JSON `or_die` failures.

## Compiler bounds and verification

Existing 2 MiB source and 256-level parser/expression bounds include lambda
bodies. Each source file permits 2,048 anonymous functions. A checked unit,
including imports, permits at most 16,384 capture entries and 2 MiB of copied
capture names. These budgets bound transitive capture amplification separately
from the inference graph/depth/expanded-type budgets.

`on1x.closure_hir` checks grounded signatures, first-use/transitive capture
metadata, shadowing and repeated checking. `on1x.closures` compiles strict C11
native programs, counts live environments, checks release timing/self-assignment,
rejects unsupported semantics/ABIs, injects allocation failure, exercises runtime
guards, and releases 50,000 generated / 100,000 isolated runtime environments
without recursive destruction. `on1x.limits` checks the metadata and nesting
boundaries. See [verification](../VERIFICATION.md) for executed configurations;
none of this claims generalized foreign lifetime or thread-safety analysis.
