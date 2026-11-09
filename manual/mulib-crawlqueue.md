# mulib-crawlqueue: Owned bounded FIFO frontier with URL deduplication.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Owned bounded FIFO frontier with URL deduplication. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_CRAWLQUEUE` in exactly one C11 translation unit. Use `#include "mulib-crawlqueue.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-crawlqueue.c -o /tmp/test-crawlqueue
/tmp/test-crawlqueue
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/crawlqueue.c -o /tmp/example-crawlqueue
```

## API

```c
typedef struct mulib_crawlqueue {char **urls;size_t len,read,cap;} mulib_crawlqueue_t;
/* Returns 1 enqueued, 2 already present, 0 on error. */
int mulib_crawlqueue_push(mulib_crawlqueue_t *q,const char *url);
const char *mulib_crawlqueue_next(mulib_crawlqueue_t *q);
void mulib_crawlqueue_free(mulib_crawlqueue_t *q);
```

## Safety and ownership

Start zero-initialized. Push copies a nonempty string of up to 8192 bytes: 1 enqueued, 2 already seen (including consumed entries), 0 invalid/allocation failure/100000 total-entry limit. Deduplication is byte-exact and linear; URLs are neither validated nor normalized. Failed push preserves entries/read position but may reserve capacity. `next` returns a borrowed FIFO string or NULL; returned strings survive growth and consumption until `free`. Consumed entries remain allocated for lifetime deduplication. `free` resets state and may be repeated.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-crawlqueue.h`](../include/mulib-crawlqueue.h) |
| Implementation switch | `MULIB_IMPL_CRAWLQUEUE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/crawlqueue.c`](../examples/modules/crawlqueue.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/crawlqueue.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
