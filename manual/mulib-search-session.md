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
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/search-session.c -o /tmp/example-search-session
```

## API

```c
typedef struct mulib_search_session {size_t offset,limit,max_results;} mulib_search_session_t;
int mulib_search_session_init(mulib_search_session_t *s,size_t page_size,size_t max_results);
size_t mulib_search_session_next(mulib_search_session_t *s);
```

## Safety and ownership

`init` requires positive page size and maximum results; 1 initializes, 0 leaves state unchanged. `next` returns the next page's count (not its starting offset) and advances `offset`, or 0 exhausted/NULL. Save the old offset before calling if needed for a request. The final page is shortened to the remaining bound and arithmetic is safe through SIZE_MAX. No allocation, fetching or provider pagination-token support is implemented.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-search-session.h`](../include/mulib-search-session.h) |
| Implementation switch | `MULIB_IMPL_SEARCH_SESSION` |
| Repository-local includes | None |
| Complete example | [`examples/modules/search-session.c`](../examples/modules/search-session.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/search-session.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
