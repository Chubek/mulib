# mulib-scrape-table: Simple HTML table cell counting for static document inspection.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Simple HTML table cell counting for static document inspection. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SCRAPE_TABLE` in exactly one C11 translation unit. Use `#include "mulib-scrape-table.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-scrape-table.c -o /tmp/test-scrape-table
/tmp/test-scrape-table
cc -std=c11 -Wall -Wextra -Iinclude examples/scrape-table.c -o /tmp/example-scrape-table
```

## API

```c
/* Count opening <td> and <th> tags, ASCII case insensitive; not a full HTML parser. */
size_t mulib_scrape_table_cells(const char *html);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
