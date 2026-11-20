# Amb3r reference kernels

[Manifest](../MANIFEST.yaml) · [Integration guide](integration.md)

The Amb3r group contains reference kernels for circuit and digital simulation,
numerical methods, hardware design and verification, layout and routing, canvas
tools, RF, instruments, and waveforms. The manifest lists each header's purpose,
implementation switch, and direct MuLIB includes. Consult the declarations and
source comments in the individual headers for argument, ownership, and limit
details.

These kernels use the shared `ma_*` types and helpers from
[`mulib-amb3r-base.h`](../include/mulib-amb3r-base.h). The base is static-inline
and needs no implementation switch. It does not replace or automatically
adapt the older MuLIB EDA, HWIL, SPICE, or solver interfaces.

For the other headers, define the listed `MULIB_IMPL_*` switches for the
modules you use and their dependency closure in one C11 translation unit,
before including the headers. Include the same headers without implementation
switches in consumers. Link with the C math library (`-lm` where needed).

Registration describes the code present in each header. It does not imply
standards conformance, full file-format support, or integration with existing
MuLIB EDA workflows.

Do not combine the new [`dsim-resolve`](../include/mulib-dsim-resolve.h)
implementation with legacy [`dsim-logic4`](../include/mulib-dsim-logic4.h):
both export `mulib_dsim_resolve`, with incompatible signatures. The repository
checker builds the Amb3r profile separately from the legacy aggregate.

## Representation boundaries

The checked adapters in [`num-csr`](mulib-num-csr.md) are a specific exception
to the otherwise separate type models. They deep-copy Amb3r `ma_csr` into
canonically owned `mulib_num_csr_t` storage or lend a read-only `ma_csr` view
of that storage. Both representations count `size_t` elements and share
logical CSR validation. The adapters do not sort or coalesce entries or change
physical units. Legacy solvers still decide whether a valid matrix is suitable
for their algorithms.

| Domain | Keep distinct / what another adapter must prove |
| --- | --- |
| Dense and sparse numerics | `ma_solve` and `num-dense` are double dense partial-pivot reference solvers with different tolerance contracts (`ma_solve` accepts zero; the public `num-dense` solver does not). `ma_csr_mv` is an unchecked loop over validated view storage; the canonical matvec stages output and rejects non-finite accumulations. COO canonicalization and solver-specific duplicate assembly have different purposes; neither is implied by the CSR bridge. |
| 2D geometry and PCB | `ma_point`, `mulib_geometry_point_t` (also used by `2dgeom`), and `mulib_pcb_point_t` each contain two doubles but are distinct C types. `ma_box` is an ordered closed AABB, not a `floatgraphics` position/extent item. Coordinates, width and clearance must use the same length unit; the point structs encode neither unit scale nor axis orientation. A bridge must specify unit/origin/orientation and prove finite, ordered bounds. PCB layer, net and object IDs are not coordinates; legacy segments use a 32-bit net number while Amb3r pads/vias have 64-bit net IDs and explicit layers. |
| 3D geometry | `linalg` and `cgeom` use float vectors. JoltFX-bundle `mesh-core` uses double positions, `uint32_t` triangle indices and caller-backed element capacities. A bridge needs axis, handedness/winding and transform contracts, checked narrowing, index mapping and explicit storage ownership. |
| ECAD, SPICE and physics | The tested schematic path emits text cards from at most 256 named two-terminal R/C/I/V parts. Netlist parsing assigns node indices and reserved ground `0`; `ma_mna` uses `MA_NONE` for ground, and MNA branch slots are not node identities. The electrical boundary must preserve positive/negative terminal order, SI units (Ω, F, A, V, s) and stable identities, not cast to the generic graph or the dynamic net/terminal index store. The existing text path uses the bounded dense transient solver; it does not select the Amb3r sparse MNA path. |
| HWIL | `ma_hw_context` owns a dynamic index-based node array (`size_t` slot IDs, widths 1–64, `uint64_t` constants, register nodes); `ma_hw_port` separately records borrowed names, direction and width. `hwil-module` has 512 embedded nodes, `uint16_t` IDs, one-bit four-state evaluation and no register gate; `hwil-types` has a separate kind/width/signedness/four-state descriptor. Converting requires operation, port-order/direction and identity mapping and an explicit policy for X/Z, register state and unsupported widths. |

### JoltFX boundary (audit of JoltFX `30b787b`; no application changes)

- `jfx_image_t` owns packed RGBA8 bytes released by JoltFX; a MuLIB media
  frame can describe up to three planes with byte strides and capacities, and
  its release function is only for matching owned allocations. `mulib_img_view`
  uses RGBA floats with strides and capacities in **float elements** and
  linear-light straight alpha. JoltFX composition uses straight [0,1] colors
  and renders packed RGBA8; the HAL takes interleaved float pixels. Neither
  layout alone establishes a transfer function or primaries. A bridge must
  prove byte/element extents, origin, alpha association, color tags, and
  allocator/borrow lifetimes; do not reinterpret or assume linear RGBA8.
- JoltFX sequences store rational **frames per second** and unsigned frame
  positions; MuLIB media timebases (already share the `mulib_fraction_t`
  representation) are positive **seconds per tick** with signed timestamps.
  An adapter must invert the rate, bound frame-to-tick narrowing, and distinguish
  source in-points, sequence time and presentation time with explicit rounding.
- JoltFX's `.jfx` grammar owns projects, track/effect/key state and graph
  output selection; the MuLIB node graph stores borrowed typed topology
  with a single output per node. The topology writer is not a `.jfx` reader or
  writer. Mapping requires explicit node/port identities, properties and
  compatibility fixtures.
- JoltFX's HAL has separate device-available and used-GPU flags. Its native
  Vulkan path and FFmpeg-backed video/export provider must not be replaced
  by MuLIB compute adapters or codec framing helpers. Joltscript's ABI 0.9
  reads JBC1 v1 (bounded f32 stack) and v2 (typed execution); MuLIB `kernel-ir`
  models scalar f32/bool SSA expressions, not that wire format or its budget,
  effect, call, and closure semantics. No JoltFX ABI, project or provider
  integration was performed by this consolidation.
