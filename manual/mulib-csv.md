# MuLIB csv

[Manuals](README.md) · [Library index](index.md)

Streaming CSV row parser for quoted comma-separated fields. Parses escaped double quotes (`""`) and quoted delimiters/newlines. Row fields are owned temporarily by the parser and are valid only for the duration of a callback invocation. This is a lightweight dialect, not a dialect autodetector.

## Contract and behavior

The parser reads a NUL-terminated string, with comma separators, LF/CR/
CRLF row endings, and double-quoted fields. Inside quotes, doubled quotes emit
one quote and embedded newlines are retained. Unterminated quotes, quotes inside
unquoted fields, and characters after a closing quote before a separator fail.
Empty input emits no rows; empty fields are preserved.

Each callback receives temporary field strings valid only during that callback;
copy anything you keep. Return nonzero to continue, zero to stop. Parse returns
1 for completion and 0 for syntax/allocation/callback failure. Rows already
delivered are not rolled back.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_CSV
#include "mulib-csv.h"
static int mulib_example_csv_cb(void *u, const char *const *f, size_t n) {
    (void)u;
    return n == 2 && !strcmp(f[0], "A") && !strcmp(f[1], "B");
}
int main(void) {
    assert(mulib_csv_parse("A,B\n", mulib_example_csv_cb, NULL));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-csv.h`](../include/mulib-csv.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-csv.h
 * @brief Streaming CSV row parser for quoted comma-separated fields.
 * @defgroup mulib_csv MuLIB csv
 * @{
 * @details Parses escaped double quotes (`""`) and quoted delimiters/newlines.
 * Row fields are owned temporarily by the parser and are valid only for the
 * duration of a callback invocation. This is a lightweight dialect, not a
 * dialect autodetector.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_CSV before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_CSV_H
#define MULIB_CSV_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Streaming CSV row parser for quoted comma-separated fields. */
  typedef int (*mulib_csv_row_callback) (void *user, const char *const *fields,
                                         size_t count);
  /** @brief mulib csv parse; see manual/mulib-csv.md for contracts and
   * examples. */
  int mulib_csv_parse (const char *source, mulib_csv_row_callback callback,
                       void *user);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSV_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-csv.h`](../include/mulib-csv.h) |
| Implementation switch | `MULIB_IMPL_CSV` |
| Repository-local includes | None |
| Complete example | [`examples/modules/csv.c`](../examples/modules/csv.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/csv.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
