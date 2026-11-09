# mulib-browsercontext: Owned browser context identifiers and private-mode flag.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Owned browser context identifiers and private-mode flag. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_BROWSERCONTEXT` in exactly one C11 translation unit. Use `#include "mulib-browsercontext.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-browsercontext.c -o /tmp/test-browsercontext
/tmp/test-browsercontext
cc -std=c11 -Wall -Wextra -Iinclude examples/browsercontext.c -o /tmp/example-browsercontext
```

## API

```c
typedef struct mulib_browsercontext {uint64_t id;int private_mode;} mulib_browsercontext_t;
int mulib_browsercontext_init(mulib_browsercontext_t *c,uint64_t id,int private_mode);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
