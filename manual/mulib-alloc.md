# mulib-alloc

[Manuals](README.md) · [Library index](index.md)

Allocator callbacks, overflow-checked zero allocation, string copying, and
transactional array resizing. Define `MULIB_IMPL_ALLOC` and
`MULIB_IMPL_PORT` in one C11 implementation translation unit.

## Contract

`mulib_allocator_t` contains a context pointer and allocate/reallocate/deallocate
callbacks. The struct can be copied; context and callback lifetimes remain the
caller's responsibility. `allocate` and `deallocate` are required; `reallocate`
may be NULL. Callbacks must provide `max_align_t` alignment and preserve the
original allocation when nonzero reallocation fails. A NULL allocator selects
libc. `mulib_alloc_default()` returns explicit libc callbacks.

- `mulib_alloc(a,n)` returns owned memory, or NULL for failure/zero size.
- `mulib_alloc_calloc(a,count,stride)` checks multiplication and zeroes bytes.
- `mulib_alloc_realloc(a,p,n)` frees on zero, allocates if p is NULL, otherwise
  requires a realloc callback. Failure does not free p.
- `mulib_alloc_free(a,p)` accepts NULL. Always use the allocator that created p.
- `mulib_alloc_resize_array(a,&p,count,stride)` changes p only on success;
  zero count frees and NULLs p. Nonzero count with zero stride is invalid.
- `mulib_alloc_strdup_n(a,s,n)` copies arbitrary bytes and adds a trailing NUL;
  NULL s is permitted for zero n. Free the returned pointer with a.

No process-global allocator is changed. Thread safety is that of callbacks.
Unknown pointers, mismatched allocators, and concurrent unsynchronized access
to the same allocation are caller errors.

## Example

```c
#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#include <mulib-alloc.h>
#include <assert.h>
int main(void) {
    void *p = NULL;
    assert(mulib_alloc_resize_array(NULL, &p, 32, sizeof(double)));
    mulib_alloc_free(NULL, p);
    return 0;
}
```

Runnable example: `examples/modules/alloc.c`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-alloc.h`](../include/mulib-alloc.h) |
| Implementation switch | `MULIB_IMPL_ALLOC` |
| Repository-local includes | [mulib-port](mulib-port.md) |
| Complete example | [`examples/modules/alloc.c`](../examples/modules/alloc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/alloc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
