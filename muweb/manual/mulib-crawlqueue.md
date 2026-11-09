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
cc -std=c11 -Wall -Wextra -Iinclude examples/crawlqueue.c -o /tmp/example-crawlqueue
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

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
