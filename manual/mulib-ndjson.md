# ndjson — borrowed line iteration

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_NDJSON`.

Init borrows explicit source bytes and resets cursor/physical-line count; NULL
source initializes empty input. Next returns 1 line, 0 EOF, -1 invalid arguments.
It strips LF and one trailing CR, skips empty lines, and accepts the final line
without LF. Whitespace-only lines are not empty. Embedded NUL is ordinary data;
this iterator does not validate JSON. Returned slices are not NUL-terminated and
remain valid only while the source does. EOF/errors leave outputs untouched.
Total iteration cost O(bytes), allocation-free. Do not edit cursor fields.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ndjson.h`](../include/mulib-ndjson.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Bounded borrowed line iteration over newline-delimited JSON source bytes. C11
 * single-header; define MULIB_IMPL_NDJSON once. */
#ifndef MULIB_NDJSON_H
#define MULIB_NDJSON_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    const char *data;
    size_t length, offset, line;
  } mulib_ndjson_t;
  void mulib_ndjson_init (mulib_ndjson_t *p, const char *data, size_t length);
  /* 1 line, 0 eof, -1 invalid; excludes trailing CR/LF, skips blank lines. */
  int mulib_ndjson_next (mulib_ndjson_t *p, const char **line, size_t *length);
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
| Header | [`include/mulib-ndjson.h`](../include/mulib-ndjson.h) |
| Implementation switch | `MULIB_IMPL_NDJSON` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ndjson.c`](../examples/modules/ndjson.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ndjson.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
