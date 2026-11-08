# mulib-xatomics

[Manuals](README.md) · [Library index](index.md)

Portable atomic unsigned integers and pointers with a deliberately uniform
**sequentially consistent** memory model. Define `MULIB_IMPL_XATOMICS` in one
translation unit and include `mulib-xatomics.h`. No other MuLIB module is needed.

## Types and operations

- `mulib_xatomic_u32_t`: exact-width unsigned 32-bit integer.
- `mulib_xatomic_u64_t`: exact-width unsigned 64-bit integer, eight-byte aligned.
- `mulib_xatomic_ptr_t`: `void *` pointer, without pointee ownership.

Initialize before publication using `MULIB_XATOMIC_INIT(value)`. For example:

```c
mulib_xatomic_u64_t counter = MULIB_XATOMIC_INIT(0);
uint64_t old = mulib_xatomic_u64_fetch_add(&counter, 1);
```

Each integer type supports `load`, `store`, `exchange`, `compare_exchange`, and
`fetch_add/sub/and/or/xor`, using the `mulib_xatomic_u32_` or
`mulib_xatomic_u64_` prefix. Fetch operations and exchange return the previous
value. Arithmetic wraps modulo the unsigned width. Strong compare/exchange
returns 1 on success; on mismatch it returns 0 and replaces `*expected` with
the actual value. It does not fail spuriously. Expected storage is caller-owned,
non-atomic and must not race with other accesses.

Pointers support load/store/exchange/compare_exchange with the
`mulib_xatomic_ptr_` prefix. `mulib_xatomic_fence()` is a sequentially consistent
thread fence. Pointer publication does not solve reclamation or ABA: keep the
pointee alive while readers can use it.

## Portability and lifecycle

GCC and Clang use their `__atomic` intrinsics. MSVC uses Interlocked operations
and strong-CAS loops. On architectures where compiler atomic operations require
runtime support, link `libatomic` (`-latomic`). No lock-free guarantee is made;
the API remains atomic even when a compiler uses a runtime lock. These are not
intended as async-signal-safe operations.

Do not inspect the public storage fields directly, mix atomic and ordinary
accesses, pack these structs, or copy/move a live atomic object. Stack/static
objects and normally aligned allocations are supported; over-aligned storage
must preserve the type's alignment on the target. NULL object/expected pointers
are invalid. No allocation occurs in this module itself. There is no destroy
operation: end the object's lifetime only after all users finish.

All accesses are seq_cst; relaxed/acquire/release order parameters are deliberately
absent. Public declarations work in C and C++. Compile implementation as C11.

See [`examples/modules/xatomics.c`](../examples/modules/xatomics.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-xatomics.h`](../include/mulib-xatomics.h) |
| Implementation switch | `MULIB_IMPL_XATOMICS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/xatomics.c`](../examples/modules/xatomics.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/xatomics.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
