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
cc -std=c11 -Wall -Wextra -Iinclude examples/browsertrace.c -o /tmp/example-browsertrace
```

## API

```c
typedef struct mulib_browsertrace_event {uint64_t time_ms;uint32_t kind;uint32_t detail;} mulib_browsertrace_event_t;
typedef struct mulib_browsertrace {mulib_browsertrace_event_t events[128];size_t next,count;} mulib_browsertrace_t;
void mulib_browsertrace_push(mulib_browsertrace_t *t,mulib_browsertrace_event_t e);
const mulib_browsertrace_event_t *mulib_browsertrace_get(const mulib_browsertrace_t *t,size_t index);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
