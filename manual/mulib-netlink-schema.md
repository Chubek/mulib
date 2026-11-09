# mulib-netlink-schema

**Typed fixed-width policy checks for a collection of parsed Netlink attributes**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NETLINK_SCHEMA` once before including `mulib-netlink-schema.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink-attr.h`.

## Public API

```c
typedef struct mulib_netlink_policy {uint16_t type;size_t minimum,maximum;int required;} mulib_netlink_policy_t;
int mulib_netlink_schema_check(const void *attrs,size_t n,const mulib_netlink_policy_t *rules,size_t count);
```

## Semantics and safety

Attribute flag bits (`NLA_F_NESTED` / `NLA_F_NET_BYTEORDER`) are masked when
matching policy types. This does not convert byte order: the application must
interpret the payload appropriately. Policy types must be at most `0x3fff`,
length bounds must be ordered, and duplicate policy types are rejected with
`-EINVAL`. Unknown attributes remain allowed.

Bounded 256-rule validator, unknown attributes ignored; parse wire first. This validates sizes, not YAML spec ingestion or nested attribute schemas.

## Demonstration

See `../tests/test_netlink-schema.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-netlink-schema.h`](../include/mulib-netlink-schema.h) |
| Implementation switch | `MULIB_IMPL_NETLINK_SCHEMA` |
| Repository-local includes | [mulib-netlink-attr](mulib-netlink-attr.md) |
| Complete example | [`examples/modules/netlink-schema.c`](../examples/modules/netlink-schema.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/netlink-schema.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
