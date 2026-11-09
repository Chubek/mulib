# mulib-bpf-kernel

**Invoke Linux BPF_MAP_CREATE for an ARRAY map using the kernel bpf syscall**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_BPF_KERNEL` once before including `mulib-bpf-kernel.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_bpf_kernel_array_map(uint32_t value_size,uint32_t entry_count);
```

## Semantics and safety

Creates an ARRAY map, caller closes fd. No eBPF assembler/verifier/program loader, and kernel capabilities and memlock limits apply. Build requires -D_GNU_SOURCE.

## Demonstration

See `../tests/test_bpf-kernel.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bpf-kernel.h`](../include/mulib-bpf-kernel.h) |
| Implementation switch | `MULIB_IMPL_BPF_KERNEL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bpf-kernel.c`](../examples/modules/bpf-kernel.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bpf-kernel.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
