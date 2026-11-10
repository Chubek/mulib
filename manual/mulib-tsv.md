# tsv — borrowed tab-separated fields

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_TSV`.

Init borrows one explicit row byte range; NULL becomes an empty row. Next
returns 1 with a borrowed slice, or 0 for exhausted/invalid calls. Leading,
consecutive and trailing tabs preserve empty fields; an empty row yields one
empty field (possibly NULL pointer with zero length). Slices are not terminated.
No quoting, escaping, newline trimming or type conversion is performed; pass
one row without its line ending. Source must remain alive and unchanged.
Total iteration is O(row bytes), allocation-free.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-tsv.h`](../include/mulib-tsv.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Borrowed tab-separated field iterator preserving empty fields. C11
 * single-header; define MULIB_IMPL_TSV once. */
#ifndef MULIB_TSV_H
#define MULIB_TSV_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    const char *row;
    size_t length, pos;
    int done;
  } mulib_tsv_t;
  void mulib_tsv_init (mulib_tsv_t *t, const char *row, size_t length);
  int mulib_tsv_next (mulib_tsv_t *t, const char **field, size_t *field_length);
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
| Header | [`include/mulib-tsv.h`](../include/mulib-tsv.h) |
| Implementation switch | `MULIB_IMPL_TSV` |
| Repository-local includes | None |
| Complete example | [`examples/modules/tsv.c`](../examples/modules/tsv.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tsv.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
