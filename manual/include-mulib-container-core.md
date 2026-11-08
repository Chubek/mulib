# include-mulib-container-core

[Manuals](README.md) · [Library index](index.md)

## NAME

`include/include-mulib-container-core.h` — checked storage, wire integers and
shared result codes for the container stack.

## SYNOPSIS

```c
#include "include-mulib-container-core.h"
mulib_ct_blob bytes = {0};
int rc = mulib_ct_blob_append(&bytes, "hello", 5, 1024);
mulib_ct_blob_free(&bytes);
```

All definitions are `static inline`; there is no implementation switch or link
dependency. The `include-mulib-` dependency filename is intentional. A standalone
example is `examples/modules/container-core.c`.

## STORAGE

`mulib_ct_blob` owns `data`, with logical `length` and allocated `capacity`.
`reserve` grows to at least the requested capacity while preserving length.
`append` uses bounded geometric growth and requires nonaliasing input.
`assign` allocates/copies before releasing the old blob, so source ranges may
alias old storage. Empty assignments reset the blob. `free` is NULL-safe and
idempotent. A limit bounds requested logical/allocated growth; lowering it does
not shrink already allocated storage.

Allocation failure and limit errors preserve the previous blob. Construction
uses libc unless all three `MULIB_CT_MALLOC`, `MULIB_CT_REALLOC`, and
`MULIB_CT_FREE` macros select compatible allocators. Keep one allocator
configuration across allocation, transfer and destruction of each object.

## VALIDATION AND WIRE HELPERS

`text_length` checks for a terminating NUL within maximum+1 readable bytes.
`text_copy` copies a bounded string, including its terminator, with overlap
support. `id_valid` accepts 1–63 ASCII letters/digits/`._-`, except `.` and `..`.
Pointers must refer to accessible memory; these helpers are not pointer probes.

`add_size` rejects `size_t` overflow. `load32/load64/store32/store64` access
unaligned little-endian wire integers; the caller supplies a sufficient range.
`counter_add` saturates at UINT64_MAX for statistics only.

## RESULTS

`OK=0`, `AGAIN=1`, `DONE=2`. Negative codes cover invalid input, allocation,
limits, malformed input, lifecycle state, missing/duplicate objects, unsupported
operations, backend I/O, access, busy resources, stale generations, cancellation
and checksum mismatch. `strerror` returns immutable static descriptions.

See [container-stack](container-stack.md) for cross-module ownership and error families.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/include-mulib-container-core.h`](../include/include-mulib-container-core.h) |
| Implementation switch | None (support header; see scope above) |
| Repository-local includes | None |
| Complete example | [`examples/modules/container-core.c`](../examples/modules/container-core.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/container-core.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
