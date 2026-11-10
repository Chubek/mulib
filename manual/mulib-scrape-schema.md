# mulib-scrape-schema: String, integer, real and boolean field validation.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Validate extracted scalar fields without allocating storage.

## Integration

Define `MULIB_IMPL_SCRAPE_SCHEMA` in exactly one C11 translation unit. Use `#include "mulib-scrape-schema.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-scrape-schema.c -o /tmp/test-scrape-schema
/tmp/test-scrape-schema
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/scrape-schema.c -o /tmp/example-scrape-schema
```

## API

```c
/* Return 1 when required field value exists and meets max bytes. */
int mulib_scrape_schema_string(const char *value,int required,size_t max_bytes);
```

## Safety and ownership

Returns 1 when byte length excluding NUL fits `max_bytes` and a required string is nonempty. Optional NULL is valid; required NULL/empty is invalid. Empty optional strings fit a zero limit. Whitespace-only strings are nonempty; no trimming, Unicode/code-point counting, type coercion or schema traversal occurs. No allocation or output mutation.

Integer parses decimal with `strtoll`; real parses with `strtod`. Both require
complete consumption and inclusive ordered bounds, reject ERANGE, and return
1 on success or 0 on failure without changing optional output. Real bounds and
results must be finite. Leading C-locale whitespace/signs are accepted; trailing
whitespace/junk is rejected. Real syntax and decimal separator follow the active
C locale (including hexadecimal float syntax). Boolean accepts exactly `true`,
`false`, `1`, or `0`. Inputs are NUL-terminated. Regression:
`tests/test_data_patches.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-scrape-schema.h`](../include/mulib-scrape-schema.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB scrape-schema: patched string, integer, real and boolean validation. */
#ifndef MULIB_SCRAPE_SCHEMA_H
#define MULIB_SCRAPE_SCHEMA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_scrape_schema_string (const char *value, int required, size_t max_bytes);
  int mulib_scrape_schema_integer (const char *value, int64_t min_value,
                                   int64_t max_value, int64_t *parsed);
  int mulib_scrape_schema_real (const char *value, double min_value, double max_value,
                                double *parsed);
  int mulib_scrape_schema_boolean (const char *value, int *parsed);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-scrape-schema.h`](../include/mulib-scrape-schema.h) |
| Implementation switch | `MULIB_IMPL_SCRAPE_SCHEMA` |
| Repository-local includes | None |
| Complete example | [`examples/modules/scrape-schema.c`](../examples/modules/scrape-schema.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/scrape-schema.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
