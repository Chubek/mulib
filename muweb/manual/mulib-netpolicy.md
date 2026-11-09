# mulib-netpolicy: URL scheme and obvious loopback/private-host denial policy.

[MuLIB repository](https://github.com/Chubek/mulib)

## Scope

URL scheme and obvious loopback/private-host denial policy. The implementation is deliberately bounded and is not a replacement for a complete browser standards implementation. The provided functions are implemented, not placeholders. This module does not silently provide unrelated browser services.

## Integration

Define `MULIB_IMPL_NETPOLICY` in exactly one C11 translation unit. Use `#include "mulib-netpolicy.h"`. Other translation units include the header without the macro. The module uses only ISO C standard-library facilities unless noted. No additional library implementations are enabled implicitly.

## Build and test

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test-netpolicy.c -o /tmp/test-netpolicy
/tmp/test-netpolicy
cc -std=c11 -Wall -Wextra -Iinclude examples/netpolicy.c -o /tmp/example-netpolicy
```

## API

```c
/* Deny local hostnames, raw private/loopback IPv4 ranges, and non-HTTP(S) schemes. Not a DNS-resolution SSRF defense. */
int mulib_netpolicy_public_url(const char *url);
```

## Safety and ownership

Functions return error indicators as described in the function comments. Byte buffers and text slices are explicit length-aware where applicable. Callers own their input/output buffers. All testing is local, with no live network requests or external browser processes.

## Limitations

This is a functional building block, not a claim of complete W3C/WHATWG/ECMAScript compliance. For dynamic page execution use MuLIB Prowse with an external browser pending a native ECMAScript/Web API runtime.
