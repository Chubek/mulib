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
cc -std=c11 -Wall -Wextra -Iinclude examples/scrape-schema.c -o /tmp/example-scrape-schema
```

## API

```c
/* Return 1 when required field value exists and meets max bytes. */
int mulib_scrape_schema_string(const char *value,int required,size_t max_bytes);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
