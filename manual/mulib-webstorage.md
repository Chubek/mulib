# mulib-webstorage: In-memory Web Storage style string key-value store.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

In-memory Web Storage style string key-value store. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBSTORAGE` in exactly one C11 translation unit. Use `#include "mulib-webstorage.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-webstorage.c -o /tmp/test-webstorage
/tmp/test-webstorage
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/webstorage.c -o /tmp/example-webstorage
```

## API

```c
typedef struct mulib_webstorage_entry {char *key,*value;} mulib_webstorage_entry_t;
typedef struct mulib_webstorage {mulib_webstorage_entry_t *entries;size_t len,cap;} mulib_webstorage_t;
int mulib_webstorage_set(mulib_webstorage_t *s,const char *key,const char *value);
const char *mulib_webstorage_get(const mulib_webstorage_t *s,const char *key);
void mulib_webstorage_free(mulib_webstorage_t *s);
```

## Safety and ownership

Start zero-initialized. `set` copies keys and values, including empty strings, replacing an existing case-sensitive key. It returns 1 on success, 0 invalid/allocation failure with entries unchanged; aliases of stored strings are safe. `get` returns a borrowed value or NULL, invalidated by replacement of that key or `free`, not by array growth. `free` releases all storage, resets state and may be repeated. Lookup is linear; growth arithmetic is checked. No persistence, origin isolation, quota, events or cross-thread synchronization is provided.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webstorage.h`](../include/mulib-webstorage.h) |
| Implementation switch | `MULIB_IMPL_WEBSTORAGE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/webstorage.c`](../examples/modules/webstorage.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webstorage.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
