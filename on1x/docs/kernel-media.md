# Bounded media kernels: integration boundary

The reviewed inventory in [`manual/media-kernels.json`](../../manual/media-kernels.json)
contains 193 original kernels plus the shared
`mulib-audio-core.h` helper, are bounded C11 algorithms and
provider contracts, not a complete media application or production-admitted
foreign API. Their MANIFEST registrations use `implementation: static-inline`
and `status: bounded`. On1x inventory and compiler-derived declaration inspection
are available; neither an archive ABI nor callable safe/unsafe bindings follow
from registration.

## Inspected baseline

- MuLIB: `53f77657126cf36401a6c18f49691499fdb53e11`, with the supplied 193
  untracked headers preserved and reviewed in place.
- Configured read-only LibStone:
  `3399047f6c6328b8f07788795c610699e66c060a`.
- Native Linux x86-64; GCC 16.2.1 and Clang 22.1.8.

## Provider and algorithm boundaries

| Boundary | Supported scope and required separation |
|---|---|
| Media | Codec framing helpers do not replace real decode/encode/mux providers such as FFmpeg, or native audio providers. |
| Export | Packet remuxing is not encoded export or atomic file publication. A complete export path needs codec draining, cancellation cleanup and a defined commit/rollback policy. |
| Backend | Device availability and actual GPU use are distinct. Provider attachment is not device discovery or evidence of command execution. |
| Bytecode | Scalar f32/bool expression IR is not a general bytecode reader, compiler or compatibility layer. |
| Project | Generic journal and graph helpers do not establish application project serialization. |
| Audio | Mixer pan and gain must be interpreted according to the documented mono/stereo channel contract; broader layout and device support require separate validation. |

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
no semantic sidecar or independent approval is invented. Discovery does not
implicitly create an externally callable archive ABI. Explicit raw-wrapper
generation is a separate operation described in the
[KernelBind guide](kernel-bind.md), not safe admission. The three pre-existing
support headers (`container-core`, `qvm`, `lprowse`) remain explicitly inventoried
as auxiliaries, not falsely promoted to independent kernels.

## Remaining release gates

This pass is a bounded review/inventory slice. It does not establish native
application integration. Canonical resource owners/borrows,
callable external symbols in one implementation TU, exact ABI lowering, full
ExoML ownership/effect/capacity contracts and independent signed admission still
need implementation and review for admitted call profiles. Real provider
bridges, decoded-frame presentation, versioned storage, malformed-input fuzzing, numerical
oracles, realtime scheduling and native GPU/device tests. Cross-platform and
C++ compatibility of the new bundle are not established by the C11 checks.

Reproducible test results and commands are recorded in
[`../VERIFICATION.md`](../VERIFICATION.md); a passing discovery test does not
advance the general safe-FFI gate.
