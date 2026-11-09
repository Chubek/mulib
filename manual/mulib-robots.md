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
cc -std=c11 -Wall -Wextra -Iinclude examples/modules/robots.c -o /tmp/example-robots
```

## API

```c
/* Literal prefix rules, longest match, Allow wins ties. No path wildcards. */
int mulib_robots_allowed(const char *robots,const char *agent,const char *path);
```

## Safety and ownership

Returns 1 allowed, 0 disallowed or invalid (NULL/empty agent or path not beginning `/`). Agent/directive names are ASCII case-insensitive; path prefixes are case-sensitive. Consecutive User-agent lines share a group, repeated matching groups combine, and exact agent groups override wildcard groups. Longest literal path prefix wins; Allow wins equal-length ties. Empty Disallow imposes no restriction. Comments, horizontal whitespace and CRLF are accepted. No wildcard patterns, `$`, percent decoding or product-token substring matching is implemented. This local helper is not a complete RFC 9309 implementation or an access-control mechanism.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-robots.h`](../include/mulib-robots.h) |
| Implementation switch | `MULIB_IMPL_ROBOTS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/robots.c`](../examples/modules/robots.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/robots.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
