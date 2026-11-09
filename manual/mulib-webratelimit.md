# mulib-webratelimit: Deterministic caller-clocked minimum-interval request scheduler.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Deterministic caller-clocked minimum-interval request scheduler. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_WEBRATELIMIT` in exactly one C11 translation unit. Use `#include "mulib-webratelimit.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-webratelimit.c -o /tmp/test-webratelimit
/tmp/test-webratelimit
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/webratelimit.c -o /tmp/example-webratelimit
```

## API

```c
typedef struct mulib_webratelimit { uint64_t interval_ms,next_ms; } mulib_webratelimit_t;
void mulib_webratelimit_init(mulib_webratelimit_t *r,uint64_t delay_ms);
/* 1 if permitted and slot consumed, 0 otherwise. */
int mulib_webratelimit_take(mulib_webratelimit_t *r,uint64_t now_ms);
```

## Safety and ownership

Call `init` before use. The first request may run at time zero. `take` returns 1 and sets the next deadline to now + interval, or 0 without mutation if early, NULL or the addition would overflow. The deadline is inclusive; zero interval permits every call. Times are caller-supplied monotonic unsigned milliseconds; there is no sleeping, wall-clock query, catch-up burst, network activity or synchronization.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webratelimit.h`](../include/mulib-webratelimit.h) |
| Implementation switch | `MULIB_IMPL_WEBRATELIMIT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/webratelimit.c`](../examples/modules/webratelimit.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webratelimit.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
