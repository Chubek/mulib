# Engineering reference kernels

[Manifest](../MANIFEST.yaml) · [Integration guide](integration.md)

The engineering families provide circuit and digital simulation, numerical
methods, hardware design and verification, layout/routing, canvas tools, RF,
instrument, and waveform mechanisms. Each header's manifest entry records its
purpose, implementation switch, and dependencies. Consult its declarations and
source comments for capacities, ownership, units, and deliberate restrictions.

## Implementation placement

Reusable types and helpers are separated into storage, numerical, graph,
2D-geometry, hardware, and domain-specific substrates. The
[`mulib-kernel-substrates.h`](../include/mulib-kernel-substrates.h) aggregate
includes these static-inline facilities; it does not automatically adapt
the older EDA/HWIL/SPICE representations.

For implementation-switch headers, define the listed `MULIB_IMPL_*` switches
and their dependency closure in one C11 translation unit before inclusion.
Other consumers include declarations only. Link with `-lm` where required.
Registration describes code present, not complete standards conformance.

Do not combine the new [`dsim-resolve`](../include/mulib-dsim-resolve.h)
implementation with legacy [`dsim-logic4`](../include/mulib-dsim-logic4.h):
both export `mulib_dsim_resolve` with incompatible signatures. The checker
builds these profiles separately.

## Representation boundaries

The checked [CSR adapters](mulib-num-csr.md) copy a borrowed
`mulib_num_csr_view` into canonical owned `mulib_num_csr_t` storage or lend a
read-only view of that storage. Both count `size_t` elements and share logical
validation. Adapters preserve row order, duplicates, explicit zeros, and units;
they do not establish suitability for a particular solver.

| Domain | Required conversion contract |
| --- | --- |
| Numerics | `mulib_num_dense_reference_solve` and `num-dense` use double partial-pivot solvers but have different tolerance rules: only the former accepts zero tolerance. `mulib_num_csr_view_matvec` expects validated storage; canonical matvec stages output and rejects nonfinite accumulations. COO canonicalization and solver-specific duplicate assembly are separate operations. |
| 2D geometry and PCB | `mulib_geom2_point`, `mulib_geometry_point_t`, and `mulib_pcb_point_t` are distinct types despite containing two doubles. `mulib_geom2_box` is an ordered closed AABB, not a position/extent item. Specify units, origin, orientation, finite bounds, layer/net/object identity, and checked 32-/64-bit ID conversion. |
| 3D geometry | `linalg`/`cgeom` float vectors differ from `mesh-core` double positions and `uint32_t` indices. Specify handedness, winding, transforms, checked narrowing, index mapping, capacities, and ownership. |
| Electrical simulation | The tested schematic path supports at most 256 named two-terminal R/C/I/V parts and emits text cards. Parsed netlists reserve node `0` for ground; `mulib_spice_mna` uses `MULIB_STORAGE_NONE`. Branch slots are not node IDs. Preserve terminal order, SI units, and stable identities. The text path uses the bounded dense transient solver, not automatic sparse MNA dispatch. |
| HWIL | `mulib_hwil_hw_context` owns dynamic indexed nodes with widths 1–64 and `uint64_t` constants; ports separately borrow names and record direction/width. `hwil-module` instead has 512 embedded nodes, `uint16_t` IDs, one-bit four-state evaluation, and no register gate. Conversion must define operations, port order/direction, identity, X/Z, register state, and unsupported widths. |

## Media and execution boundaries

Media frame capacities and strides are in bytes; `mulib_img_view` capacities
and strides are in float elements with linear-light straight RGBA. A bridge
must prove extents, color transfer/primaries, alpha association, origin, and
allocator/borrow lifetimes. Packed RGBA8 is not implicitly linear.

Frames per second and seconds per tick are reciprocal conventions. Convert
explicitly with range checks, rounding policy, and distinctions between source,
sequence, and presentation time.

Borrowed single-output graph topology does not encode arbitrary application
project state. Graph writers and memory journals are not complete project
storage. Device availability and actual GPU execution need separate evidence;
compute attachment and codec framing do not replace native providers.
Scalar f32/bool SSA is not a general bytecode compatibility layer.

See the [shared representation contracts](representation-contracts.md),
[media boundary](../on1x/docs/kernel-media.md), and
[engineering status](engineering-status.md) for further limits and historical
verification.
