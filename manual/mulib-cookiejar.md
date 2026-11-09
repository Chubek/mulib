# mulib-cookiejar: Owned in-memory cookie-pair store with name replacement.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Owned in-memory cookie-pair store with name replacement. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_COOKIEJAR` in exactly one C11 translation unit. Use `#include "mulib-cookiejar.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-cookiejar.c -o /tmp/test-cookiejar
/tmp/test-cookiejar
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/cookiejar.c -o /tmp/example-cookiejar
```

## API

```c
typedef struct mulib_cookiejar {char **names,**values;size_t length,capacity;} mulib_cookiejar_t;
int mulib_cookiejar_set(mulib_cookiejar_t *j,const char *name,const char *value);
const char *mulib_cookiejar_get(const mulib_cookiejar_t *j,const char *name);
void mulib_cookiejar_free(mulib_cookiejar_t *j);
```

## Safety and ownership

Start with `mulib_cookiejar_t jar = {0}`. `set` copies a nonempty name and any string value; it returns 1 on success, 0 on invalid input/allocation failure, preserving stored pairs on failure. Setting an existing name replaces its value and accepts aliases of stored strings. `get` returns a borrowed value or NULL; replacement of that key or `free` invalidates it. `free` resets state and may be repeated. Lookup is linear and growth checked. This is only a case-sensitive pair store: validate cookies separately and do not use it as an origin-aware browser cookie jar.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-cookiejar.h`](../include/mulib-cookiejar.h) |
| Implementation switch | `MULIB_IMPL_COOKIEJAR` |
| Repository-local includes | None |
| Complete example | [`examples/modules/cookiejar.c`](../examples/modules/cookiejar.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/cookiejar.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
