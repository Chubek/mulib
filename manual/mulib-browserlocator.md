# mulib-browserlocator: Typed, owned browser locator definitions for CSS, XPath, text, and role queries.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Typed, owned browser locator definitions for CSS, XPath, text, and role queries. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_BROWSERLOCATOR` in exactly one C11 translation unit. Use `#include "mulib-browserlocator.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-browserlocator.c -o /tmp/test-browserlocator
/tmp/test-browserlocator
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/browserlocator.c -o /tmp/example-browserlocator
```

## API

```c
typedef enum mulib_browserlocator_kind {MULIB_BROWSERLOCATOR_CSS=1,MULIB_BROWSERLOCATOR_XPATH=2,MULIB_BROWSERLOCATOR_TEXT=3,MULIB_BROWSERLOCATOR_ROLE=4} mulib_browserlocator_kind_t;
typedef struct mulib_browserlocator {mulib_browserlocator_kind_t kind;char *selector;} mulib_browserlocator_t;
int mulib_browserlocator_init(mulib_browserlocator_t *l,mulib_browserlocator_kind_t kind,const char *s);
void mulib_browserlocator_free(mulib_browserlocator_t *l);
```

## Safety and ownership

Start zero-initialized. `init` copies or replaces a nonempty selector of up to 8192 bytes for one of the four declared kinds. It returns 1 on success, 0 on invalid input/allocation failure with state unchanged. Aliases of the previous selector are safe during replacement. `free` resets state and may be repeated; do not shallow-copy owning objects. The selector is stored verbatim: syntax, DOM matching, role resolution and browser automation are not implemented here.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browserlocator.h`](../include/mulib-browserlocator.h) |
| Implementation switch | `MULIB_IMPL_BROWSERLOCATOR` |
| Repository-local includes | None |
| Complete example | [`examples/modules/browserlocator.c`](../examples/modules/browserlocator.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browserlocator.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
