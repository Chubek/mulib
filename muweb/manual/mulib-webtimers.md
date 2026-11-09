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
cc -std=c11 -Wall -Wextra -Iinclude examples/webtimers.c -o /tmp/example-webtimers
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

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
