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
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/scrape-table.c -o /tmp/example-scrape-table
```

## API

```c
/* Count opening <td> and <th> tags, ASCII case insensitive; not a full HTML parser. */
size_t mulib_scrape_table_cells(const char *html);
```

## Safety and ownership

Returns the count of complete opening `td`/`th` tags, ignoring ASCII case and excluding comments, script/style contents and markup inside quoted attributes. NULL/empty input returns zero. No allocation occurs. Cells outside a table also count; nesting, row/column spans, implied closing tags, malformed HTML recovery and cell-value extraction are not implemented.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-scrape-table.h`](../include/mulib-scrape-table.h) |
| Implementation switch | `MULIB_IMPL_SCRAPE_TABLE` |
| Repository-local includes | [`include/detail/mulib-webscan.h`](../include/detail/mulib-webscan.h) |
| Complete example | [`examples/modules/scrape-table.c`](../examples/modules/scrape-table.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/scrape-table.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
