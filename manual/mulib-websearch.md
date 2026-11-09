# mulib-websearch: Static Google/Bing provider identification

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Identifies the two supported providers without performing URL routing or search requests. This value helper is not a browser or search-engine integration.

## Integration

Define `MULIB_IMPL_WEBSEARCH` in exactly one C11 translation unit. Use `#include "mulib-websearch.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-websearch.c -o /tmp/test-websearch
/tmp/test-websearch
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/websearch.c -o /tmp/example-websearch
```

## API

```c
typedef enum mulib_websearch_provider {MULIB_WEBSEARCH_GOOGLE=1,MULIB_WEBSEARCH_BING=2} mulib_websearch_provider_t;
const char *mulib_websearch_provider_name(mulib_websearch_provider_t p);
int mulib_websearch_known(mulib_websearch_provider_t p);
```

## Safety and ownership

`provider_name` returns a borrowed static `google`/`bing` string, or NULL for any unknown enum. `known` returns Boolean membership. No allocation or cleanup is required. This module identifies providers only: use [search-query](mulib-search-query.md) to build a URL; no routing, result fetching or browser interaction happens here.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-websearch.h`](../include/mulib-websearch.h) |
| Implementation switch | `MULIB_IMPL_WEBSEARCH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/websearch.c`](../examples/modules/websearch.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/websearch.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
