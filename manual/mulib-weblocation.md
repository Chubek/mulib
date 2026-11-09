# mulib-weblocation: Owned current URL state for navigation integration.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Owned current URL state for navigation integration. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBLOCATION` and `MULIB_IMPL_URLCANON` in exactly one C11 translation unit. Include `mulib-weblocation.h`; implementations are never enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-weblocation.c -o /tmp/test-weblocation
/tmp/test-weblocation
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/weblocation.c -o /tmp/example-weblocation
```

## API

```c
typedef struct mulib_weblocation {char *href;} mulib_weblocation_t;
int mulib_weblocation_set(mulib_weblocation_t *l,const char *href);
void mulib_weblocation_free(mulib_weblocation_t *l);
```

## Safety and ownership

Start zero-initialized. `set` validates an absolute HTTP(S) URL using [urlcanon](mulib-urlcanon.md), then copies its original spelling (including fragment/case) into owned `href`. It returns 1 on success or 0 preserving old state for invalid input/allocation failure. Aliases of the old href are safe. `free` resets state and may be repeated; do not shallow-copy ownership. No navigation, history, relative resolution, general URI schemes or destination security policy is provided.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-weblocation.h`](../include/mulib-weblocation.h) |
| Implementation switch | `MULIB_IMPL_WEBLOCATION` |
| Repository-local includes | [mulib-urlcanon](mulib-urlcanon.md) |
| Complete example | [`examples/modules/weblocation.c`](../examples/modules/weblocation.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/weblocation.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
