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
cc -std=c11 -Wall -Wextra -Iinclude examples/browserwait.c -o /tmp/example-browserwait
```

## API

```c
typedef struct mulib_browserwait {uint64_t deadline_ms,interval_ms,next_poll_ms;} mulib_browserwait_t;
int mulib_browserwait_init(mulib_browserwait_t *w,uint64_t now,uint64_t timeout,uint64_t interval);
/* 1 poll, 0 not yet, -1 timed out. */
int mulib_browserwait_poll(mulib_browserwait_t *w,uint64_t now);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
