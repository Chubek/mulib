# mulib-scrape-schema: Schema-level required string validation for extracted fields.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Schema-level required string validation for extracted fields. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

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
