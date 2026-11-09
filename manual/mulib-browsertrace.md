# mulib-browsertrace: Caller-owned ring buffer of timestamped browser action events.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Caller-owned ring buffer of timestamped browser action events. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_BROWSERTRACE` in exactly one C11 translation unit. Use `#include "mulib-browsertrace.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-browsertrace.c -o /tmp/test-browsertrace
/tmp/test-browsertrace
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/browsertrace.c -o /tmp/example-browsertrace
```

## API

```c
typedef struct mulib_browsertrace_event {uint64_t time_ms;uint32_t kind;uint32_t detail;} mulib_browsertrace_event_t;
typedef struct mulib_browsertrace {mulib_browsertrace_event_t events[128];size_t next,count;} mulib_browsertrace_t;
void mulib_browsertrace_push(mulib_browsertrace_t *t,mulib_browsertrace_event_t e);
const mulib_browsertrace_event_t *mulib_browsertrace_get(const mulib_browsertrace_t *t,size_t index);
```

## Safety and ownership

Start zero-initialized. `push` copies one event into a fixed 128-entry ring, overwriting the oldest when full; NULL is ignored. Timestamps/kind/detail are opaque caller values, not validated or sorted. `get` borrows the zero-based oldest-first event or returns NULL out of range. Later writes can overwrite borrowed content. There is no allocation or cleanup; reset with a zero-valued struct. External synchronization is required for concurrent access.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browsertrace.h`](../include/mulib-browsertrace.h) |
| Implementation switch | `MULIB_IMPL_BROWSERTRACE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/browsertrace.c`](../examples/modules/browsertrace.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browsertrace.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
