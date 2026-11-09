# mulib-urlcanon: Conservative URL normalization for HTTP(S) resource keys.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Conservative URL normalization for HTTP(S) resource keys. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_URLCANON` in exactly one C11 translation unit. Use `#include "mulib-urlcanon.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-urlcanon.c -o /tmp/test-urlcanon
/tmp/test-urlcanon
cc -std=c11 -Wall -Wextra -Iinclude examples/urlcanon.c -o /tmp/example-urlcanon
```

## API

```c
/* Normalize ASCII scheme and host; strip fragment and default :80/:443 port. Does not resolve relative references, IDNA or dot segments. Returns 1 on success. */
int mulib_urlcanon_normalize(const char *src, char *dst, size_t cap);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
