# mulib-pci

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_PCI` once; no dependencies or allocation.
[Example](../examples/modules/pci.c).

`mulib_pci_scan` reads aligned conventional configuration dwords through a
caller-supplied backend, scanning inclusive buses 0..255 and devices 0..31.
Function zero determines whether functions 1..7 are scanned. Absent functions
must return vendor ID `0xffff` with success status. Read/visitor nonzero status
stops and propagates unchanged; invalid arguments return -1. Device metadata
is a temporary borrowed view: copy it if retaining it beyond the callback.
No BAR writes, bus assignment or hardware enabling occurs.

The read backend must implement appropriate CF8/CFC locking or ECAM MMIO with
architecture-specific access widths/barriers. The module supports only ordinary
8-function discovery, not ARI or bridge bus-number assignment. Caller chooses
bus ranges from firmware data.

`mulib_pci_ecam` returns `(relative_bus << 20) | (device << 15) |
(function << 12) | offset`, or `UINT64_MAX` for invalid BDF/alignment/range.
Offsets are aligned dwords 0..4092. For an MCFG region whose start bus is nonzero,
subtract that start bus first, add the physical region base with overflow checks,
then map the result. This helper never dereferences a physical address.

`mulib_pci_capability` searches a conventional capability list for a given ID.
It checks presence/status/header type, pointer range/alignment, and cycles with
a 64-bit visited-offset bitmap. Returns 0 found (writes offset), 1 absent,
-1 malformed/unsupported; backend errors propagate. Supports type 0/1 headers,
not CardBus, PCIe extended lists or ARI. Stabilize configuration while traversing.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-pci.h`](../include/mulib-pci.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-pci.h
 * @brief Conventional PCI configuration discovery with a supplied read backend.
 * Define MULIB_IMPL_PCI once. No writes, BAR probing or bus reconfiguration.
 */
#ifndef MULIB_PCI_H
#define MULIB_PCI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_pci_read_t)(void *, unsigned bus, unsigned device, unsigned function, unsigned offset, uint32_t *);
typedef struct { unsigned bus, device, function; uint16_t vendor, product; uint8_t class_code, subclass, interface_code, revision, header; } mulib_pci_device_t;
typedef int (*mulib_pci_visit_t)(void *, const mulib_pci_device_t *);
/** @brief Scan inclusive bus range 0..255, respecting multifunction header bits.
 * Returns zero on completion, backend/visitor status on failure or early stop.
 * Backend returns 0xffffffff for absent functions. Enumeration never enables devices.
 */
int mulib_pci_scan(mulib_pci_read_t read, void *read_context, unsigned first_bus,
                   unsigned last_bus, mulib_pci_visit_t visit, void *visit_context);
/** @brief Compute segment-relative ECAM offset; UINT64_MAX on invalid BDF/offset. */
uint64_t mulib_pci_ecam(unsigned bus, unsigned device, unsigned function, unsigned offset);
/** @brief Locate conventional PCI capability, following a bounded cycle-checked
 * list. 0 found, 1 absent, -1 malformed; read errors propagate. offset is written
 * only on success. Supports header type 0/1, not CardBus or extended capabilities.
 */
int mulib_pci_capability(mulib_pci_read_t read, void *context, unsigned bus,
                         unsigned device, unsigned function, uint8_t id, unsigned *offset);
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
| Header | [`include/mulib-pci.h`](../include/mulib-pci.h) |
| Implementation switch | `MULIB_IMPL_PCI` |
| Repository-local includes | None |
| Complete example | [`examples/modules/pci.c`](../examples/modules/pci.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pci.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
