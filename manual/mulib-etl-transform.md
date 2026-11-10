# etl-transform — record projection callbacks

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_ETL_TRANSFORM`,
`MULIB_IMPL_RECORD`, and `MULIB_IMPL_DATUM`.

Apply invokes the callback with a borrowed input and a fresh empty temporary
output. Callback returns 1 to publish, 0 to skip, or a negative error. Only 1
replaces the caller's initialized output; otherwise temporary fields are freed
and the old output survives. Invalid arguments or input/output identity return
-1. The callback must create independently owned fields (use record setters or
clone), not shallow-copy the input. Callback work determines complexity; the
wrapper manages ownership, not a parser, scheduler, or stream source.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-etl-transform.h`](../include/mulib-etl-transform.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Composable pull-based ETL record filtering and transformation callbacks. C11
 * single-header; define MULIB_IMPL_ETL_TRANSFORM once. */
#ifndef MULIB_ETL_TRANSFORM_H
#define MULIB_ETL_TRANSFORM_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-record.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef int (*mulib_etl_transform_fn) (const mulib_record_t *input,
                                         mulib_record_t *output, void *context);
  /* transform returns 1 output, 0 skip, negative error; output is zero initialized. */
  int mulib_etl_transform_apply (const mulib_record_t *in, mulib_record_t *out,
                                 mulib_etl_transform_fn transform, void *ctx);
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
| Header | [`include/mulib-etl-transform.h`](../include/mulib-etl-transform.h) |
| Implementation switch | `MULIB_IMPL_ETL_TRANSFORM` |
| Repository-local includes | [mulib-record](mulib-record.md) |
| Complete example | [`examples/modules/etl-transform.c`](../examples/modules/etl-transform.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/etl-transform.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
