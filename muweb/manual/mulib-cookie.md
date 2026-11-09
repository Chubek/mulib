# mulib-cookie: HTTP Set-Cookie name/value validation and extraction.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

HTTP Set-Cookie name/value validation and extraction. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_COOKIE` in exactly one C11 translation unit. Use `#include "mulib-cookie.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-cookie.c -o /tmp/test-cookie
/tmp/test-cookie
cc -std=c11 -Wall -Wextra -Iinclude examples/cookie.c -o /tmp/example-cookie
```

## API

```c
/* Copy initial cookie-pair from Set-Cookie, preserving raw value; rejects controls. */
int mulib_cookie_pair(const char *header,char *name,size_t nc,char *value,size_t vc);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
