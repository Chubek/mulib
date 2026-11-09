# mulib-search-rank: Deterministic lexical score for ranking title/description matches.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Deterministic lexical score for ranking title/description matches. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SEARCH_RANK` in exactly one C11 translation unit. Use `#include "mulib-search-rank.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-search-rank.c -o /tmp/test-search-rank
/tmp/test-search-rank
cc -std=c11 -Wall -Wextra -Iinclude examples/search-rank.c -o /tmp/example-search-rank
```

## API

```c
/* Case-insensitive token occurrence count of query in haystack; capped at SIZE_MAX. */
size_t mulib_search_rank_count(const char *text,const char *query);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
