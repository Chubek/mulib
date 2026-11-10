# datum — owned scalar values

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_DATUM`.

Zero state represents NULL. `init` is for fresh storage; `free` resets an existing
value. Setters replace an initialized destination; bytes are copied without a
terminating NUL and may contain NUL. Empty bytes allow NULL input. Byte setters
and clone return 0 on invalid input/allocation failure without changing the
destination, and 1 on success. Input bytes may alias the old value. Clone is deep
for bytes, scalar for other kinds, and self-clone succeeds. F64 accepts IEEE
nonfinite values; queryexpr treats NaN as incomparable. BOOL may be initialized
with a canonical 0/1 scalar value. Do not assign owned byte datums by value.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-datum.h`](../include/mulib-datum.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Owned typed scalar values with binary-safe strings, deep-copy semantics and
 * NULL. C11 single-header; define MULIB_IMPL_DATUM once. */
#ifndef MULIB_DATUM_H
#define MULIB_DATUM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    MULIB_DATUM_NULL,
    MULIB_DATUM_I64,
    MULIB_DATUM_F64,
    MULIB_DATUM_BOOL,
    MULIB_DATUM_BYTES
  } mulib_datum_kind_t;
  typedef struct
  {
    mulib_datum_kind_t kind;
    union
    {
      int64_t integer;
      double real;
      int boolean;
      struct
      {
        unsigned char *data;
        size_t length;
      } bytes;
    } value;
  } mulib_datum_t;
  void mulib_datum_init (mulib_datum_t *v);
  int mulib_datum_set_i64 (mulib_datum_t *v, int64_t x);
  int mulib_datum_set_f64 (mulib_datum_t *v, double x);
  int mulib_datum_set_bytes (mulib_datum_t *v, const void *bytes, size_t len);
  int mulib_datum_clone (mulib_datum_t *dst, const mulib_datum_t *src);
  void mulib_datum_free (mulib_datum_t *v);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-datum.h`](../include/mulib-datum.h) |
| Implementation switch | `MULIB_IMPL_DATUM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/datum.c`](../examples/modules/datum.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/datum.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
