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
cc -std=c11 -Wall -Wextra -Iinclude examples/search-query.c -o /tmp/example-search-query
```

## API

```c
/* Encode a search phrase as application/x-www-form-urlencoded URL for supported provider. */
int mulib_search_query_url(const char *provider,const char *query,char *out,size_t cap);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
