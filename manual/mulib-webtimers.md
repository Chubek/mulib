# mulib-webtimers: Caller-clocked fixed-capacity timer scheduler.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Caller-clocked fixed-capacity timer scheduler. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBTIMERS` in exactly one C11 translation unit. Use `#include "mulib-webtimers.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-webtimers.c -o /tmp/test-webtimers
/tmp/test-webtimers
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/webtimers.c -o /tmp/example-webtimers
```

## API

```c
typedef struct mulib_webtimer {uint64_t due_ms,id;} mulib_webtimer_t;
typedef struct mulib_webtimers {mulib_webtimer_t values[64];size_t count;uint64_t next_id;} mulib_webtimers_t;
uint64_t mulib_webtimers_add(mulib_webtimers_t *t,uint64_t now,uint64_t delay);
int mulib_webtimers_cancel(mulib_webtimers_t *t,uint64_t id);
uint64_t mulib_webtimers_next(mulib_webtimers_t *t,uint64_t now);
```

## Safety and ownership

Start zero-initialized. Up to 64 one-shot timers are stored without allocation. `add` returns a unique nonzero increasing ID or 0 unchanged for full capacity, deadline overflow or ID exhaustion. Zero delay is immediately due. `cancel` returns 1 if removed, 0 for unknown/zero IDs. `next` consumes the earliest due timer, breaking equal deadlines by insertion ID, or returns 0 when none is due. Supply monotonic milliseconds. Cancellation may rearrange internal storage but not firing order. No callbacks, sleeping, repeating timers or event loop are provided.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webtimers.h`](../include/mulib-webtimers.h) |
| Implementation switch | `MULIB_IMPL_WEBTIMERS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/webtimers.c`](../examples/modules/webtimers.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webtimers.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
