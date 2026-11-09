# mulib-browsercontext: Caller-assigned context metadata

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Stores a caller-assigned context identifier and private-mode flag, without owning resources or enforcing isolation. This value helper does not create browser contexts.

## Integration

Define `MULIB_IMPL_BROWSERCONTEXT` in exactly one C11 translation unit. Use `#include "mulib-browsercontext.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-browsercontext.c -o /tmp/test-browsercontext
/tmp/test-browsercontext
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/browsercontext.c -o /tmp/example-browsercontext
```

## API

```c
typedef struct mulib_browsercontext {uint64_t id;int private_mode;} mulib_browsercontext_t;
int mulib_browsercontext_init(mulib_browsercontext_t *c,uint64_t id,int private_mode);
```

## Safety and ownership

`init` assigns a nonzero caller-supplied ID and normalizes the private-mode flag to Boolean. Returns 1 on success, 0 for NULL or zero ID without changing state. The struct owns no memory and needs no cleanup. IDs are not generated or checked for uniqueness. The flag is metadata only: there is no browser context creation, isolation, cookie policy or private-data enforcement.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browsercontext.h`](../include/mulib-browsercontext.h) |
| Implementation switch | `MULIB_IMPL_BROWSERCONTEXT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/browsercontext.c`](../examples/modules/browsercontext.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browsercontext.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
