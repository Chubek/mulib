# mulib-webstorage: In-memory Web Storage style string key-value store.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

In-memory Web Storage style string key-value store. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBSTORAGE` in exactly one C11 translation unit. Use `#include "mulib-webstorage.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Ikernels tests/test-webstorage.c -o /tmp/test-webstorage
/tmp/test-webstorage
cc -std=c11 -Wall -Wextra -Ikernels examples/modules/webstorage.c -o /tmp/example-webstorage
/tmp/example-webstorage
```

## API

```c
typedef struct mulib_webstorage_entry {char *key,*value;} mulib_webstorage_entry_t;
typedef struct mulib_webstorage {mulib_webstorage_entry_t *entries;size_t len,cap;} mulib_webstorage_t;
int mulib_webstorage_set(mulib_webstorage_t *s,const char *key,const char *value);
int mulib_webstorage_set_limited(mulib_webstorage_t *s,const char *key,const char *value,
                                 size_t max_entries,size_t max_string_bytes);
const char *mulib_webstorage_get(const mulib_webstorage_t *s,const char *key);
const char *mulib_webstorage_key(const mulib_webstorage_t *s,size_t index);
int mulib_webstorage_remove(mulib_webstorage_t *s,const char *key);
void mulib_webstorage_clear(mulib_webstorage_t *s);
void mulib_webstorage_free(mulib_webstorage_t *s);
```

## Safety and ownership

Start zero-initialized and do not copy the owning struct. `set` copies
NUL-terminated keys and values, including empty strings, replacing an existing
case-sensitive key in place. It returns 1 on success or 0 on invalid arguments or
allocation failure. Aliases of stored strings are safe. `set_limited` additionally
rejects a write if the result would exceed either of the given limits (zero is a
literal limit). It charges each live key/value string plus its terminating NUL,
using the new value instead of the old one. It does not bound the allocated
entry array, transient allocations, allocator overhead, or other stores.
Rejections and allocation failures leave the store unchanged. The ordinary
`set` is not limited by previous `set_limited` calls.

`get` returns a borrowed value or NULL; that value expires if its key is
overwritten or removed, or on `clear` or `free`. `key` returns a borrowed key in
insertion order or NULL if out of range. Overwrite preserves its position and
pointer; removal closes the gap, and reinsertion goes at the end. A key pointer
expires on removal, `clear`, or `free`. Growing the backing array does not
invalidate the strings. `remove` accepts a borrowed key and returns 1 on removal
or 0 on miss/invalid arguments. `clear` releases strings and retains the array;
`free` releases both and resets state. Both accept NULL and repeated calls.
Lookup and mutation are linear; limited writes additionally scan the live string
bytes. Growth arithmetic is checked.

## Limitations

There is no persistence, origin or session namespace enforcement, quota
coordination across stores, UTF-16/embedded-NUL support, event delivery, or
cross-thread synchronization. See [offline browser storage](browser-storage-offline.md)
for the separate staging storage mechanisms and integration limits.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-webstorage.h`](../kernels/mulib-webstorage.h) |
| Implementation switch | `MULIB_IMPL_WEBSTORAGE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/webstorage.c`](../examples/modules/webstorage.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/webstorage.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
