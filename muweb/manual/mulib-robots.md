# mulib-robots: Simple robots.txt User-agent and Disallow matcher for a selected bot.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

Simple robots.txt User-agent and Disallow matcher for a selected bot. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_ROBOTS` in exactly one C11 translation unit. Use `#include "mulib-robots.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-robots.c -o /tmp/test-robots
/tmp/test-robots
cc -std=c11 -Wall -Wextra -Iinclude examples/robots.c -o /tmp/example-robots
```

## API

```c
/* Basic prefix-based robots rules, case-insensitive agent name; no wildcard or longest-match Allow support. 1 allowed, 0 disallowed. */
int mulib_robots_allowed(const char *robots,const char *agent,const char *path);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
