# mulib-uip-arp-cache

**Small time-bounded IPv4-to-MAC ARP cache with deterministic eviction**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_ARP_CACHE` once before including `mulib-uip-arp-cache.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_uip_arp_entry {uint8_t ip[4],mac[6];uint64_t expires_at;int valid;} mulib_uip_arp_entry_t;
typedef struct mulib_uip_arp_cache {mulib_uip_arp_entry_t entries[32];} mulib_uip_arp_cache_t;
int mulib_uip_arp_cache_store(mulib_uip_arp_cache_t *cache,const uint8_t ip[4],const uint8_t mac[6],uint64_t now,uint64_t lifetime);
int mulib_uip_arp_cache_find(const mulib_uip_arp_cache_t *cache,const uint8_t ip[4],uint64_t now,uint8_t mac[6]);
void mulib_uip_arp_cache_expire(mulib_uip_arp_cache_t *cache,uint64_t now);
```

## Semantics and safety

Updating an existing IP searches the entire table before choosing an empty or
expired slot, so an update does not create duplicate live entries. When full,
the earliest expiration is evicted (first slot wins ties). An entry expires at
`now == expires_at`; lifetimes that overflow `uint64_t` are rejected.

Fixed 32-entry ARP cache. Caller provides monotonic clock ticks and lifetime in same units; no ARP validation or spoofing defense. Expiry is strict (expires_at <= now).

## Demonstration

See `../tests/test_uip-arp-cache.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-arp-cache.h`](../include/mulib-uip-arp-cache.h) |
| Implementation switch | `MULIB_IMPL_UIP_ARP_CACHE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uip-arp-cache.c`](../examples/modules/uip-arp-cache.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-arp-cache.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
