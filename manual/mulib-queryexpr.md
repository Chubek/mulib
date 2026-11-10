# queryexpr — typed scalar comparison

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_QUERYEXPR`.
Enable `MULIB_IMPL_DATUM` when using datum constructors/destructors.

Compare returns -1, 0 or 1 for less/equal/greater, and 2 for incomparable
(NULL pointers, NULL values, mixed types, unknown types or NaN). No numeric
coercion occurs. I64 and canonical BOOL compare numerically; F64 supports
infinities and treats signed zeros as equal. Bytes compare unsigned
lexicographically, using length to break equal-prefix ties, including NUL bytes.
Inputs are borrowed valid datums. No allocation; bytes cost O(common prefix),
other types O(1). NULL is not equal to NULL under this predicate convention.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-queryexpr.h`](../include/mulib-queryexpr.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Typed equality and ordering of MuLIB datum scalar values. C11 single-header;
 * define MULIB_IMPL_QUERYEXPR once. */
#ifndef MULIB_QUERYEXPR_H
#define MULIB_QUERYEXPR_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-datum.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /* -1 less, 0 equal, 1 greater, 2 incomparable (e.g. NULL or mixed types). */
  int mulib_queryexpr_compare (const mulib_datum_t *a, const mulib_datum_t *b);
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
| Header | [`include/mulib-queryexpr.h`](../include/mulib-queryexpr.h) |
| Implementation switch | `MULIB_IMPL_QUERYEXPR` |
| Repository-local includes | [mulib-datum](mulib-datum.md) |
| Complete example | [`examples/modules/queryexpr.c`](../examples/modules/queryexpr.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/queryexpr.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
