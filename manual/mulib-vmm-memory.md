# mulib-vmm-memory

[Manuals](README.md) · [Library index](index.md)

Allocation-free guest RAM copying and per-device DMA policy validation.
Define `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_VMMDEV` and `MULIB_IMPL_VMM_MEMORY`
in one translation unit. Example: `examples/modules/vmm-memory.c`.

The module reuses `mulib_vmmdev_memory_t`. Inventories and backing RAM remain
caller-owned. Unlike a single VMMDEV DMA span, a memory copy may cross adjacent
regions. Region order in the inventory is irrelevant. Gaps, integer overflow,
host aliases and permission mismatches are rejected before any copy or zeroing.

* `check(memory, address, size, permissions)` validates the complete range and
  inventory. An empty range succeeds with a valid inventory and permission mask.
* `read()` and `write()` preflight the entire transfer, then copy. Host input or
  output storage must not overlap **any** guest RAM region; aliases are rejected
  before copying. Bounds/permission failures leave destinations unchanged.
* `zero()` validates the whole range before zeroing it.
* `subset(ram, dma)` verifies every device region is a subregion of one CPU RAM
  mapping with the same corresponding host address and equal or reduced
  permissions. An empty view grants no DMA. Inventories cannot overlap internally.

Use READ=1, WRITE=2 from `mulib-vmmdev.h`. Addresses are guest physical addresses,
not host pointers. All functions return `MULIB_VMMDEV_*` results. Valid memory
must remain immutable in layout and externally serialized throughout each call;
the helpers supply no synchronization. The runtime copies inventory metadata and
calls these helpers under its VM lock.

There is no allocation, MMU, virtual-address page-table walk or page ownership
change here. `tests/test_vmm.c` checks cross-region copies, denied writes,
transactionality, aliases, gaps, overflow and permission-reduced device views.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-vmm-memory.h`](../include/mulib-vmm-memory.h) |
| Implementation switch | `MULIB_IMPL_VMM_MEMORY` |
| Repository-local includes | [mulib-vmmdev](mulib-vmmdev.md) |
| Complete example | [`examples/modules/vmm-memory.c`](../examples/modules/vmm-memory.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vmm-memory.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
