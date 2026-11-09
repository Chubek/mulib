# mulib-search-session: Bounded paginated search offset and page-size tracking.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Bounded paginated search offset and page-size tracking. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_SEARCH_SESSION` in exactly one C11 translation unit. Use `#include "mulib-search-session.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-search-session.c -o /tmp/test-search-session
/tmp/test-search-session
cc -std=c11 -Wall -Wextra -Iinclude examples/search-session.c -o /tmp/example-search-session
```

## API

```c
typedef struct mulib_search_session {size_t offset,limit,max_results;} mulib_search_session_t;
int mulib_search_session_init(mulib_search_session_t *s,size_t page_size,size_t max_results);
size_t mulib_search_session_next(mulib_search_session_t *s);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
