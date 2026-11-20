# JoltFX assessment kernels: integration boundary

The 193 headers listed in the root `AGENTS.md`, plus the review's shared
`mulib-audio-core.h` helper, are bounded C11 algorithms and
provider contracts, not a replacement JoltFX application or production-admitted
foreign API. Their MANIFEST registrations use `implementation: static-inline`
and `status: bounded`. On1x inventory and compiler-derived declaration inspection
are available; neither an archive ABI nor callable safe/unsafe bindings follow
from registration.

## Inspected baseline

- MuLIB: `53f77657126cf36401a6c18f49691499fdb53e11`, with the supplied 193
  untracked headers preserved and reviewed in place.
- Configured read-only LibStone:
  `3399047f6c6328b8f07788795c610699e66c060a`.
- JoltFX public tree, inspected read-only:
  `30b787bd1e3d938bee4b69c7a7696e6b4d3dbb32`.
- Native Linux x86-64; GCC 16.2.1 and Clang 22.1.8.

JoltFX inspection covered `src/include/jfx/{backend_interface,jfx_project}.h`,
the SDK backend forwarding header, the JBC reader and execution implementation,
the Vulkan backend/compute implementation, and `docs/media.md`. No JoltFX files,
plugin ABI, `.jfx` reader/writer or JBC format were changed.

## Preserve existing implementations

| Boundary | Current evidence and rule |
|---|---|
| Media | JoltFX already uses FFmpeg for decode/encode/mux and miniaudio for audio. The new codec framing helpers do not replace these providers. |
| Export | JoltFX has snapshot jobs, cancellation, codec flushing and temporary-file commit. The new remux helper does not establish equivalent encoded/transactional export. |
| Backend | `gpu_available` and `used_gpu` are distinct. JoltFX retains its real Vulkan implementation. Attaching a compute provider is not device discovery or proof of GPU execution. |
| Bytecode | JoltFX executes its existing JBC representations. The scalar expression IR is not a JBC reader, compiler or compatibility layer. |
| Project | `.jfx` is the existing bounded line-oriented graph/sequence format. Generic journal and graph helpers are not replacement project serialization. |
| Audio | JoltFX stereo balance preserves both center channels. The bundle's mono equal-power pan is a different mechanism, not automatic mixer parity. |

Canonical MuLIB inspection also found distinct representations. Media rationals
now alias `mulib_fraction_t`, with additional positive-timebase validation, but
that C type reuse does not establish an admitted foreign type;
`mulib_linalg_vec3_t` uses float while mesh vectors use double; GLX pixel formats
describe native-endian packed words rather than the bundle's tagged media planes.
`mulib-floatgraphics.h` is an ordered rectangle collection, not an RGBA image-view
ABI. Layout similarity is not permission to cast or transfer ownership.

## On1x use

```on1x
import kernel mulib.media_timebase as Time;
import kernel mulib.img_blend as Blend;
import kernel mulib.compute_vulkan as Vulkan;
fn main() {}
```

Use the [KernelBind inspection command](kernel-bind.md#inspect-requests-and-discover-declarations).
The resulting CDecl v2 retains real inline definitions with internal linkage.
Reports say `not_representable`, `callable:false`, and `safe_export:false`;
no semantic sidecar or independent approval is invented. Archive construction
rejects static-inline-only profiles before publication. The three pre-existing
support headers (`container-core`, `qvm`, `lprowse`) remain explicitly inventoried
as auxiliaries, not falsely promoted to independent kernels.

## Remaining release gates

This pass is a bounded review/inventory slice. It does not complete the root
assessment's native application milestones. Canonical resource owners/borrows,
callable external symbols in one implementation TU, exact ABI lowering, full
ExoML ownership/effect/capacity contracts and independent signed admission still
need implementation and review. So do real provider bridges, decoded-frame UI
presentation, project compatibility, complete malformed-input fuzzing, numerical
oracles, realtime scheduling and native GPU/device tests. Cross-platform and
C++ compatibility of the new bundle are not established by the C11 checks.

Reproducible test results and commands are recorded in
[`../VERIFICATION.md`](../VERIFICATION.md); a passing discovery test does not
advance the general safe-FFI gate.
