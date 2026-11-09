# mulib-search-query: Percent-encoded form query builder and Google/Bing URL construction.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Percent-encoded form query builder and Google/Bing URL construction. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SEARCH_QUERY` in exactly one C11 translation unit. Use `#include "mulib-search-query.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-search-query.c -o /tmp/test-search-query
/tmp/test-search-query
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/search-query.c -o /tmp/example-search-query
```

## API

```c
/* Encode a search phrase as application/x-www-form-urlencoded URL for supported provider. */
int mulib_search_query_url(const char *provider,const char *query,char *out,size_t cap);
```

## Safety and ownership

Provider must be exactly `google` or `bing`. The query may be empty; ASCII letters/digits and `-_.~` pass through, space becomes `+`, and every other byte is encoded with uppercase percent escapes. UTF-8 bytes are encoded without validation. Returns 1 on success, 0 for invalid pointers/provider or insufficient capacity, leaving output unchanged. Capacity includes NUL; input/output must not overlap. No network request is performed.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-search-query.h`](../include/mulib-search-query.h) |
| Implementation switch | `MULIB_IMPL_SEARCH_QUERY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/search-query.c`](../examples/modules/search-query.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/search-query.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
