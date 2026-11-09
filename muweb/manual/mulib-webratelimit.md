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
cc -std=c11 -Wall -Wextra -Iinclude examples/webratelimit.c -o /tmp/example-webratelimit
```

## API

```c
typedef struct mulib_webratelimit { uint64_t interval_ms,next_ms; } mulib_webratelimit_t;
void mulib_webratelimit_init(mulib_webratelimit_t *r,uint64_t delay_ms);
/* 1 if permitted and slot consumed, 0 otherwise. */
int mulib_webratelimit_take(mulib_webratelimit_t *r,uint64_t now_ms);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
