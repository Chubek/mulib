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
cc -std=c11 -Wall -Wextra -Iinclude examples/webstorage.c -o /tmp/example-webstorage
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

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
