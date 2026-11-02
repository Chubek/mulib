# mulib-pci

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
