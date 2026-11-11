# microjs: embeddable JavaScript core

[Manuals](README.md) · [Library index](index.md)

MicroJS compiles a JavaScript core to MuLIB's **MiniVM** stack bytecode. It has
persistent globals, functions and lexical closures, ordinary objects and arrays,
UTF-16 strings, host callbacks, and a tracing collector. It uses only MuLIB and
the C standard library (`libm` on Unix).

The target is **ECMA-262, 16th edition, ECMAScript 2025**, supplied in
`standards/ecma-262/ECMA-262.txt`. **This implementation is not fully
ECMAScript-conforming.** The conformance ledger below is part of the contract;
passing the focused regression suite does not establish full conformance.

## Build and embedding

Define both `MULIB_IMPL_MINIVM` and `MULIB_IMPL_MICROJS` in one C11 translation
unit before including `mulib-microjs.h`. Implementations can also live in
separate translation units. The application owns implementation placement;
MicroJS never silently enables another module's implementation.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/microjs.c -lm -o microjs
./microjs
```

```c
mulib_microjs_t *js = mulib_microjs_create(NULL);
mulib_microjs_value_t result = {0};
mulib_microjs_error_t error;
const char source[] =
    "function sum(n) { let s=0; for(let i=0;i<n;i++) s+=i; return s; }"
    "sum(10)";
if (js && mulib_microjs_eval(js, source, sizeof source - 1, &result, &error)
          == MULIB_MICROJS_OK) {
    /* result.type == MULIB_MICROJS_NUMBER; result.number == 45 */
}
mulib_microjs_value_free(&result);
mulib_microjs_destroy(js);
```

The complete example binds a C callback, creates a closure, iterates, and passes
a computed value back to C. Public declarations have C++ linkage guards; compile
the implementation as C11. A state requires external synchronization and does
not support concurrent mutation.

### C API

* `create` selects default limits when passed NULL. `create_with_heap` additionally
  accepts an exact managed-heap byte budget. Both return NULL on creation failure.
* `eval` borrows length-delimited UTF-8 source for the call. NULL is allowed only
  at length zero. Code retained by closures is owned by the state; the caller can
  release source immediately after evaluation.
* `get_global` returns an owned copy. `set_global` copies a borrowed input into a
  mutable binding; a constant or uninitialized existing binding rejects updates.
* `bind` creates a native function. A callback receives borrowed arguments and
  writes an **owned** result into an initially zero output. Set its type to
  `MULIB_MICROJS_UNDEFINED` for no return value. Callback user data is borrowed
  and must outlive all use of the state. A non-OK callback status aborts execution.
* `call` invokes a rooted function with borrowed arguments. Its result is owned.
* `make_object` creates an empty object or array. `get_property` and
  `set_property` copy values across the C boundary using NUL-terminated WTF-8
  property names. A missing ordinary property returns undefined. These APIs
  expose the currently implemented own-property model, not descriptors/accessors.
* `value_copy` creates another owned value, including another GC root for an
  object/function. Use it when returning a borrowed callback argument.
* `value_free` releases an owned value and clears its struct. NULL is harmless.
* `collect` reclaims unreachable cells, including cycles, and returns managed
  bytes still allocated. During execution it returns the current byte count
  without collecting.

All result outputs must be zero-initialized/empty, and are written only on
success. Never shallow-copy an owned result or edit its opaque object pointer.
Scalar input values can be constructed by filling their public fields. String
inputs are borrowed by copy-taking APIs; only pass owned strings to `value_free`.
Objects/functions from a different state are rejected.

### Ownership and strings

JavaScript strings are stored as UTF-16 code units (ECMA-262 §6.1.4), including
embedded NULs and unpaired surrogates. `.length`, indexing, comparison and equality
operate on code units. C string values use length-delimited **WTF-8**: ordinary
UTF-8 with three-byte encodings for lone surrogates. `value.length` is the byte
length, excluding the convenience terminating NUL. Use the length, not `strlen`,
when data may contain NUL. A supplementary character occupies two JavaScript
indices but four UTF-8 bytes.

String results own independent buffers. Object/function results hold explicit
collector roots. Both remain safe to release after `destroy`: a destroyed state
with outstanding object roots defers final heap release until the last root is
freed. It cannot execute again. Callbacks can use copy/get/set/make APIs; they
cannot reenter `eval` or `call`. `destroy` is a no-op during execution.

## Compiler and runtime

The frontend performs lexical analysis, builds an owned syntax graph, validates
declaration conflicts (including unreachable code), and lowers complete scripts
and function bodies before execution. A syntax error has no JavaScript side
effects. Runtime errors preserve effects already performed; execution is not a
transaction. Allocation failure during declaration instantiation may leave
bindings instantiated, as may runtime TDZ failures.

MiniVM owns the instruction dispatch, operand stack, conditional branches,
loop edges and halt/return. Stack entries are checked indices into an
execution-local table of JavaScript values and references. Runtime helpers
implement dynamic operations; **JavaScript numbers do not use MiniVM's checked
integer arithmetic**. `GetValue` operations are emitted before later operands
can mutate a reference. Short-circuit operators and conditionals use actual
bytecode branches. Closures retain their compiled bodies and lexical environments.

The nonrecursive mark/sweep collector traces environments, bindings, objects,
closures, syntax graphs and code images. Collection runs between host operations
and before/after top-level execution, never while a callback or MiniVM frame is
active. Transient values and dead loop environments count against limits until
execution finishes; long-running allocating programs can reach limits even if
some allocations are already unreachable.

## Implemented language

* Primitive values: undefined, null, Boolean, binary64 Number (including NaN,
  infinities and signed zero), String. Objects and functions have identity.
* ASCII identifiers. Unicode source strings and whitespace, single/double-quoted
  strings, Unicode/hex escapes, escaped line continuations, line/block comments.
* Decimal, exponent, leading/trailing decimal-point, hexadecimal, binary, octal
  and separated numeric literals. Legacy leading-zero syntax is not fully checked.
* Primitive coercion, strict/loose equality, relational and bitwise operators,
  arithmetic, exponentiation, unary operators, `typeof`, `void`, prefix/postfix
  updates, simple/arithmetic compound assignments and comma expressions.
* `&&`, `||`, `??` return operand values and short-circuit. Conditional expressions
  evaluate one arm. Invalid unparenthesized nullish/logical mixing is rejected.
* `let`, `const`, `var`, multiple declarators, lexical blocks, temporal dead zones,
  function-scoped `var` hoisting and function declarations. Global bindings persist.
* Ordinary function declarations/expressions, recursion, closures, simple named
  parameters, return values, property calls with `this`, and named function expressions.
* Object data-property literals, arrays with holes, property reads/writes,
  computed keys, array length growth/truncation, and UTF-16 string indexing.
* `if`/`else`, `while`, C-style `for`, unlabelled `break`/`continue`, and per-iteration
  `let` environments. `throw` reports an uncaught exception status.
* Semicolon insertion for supported productions, including return/postfix newline
  restrictions and block/EOF termination. `throw` followed by a newline is invalid.

### Selected intrinsics

The following are lazily created per state and retain identity:

* `Number`, `String`, `Boolean` conversions; `Number.isNaN`, `Number.isFinite`.
* `Object()` for an empty object or an existing object; `Object.is`.
* `Array()` / `Array(length)` / `Array(a,b,...)`; `Array.isArray`.
* Coercing global `isNaN` and `isFinite`.
* `Math.abs`, `floor`, `ceil`, `trunc`, `round`, `sqrt`, `pow`, `min`, `max`,
  `sign`, `PI`, and `E`.
* `undefined`, `NaN`, and `Infinity` resolve when not shadowed by a binding.

These are not complete constructor/prototype implementations. `Object(primitive)`
does not box primitives and returns a type error. Intrinsics are not a complete
ECMAScript global object, and their property attributes are not standardized yet.

## ECMA-262 conformance ledger

| Specification area | Current evidence / remaining work |
| --- | --- |
| §6.1.4 String | UTF-16 storage, surrogate-preserving boundary, equality, length and indexing regressions. Unicode identifier grammar still absent. |
| §6.1.6.1 Number | Division/remainder, NaN, infinities, signed zero, exponentiation and 32-bit bitwise regressions. Decimal parsing is bounded and locale-independent but not guaranteed correctly rounded for every input. Number-to-string uses `%.17g`, not the required shortest-decimal algorithm, and follows the C numeric locale. |
| §7.1–7.2 abstract operations | Primitive truthiness/conversion/equality and ordering tested. Object-to-primitive hooks, boxed primitives, Symbol and BigInt conversions are missing; object coercion is incomplete. |
| §8–10 environments/objects | Lexical scope, TDZ, closures and cyclic ownership tested. Full environment records, realms, prototypes, descriptors, accessors, extensibility, exotic objects and proxy internal methods remain. |
| §12 lexical grammar | Supported literals/comments and ASI restrictions tested. Full Unicode identifiers/escapes, RegExp lexical goals, hashbang, templates and all legacy/strict numeric restrictions remain. |
| §13 expressions | Evaluation order, primitive operators, properties, calls and assignments tested. `new`, optional chaining, spread, destructuring, `delete`, `in`, `instanceof`, logical/bitwise compound assignment and other productions remain. |
| §14 statements | Supported blocks, declarations, branching, loops, per-iteration closures and abrupt returns tested. Labels, switch, do/while, for-in/of, try/catch/finally and complete completion-record propagation remain. |
| §15 functions/classes | Ordinary simple-parameter closures tested. Strict/sloppy distinctions, global `this`, arguments objects, full declaration merging, arrows, methods, constructors, classes, rest/default parameters, generators and async functions remain. |
| §16 scripts/modules | Script compilation and persistent execution tested. Module parsing, linking, live bindings and host loading remain. |
| §18–28 built-ins | Only the enumerated intrinsic subset exists. Complete Object/Function/Array/String/Number APIs, errors, Date, RegExp, JSON, collections, iterators, typed arrays, buffers, Atomics, promises and weak references remain. |
| Annex B / host integration | Legacy web compatibility, agents/jobs, module hooks and Test262 host hooks remain. No browser/DOM APIs are provided. |

Unsupported syntax generally produces `MULIB_MICROJS_SYNTAX`; unsupported
properties behave as missing own properties. The existing strict-like unbound
assignment and function-call behavior does not implement ECMAScript's complete
strict/sloppy mode distinction. A `"use strict"` directive is currently just a
string expression. This ledger is **not** an exhaustive list of failing Test262
cases. Test262 has not been run or certified by this change.

## Limits and diagnostics

Default limits are 1 MiB source, 100000 compiler work units, 100000 runtime helper
operations, at most 100000 MiniVM instructions per invocation, depth 64, 4096
managed user bindings/properties, and 64 KiB of UTF-16 storage per string. The
same `steps` field sets the three work budgets; recursive calls share the helper
budget. The independent default managed heap is 64 MiB. Supplied limits are exact,
including zero; depth above 128 is rejected at creation. Depth bounds parsing,
compilation/static checks and function calls. Intrinsic bindings do not consume
the user binding limit; they do consume heap storage.

Managed bytes include cells, source-derived UTF-16 data, bytecode, execution
tables, callback argument arrays and reserved MiniVM stack storage. They exclude
the state struct, allocator overhead, C recursion stack, caller-owned source and
exported string buffers. Sources/code/constants are bounded by source, work and
heap limits. Compiler and value-table growth reserve both old and new arrays
before replacing them. There is no implicit unlimited mode.

Errors include invalid arguments/reentry, syntax, type, unresolved/TDZ name,
invalid array length (`ARITHMETIC`), resource limit, allocation failure, and
uncaught exception. Division by zero and numeric overflow are successful Number
operations, not API errors. Diagnostics use static messages and byte offsets with
one-based line/Unicode-code-point column. A runtime function retained from an
earlier script currently has only its original node offset; diagnostics during a
later `eval` are bounded to that later source. `call` reports no source location
beyond 1:1. Exception objects/stacks are not yet exposed.

## Verification and compatibility

`tests/test_microjs_engine.c` covers spec-derived primitive behavior, operand
evaluation order, Unicode, scopes, recursion, loop closures, objects/arrays,
callbacks, public roots, cycles, allocation failure, runtime limits and static
errors before side effects. `tests/test_language_tools.c` retains mutation and
allocation-failure coverage; `tests/test_language_refinement.c` retains long
compensated numeric literals and subnormal regressions. The MuLL fixture is
`mudsl/tests/mull/mulib/microjs.mull`.

This replaces the old finite, typed JS-like evaluator. Existing type/status enum
values and function names remain, but the public value struct has grown (rebuild
callers). Empty/declaration-only programs return undefined, numeric faults now
produce ECMAScript Number values, operators coerce primitives, logical operators
return operands, strings use UTF-16 semantics, redeclarations are syntax errors,
and the whole source is compiled before side effects. Define MiniVM's
implementation in existing standalone integrations.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-microjs.h`](../include/mulib-microjs.h) |
| Implementation switch | `MULIB_IMPL_MICROJS` |
| Repository-local includes | [mulib-minivm](mulib-minivm.md) |
| Complete example | [`examples/modules/microjs.c`](../examples/modules/microjs.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/microjs.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
