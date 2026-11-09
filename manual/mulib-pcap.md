# mulib-pcap

**Write classic little-endian PCAP stream headers and timestamped capture records**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_PCAP` once before including `mulib-pcap.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_pcap_write_global(void *buf,size_t n,uint32_t snaplen,uint32_t linktype);
int mulib_pcap_write_record(void *buf,size_t cap,uint32_t seconds,uint32_t micros,
                            const void *pkt,size_t length,size_t *written);
```

## Semantics and safety

Classic pcap (microsecond timestamps), little endian only, not pcapng or compression. Write bytes to caller-managed file/stream.

## Demonstration

See `../tests/test_pcap.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-pcap.h`](../include/mulib-pcap.h) |
| Implementation switch | `MULIB_IMPL_PCAP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/pcap.c`](../examples/modules/pcap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pcap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
