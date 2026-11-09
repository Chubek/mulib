# mulib-search-serp: Extract href attributes from simple quoted HTML anchor markup.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Extract href attributes from simple quoted HTML anchor markup. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SEARCH_SERP` in exactly one C11 translation unit. Use `#include "mulib-search-serp.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-search-serp.c -o /tmp/test-search-serp
/tmp/test-search-serp
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/search-serp.c -o /tmp/example-search-serp
```

## API

```c
/* Retrieve Nth quoted href attribute; does not decode entities or execute scripts. */
int mulib_search_serp_href(const char *html,size_t index,char *out,size_t cap);
```

## Safety and ownership

Copies the zero-based Nth anchor's quoted `href`; empty quoted values count. ASCII tag/attribute names are case-insensitive; either quote style and whitespace around `=` are supported. Comments, script/style contents, non-anchor tags and `data-href` are ignored. Returns 1 copied, 0 absent/error/insufficient capacity with output unchanged. Capacity includes NUL; no overlap. Entities and relative references remain raw. This is a generic anchor scanner, not provider-specific search-result classification or URL validation.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-search-serp.h`](../include/mulib-search-serp.h) |
| Implementation switch | `MULIB_IMPL_SEARCH_SERP` |
| Repository-local includes | [`include/detail/mulib-webscan.h`](../include/detail/mulib-webscan.h) |
| Complete example | [`examples/modules/search-serp.c`](../examples/modules/search-serp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/search-serp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
