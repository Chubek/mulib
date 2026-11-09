# mulib-weblocation: Owned current URL state for navigation integration.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Owned current URL state for navigation integration. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBLOCATION` in exactly one C11 translation unit. Use `#include "mulib-weblocation.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-weblocation.c -o /tmp/test-weblocation
/tmp/test-weblocation
cc -std=c11 -Wall -Wextra -Iinclude examples/weblocation.c -o /tmp/example-weblocation
```

## API

```c
typedef struct mulib_weblocation {char *href;} mulib_weblocation_t;
int mulib_weblocation_set(mulib_weblocation_t *l,const char *href);
void mulib_weblocation_free(mulib_weblocation_t *l);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
