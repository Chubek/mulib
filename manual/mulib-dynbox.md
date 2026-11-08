# Native dynamic calls and closures

[Manuals](README.md) · [Library index](index.md)

**Header:** `mulib-dynbox.h`; define **`MULIB_IMPL_DYNBOX`** in exactly one C11
implementation TU. No MuLIB or third-party dependencies; no libffi/dyncall link.
Declaration-only use is also supported from C++17.
[Runnable example](../examples/modules/dynbox.c).

## Native ABI profile

The original backend implements **Linux x86-64 LP64 System V** runtime-typed
calls and callbacks. `mulib_dynbox_supported()` reports build support; preparing
code can still fail if the OS forbids executable mappings. Other platforms
return `MULIB_DYNBOX_ENOTSUP`, rather than attempting a wrong calling convention.
This is libffi-style functionality, **not all of libffi's platforms or types**.

Supported values: void returns, signed/unsigned 8/16/32/64-bit integers, native
long, float, double, 80-bit x87 long double in 16-byte native storage, pointers,
and recursively nested naturally aligned C structs. Struct fields use ordinary
C padding; arrays can be represented by repeated fields. INTEGER/SSE eightbyte
classification, register exhaustion/whole-aggregate rollback, aligned stack
arguments, hidden structure-result pointers, mixed register returns, and x87
scalar/single-long-double aggregate returns are implemented. Variadic calls set
the SysV vector-register count and accept promoted tail arguments.

No packed structs, bitfields, unions, complex numbers, vectors, non-SysV ABIs,
C++ nontrivial objects, variadic callback closures, exception unwinding through
generated code, or asynchronous-signal-safety guarantee. Use exact native
prototypes: a mismatched descriptor/address is undefined behavior, not an input
validation error the library can detect. Native code and callbacks are trusted.

## Descriptors and ownership

Use immutable `mulib_dynbox_type_*` built-ins. `mulib_dynbox_struct_init` builds a
struct descriptor, optionally returning each field's offset; compare with
`sizeof`, `_Alignof`, and `offsetof` when binding an existing C type. Its field
array and nested descriptors are borrowed until preparation. Invalid/cyclic or
inconsistent descriptors fail without modifying output or offsets.

`mulib_dynbox_prepare(&cif, result_type, count, argument_types)` requires an
initially NULL output. `prepare_variadic` adds the fixed-argument count, which
must be between one and total count. C default promotions are the caller's job:
float tails must be double; 8/16-bit integer tails must be native int, represented
here by `sint32`. Small unsigned tails also promote to int on this platform.
Unpromoted descriptors are rejected, not silently converted.

Prepared interfaces own layouts and executable bridges. Source descriptors may
then be released. `argument_count` and `result_size` expose the prepared shape.
`call(cif, function, result, capacity, arguments)` takes one pointer per exact
native argument. A pointer argument needs the **address of a pointer value**.
Void results allow NULL; other results need at least `result_size` bytes. Small
integer results are written at their exact width, with no libffi-style machine
word widening. Native argument values are copied before calling, allowing result
storage to overlap an argument. Validation/allocation failures preserve result;
native execution and its effects cannot be rolled back.

`mulib_dynbox_fn` is an erased function-pointer representation, not a callable
prototype. C permits conversion from another function-pointer type; for loader
symbols use `mulib_dynload_function` and its documented host-ABI conversion.

## Closures

`closure_create(&closure, cif, callback, user)` makes a native entry for a
nonvariadic CIF. The CIF and user state are borrowed. `closure_function` returns
an address callable with exactly that CIF's native prototype; copy/convert it to
that function-pointer type before use. The callback receives aligned argument
copies, a zero-initialized result object (NULL for void), and user state. All
argument/result pointers expire on return. Fill the complete result value.

Callbacks use bounded per-invocation native stack scratch, not heap allocation or
shared scratch. They can recurse and reenter the same interface. Independent
calls and closures are thread-safe if the target/callback/user data are safe.
Do not mutate/free the CIF, closure, or user state while a call is active; stop
all external users before destroying a closure. Free closures before their CIF.
Destroy functions return a status and retain ownership on native unmap failure.

## Limits and executable memory

Limits are 1024 arguments, 1024 fields per struct, nesting depth 32, 65536
descriptor visits per preparation/layout operation, and 65536 bytes of combined
argument/result scratch. Stack arguments are independently bounded by 65536
bytes. These bounds are checked before code execution. Callers must provide
adequate native thread stacks for callbacks and recursion.

Bridges have an ENDBR64 entry and are generated in private **RW, non-executable**
memory, then sealed **RX, non-writable**. No RWX interval, global code cache or
compiler invocation is used. Native frames probe stack pages while growing;
x86's coherent instruction cache is relied on.
Generated frames have no unwind metadata and are not a sandbox.

Zero means success; negative `EINVAL`, `ENOMEM`, `ENOTSUP`, `ERANGE`, and `EOS`
distinguish validation, allocation, ABI, limit, and native mapping failures.
NULL destroy is harmless. The focused suite exercises compiler-ABI calls,
variadics, aggregate rollback, structure/x87 returns, qsort callbacks, stack
callbacks, and allocation failures.

Build: `cc -std=c11 -Iinclude examples/modules/dynbox.c -o dynbox-example`.

`python3 tools/check_dynbox_abi.py` generates 100 deterministic native C
signatures, compares calls and closures with compiler-generated calls, and loads
a separately compiled shared-library fixture. It exercises separate implementation
and client TUs without libffi. Use `--compiler clang --sanitizers` for ASan/UBSan.
The tool is also registered with CTest and the repository checker.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dynbox.h`](../include/mulib-dynbox.h) |
| Implementation switch | `MULIB_IMPL_DYNBOX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/dynbox.c`](../examples/modules/dynbox.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dynbox.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
