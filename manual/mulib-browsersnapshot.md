# mulib-browsersnapshot: Owned immutable byte snapshot with FNV-1a change fingerprint.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Owned immutable byte snapshot with FNV-1a change fingerprint. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_BROWSERSNAPSHOT` in exactly one C11 translation unit. Use `#include "mulib-browsersnapshot.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-browsersnapshot.c -o /tmp/test-browsersnapshot
/tmp/test-browsersnapshot
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/browsersnapshot.c -o /tmp/example-browsersnapshot
```

## API

```c
typedef struct mulib_browsersnapshot {unsigned char *data;size_t size;uint64_t hash;} mulib_browsersnapshot_t;
int mulib_browsersnapshot_create(mulib_browsersnapshot_t *s,const void *data,size_t size);
void mulib_browsersnapshot_free(mulib_browsersnapshot_t *s);
```

## Safety and ownership

Start zero-initialized. `create` copies/replaces 0..64 MiB of arbitrary bytes, returning 1 or 0 with old state preserved on invalid input/allocation failure. NULL data is valid only for zero size; aliases of the old data are safe during replacement. Data is not NUL-terminated. `free` resets state and may be repeated; do not shallow-copy ownership. The standard 64-bit FNV-1a fingerprint is non-cryptographic and may collide, so compare size/bytes for equality. This captures supplied bytes, not browser/DOM state.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browsersnapshot.h`](../include/mulib-browsersnapshot.h) |
| Implementation switch | `MULIB_IMPL_BROWSERSNAPSHOT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/browsersnapshot.c`](../examples/modules/browsersnapshot.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browsersnapshot.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
