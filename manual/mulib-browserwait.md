# mulib-browserwait: Caller-clocked deterministic browser condition timeout helper.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Caller-clocked deterministic browser condition timeout helper. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_BROWSERWAIT` in exactly one C11 translation unit. Use `#include "mulib-browserwait.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-browserwait.c -o /tmp/test-browserwait
/tmp/test-browserwait
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/browserwait.c -o /tmp/example-browserwait
```

## API

```c
typedef struct mulib_browserwait {uint64_t deadline_ms,interval_ms,next_poll_ms;} mulib_browserwait_t;
int mulib_browserwait_init(mulib_browserwait_t *w,uint64_t now,uint64_t timeout,uint64_t interval);
/* 1 poll, 0 not yet, -1 timed out. */
int mulib_browserwait_poll(mulib_browserwait_t *w,uint64_t now);
```

## Safety and ownership

`init` requires a positive interval and a representable now + timeout; 1 succeeds, 0 leaves state unchanged. `poll` returns 1 when due, 0 before the next poll, -1 for NULL/uninitialized or when now is past the inclusive deadline. Zero timeout allows an immediate poll. Rescheduling is relative to now and saturates at UINT64_MAX; callers should stop after the deadline/condition and must not repeatedly poll at a saturated clock. All times are caller-supplied monotonic milliseconds. There is no sleeping or condition evaluation.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browserwait.h`](../include/mulib-browserwait.h) |
| Implementation switch | `MULIB_IMPL_BROWSERWAIT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/browserwait.c`](../examples/modules/browserwait.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browserwait.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
