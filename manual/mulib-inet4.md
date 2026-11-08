# mulib-inet4

[Manuals](README.md) · [Library index](index.md)

Strict, length-aware IPv4 literals and network-prefix arithmetic. No sockets,
DNS, hostnames, ports, CIDR text, legacy octal/hex or integer-address syntax.

## Integration and grammar

Define `MULIB_IMPL_PARSEC` and `MULIB_IMPL_INET4` before inclusion in one C11
translation unit. Parsing uses MuLIB combinators to capture exactly four octets:
`digit{1,3} ('.' digit{1,3}){3}` followed by end of input. Semantic checks reject
values above 255 and leading zeroes in multi-digit octets. Input is 7..15 bytes;
whitespace, signs, NUL and trailing data are rejected.

## API and ownership

`mulib_inet4_t` is four copyable bytes in network order, not a native-endian
integer. `parse(text,length,&address)` and `format(&address,text,capacity)` return
1 on success, 0 on invalid input/insufficient storage/parser allocation failure.
Outputs are unchanged on failure. A 16-byte formatting buffer always suffices.
Parsing borrows input only during the call; captures are released internally.

`network(&address,prefix,&out)` clears host bits for prefix 0..32 and permits
in-place use. `contains(&network,prefix,&address)` ignores the network's host
bits. Prefix zero matches every address; invalid arguments return zero.

## Example and tests

See [`examples/modules/inet4.c`](../examples/modules/inet4.c):
`cc -std=c11 -Iinclude examples/modules/inet4.c -o inet4`.
`tests/test_network.c` covers malformed syntax, unchanged outputs and boundaries.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-inet4.h`](../include/mulib-inet4.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-inet4.h
 * @brief Strict IPv4 literals and network-prefix operations; no sockets or
 * DNS. Define MULIB_IMPL_INET4 and MULIB_IMPL_PARSEC in one C11 translation
 * unit.
 */
#ifndef MULIB_INET4_H
#define MULIB_INET4_H
#include "mulib-parsec.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Address bytes in network order; freely copyable. */
  typedef struct mulib_inet4
  {
    unsigned char bytes[4];
  } mulib_inet4_t;
  /** @brief Parse exactly four decimal octets; reject leading zeroes.
   * Unchanged on failure. */
  int mulib_inet4_parse (const char *text, size_t length, mulib_inet4_t *out);
  /** @brief Format into at least 16 bytes; output unchanged if capacity is
   * insufficient. */
  int mulib_inet4_format (const mulib_inet4_t *address, char *out,
                          size_t capacity);
  /** @brief Clear host bits for prefix 0..32. Output may alias address. */
  int mulib_inet4_network (const mulib_inet4_t *address, unsigned prefix,
                           mulib_inet4_t *out);
  /** @brief Test membership ignoring network's host bits; invalid arguments
   * return zero. */
  int mulib_inet4_contains (const mulib_inet4_t *network, unsigned prefix,
                            const mulib_inet4_t *address);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-inet4.h`](../include/mulib-inet4.h) |
| Implementation switch | `MULIB_IMPL_INET4` |
| Repository-local includes | [mulib-parsec](mulib-parsec.md) |
| Complete example | [`examples/modules/inet4.c`](../examples/modules/inet4.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/inet4.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
