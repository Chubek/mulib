# record — owned named fields

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_RECORD` and
`MULIB_IMPL_DATUM`.

Set requires a nonempty NUL-terminated field name and a valid datum. It copies
both name and value, replacing matching names in place or appending new fields.
Return 1 on success, 0 on invalid input/allocation failure without changing
contents. A source value may belong to this record. Get returns a borrowed
datum or NULL; subsequent record mutation/free can invalidate it. Clone replaces
an initialized destination atomically with a deep copy; self-clone succeeds.
Names compare byte-exactly. Lookup/set are linear in fields; free recursively
releases values and names. Embedded NUL names are not supported.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-record.h`](../include/mulib-record.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Owned named record fields with deep-copied datum values. C11 single-header;
 * define MULIB_IMPL_RECORD once. */
#ifndef MULIB_RECORD_H
#define MULIB_RECORD_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-datum.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    char *name;
    mulib_datum_t value;
  } mulib_record_field_t;
  typedef struct
  {
    mulib_record_field_t *fields;
    size_t count, capacity;
  } mulib_record_t;
  const mulib_datum_t *mulib_record_get (const mulib_record_t *r, const char *name);
  int mulib_record_set (mulib_record_t *r, const char *name,
                        const mulib_datum_t *value);
  int mulib_record_clone (mulib_record_t *dst, const mulib_record_t *src);
  void mulib_record_free (mulib_record_t *r);
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
| Header | [`include/mulib-record.h`](../include/mulib-record.h) |
| Implementation switch | `MULIB_IMPL_RECORD` |
| Repository-local includes | [mulib-datum](mulib-datum.md) |
| Complete example | [`examples/modules/record.c`](../examples/modules/record.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/record.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
