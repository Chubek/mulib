# packetrules — ordered packet policy

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_PACKETFITER` and `MULIB_IMPL_PACKETRULES`.
See [`examples/modules/packetrules.c`](../examples/modules/packetrules.c).

A rule selects IP family 4/6, protocol (zero wildcard), source/destination
prefixes, and optionally inclusive source/destination port ranges. Prefixes
are 0..32 or 0..128; network host bits are ignored. Rule action is 0 drop or
1 accept. A zero-initialized port range is port zero only when `ports=1`;
use 0..65535 for an unrestricted port range. Set `ports=0` to ignore ports.

`validate` checks all rules, including those that would be shadowed, with a
65536-entry maximum. `apply` validates the complete table, parses the packet,
and returns the first matching rule's action or the supplied default (0/1).
Port rules cannot match fragments or packets without decoded TCP/UDP ports.
Return -1 for invalid policy and -2 for malformed/unsupported packet; callers
should handle errors explicitly rather than interpret them as acceptance.
`matched` is the selected index or SIZE_MAX on default/error; it is optional.

No rule ownership, allocation, counters, mutable connection state, NAT or kernel
installation is supplied. Calls borrow the entire table and packet. The parser's
IPv6 extension-header limitations apply. Tests:
`tests/test_protocol_extensions.c` includes fragmented-packet bypass checks.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-packetrules.h`](../include/mulib-packetrules.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-packetrules.h
 * @brief Ordered IP-prefix, protocol and port-range packet rules.
 * Define MULIB_IMPL_PACKETRULES and MULIB_IMPL_PACKETFITER once.
 */
#ifndef MULIB_PACKETRULES_H
#define MULIB_PACKETRULES_H
#include "mulib-packetfiter.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_packetrules_rule
  {
    unsigned family, protocol; /* family 4/6, protocol 0 wildcard */
    unsigned char source[16], destination[16];
    unsigned source_prefix, destination_prefix;
    uint16_t source_low, source_high, destination_low, destination_high;
    int ports, action; /* ports 0 wildcard/1 ranges; action 0 drop/1 accept */
  } mulib_packetrules_rule_t;
  /** @brief Validate all rules, <=65536 entries; 1 valid, 0 invalid. */
  int mulib_packetrules_validate (const mulib_packetrules_rule_t *, size_t);
  /** @brief First matching rule wins, else default_action (0/1). Returns -1
   * for invalid rules, -2 for invalid/unsupported packet, or action 0/1.
   * matched is SIZE_MAX for no match/error. Port rules never match fragments.
   */
  int mulib_packetrules_apply (const mulib_packetrules_rule_t *, size_t,
                               int default_action, const void *, size_t,
                               size_t *matched);
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
| Header | [`include/mulib-packetrules.h`](../include/mulib-packetrules.h) |
| Implementation switch | `MULIB_IMPL_PACKETRULES` |
| Repository-local includes | [mulib-packetfiter](mulib-packetfiter.md) |
| Complete example | [`examples/modules/packetrules.c`](../examples/modules/packetrules.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/packetrules.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
