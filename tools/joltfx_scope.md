# Original JoltFX kernel scope

Source of the original 193-kernel inventory (section 7 of the integration bundle).
Keep this as provenance; current refinements belong in the headers and generator.

| Header | Implemented scope | Explicit limitations |
|---|---|---|
| `mulib-media-core.h` | Common checked arithmetic, status and byte views. |  |
| `mulib-media-timebase.h` | Exact rational timebase normalization and checked rescaling. | May reject very large intermediate products even if final quotient would fit. |
| `mulib-media-pixelformat.h` | Plane layout for packed and subsampled images. |  |
| `mulib-media-frame.h` | Checked strided frame views, heap allocation and row copying. | Copy requires disjoint frames or identical layout. Owned and borrowed frames share layout; caller tracks ownership. |
| `mulib-media-packet.h` | Deep copy and release of encoded packets. | Release only cloned packets. |
| `mulib-media-stream.h` | Short-read and short-write handling for blocking streams. |  |
| `mulib-media-clock.h` | Anchored monotonic host-clock mapping (rate in ticks/ns). | Caller supplies monotonic timestamps; no audio PLL. |
| `mulib-media-bufferpool.h` | Fixed-size reusable frame pool. | Single-threaded; all leases must be returned before destruction. |
| `mulib-media-metadata.h` | Borrowed key/value metadata with checked capacity. |  |
| `mulib-media-convert.h` | RGB, gray, float and limited-range BT.709 YUV420 to RGBA8. | Disjoint source/destination; no transfer conversion. |
| `mulib-media-demux.h` | Demux provider lifecycle and checked dispatch. | Needs caller-supplied demux implementation (e.g. FFmpeg bridge). |
| `mulib-media-mux.h` | Mux provider lifecycle with finalization guard. | Provider required. |
| `mulib-media-codec.h` | Send/receive codec provider contract with explicit draining (NULL input). | No bundled codec provider. |
| `mulib-media-isobmff.h` | Bounds-checked ISO BMFF box traversal including extended sizes. | No sample tables, timestamps or container writing. |
| `mulib-media-matroska.h` | EBML variable integers and bounded element traversal. | Unknown-sized element consumes remaining caller-supplied scope; no block/lacing support. |
| `mulib-media-mpegts.h` | 188-byte transport packet header and adaptation-field parsing. | No PES assembly, PAT/PMT or muxing. |
| `mulib-media-h264.h` | Annex B NAL iterator and emulation-prevention removal. | Bitstream tooling only; no H.264 encoder/decoder. |
| `mulib-media-h265.h` | Annex B HEVC NAL header extraction. | No HEVC encoder/decoder. |
| `mulib-media-vp9.h` | VP9 uncompressed frame prefix parsing. | Does not validate full frame or decode pixels. |
| `mulib-media-av1.h` | AV1 low-overhead OBU framing and LEB128 sizes. | No AV1 encoder/decoder. |
| `mulib-media-seek.h` | Binary timestamp seek followed by keyframe backtracking. |  |
| `mulib-media-scale.h` | Nearest-neighbor RGBA8 scale. | Requires disjoint buffers; use img-resample for high quality. |
| `mulib-media-proxy.h` | Aspect-preserving in-memory proxy frame generation. | No proxy media files or cache. |
| `mulib-media-export.h` | Cancellable packet remux loop with release and finalization. | Provider callbacks must be blocking; no encode or atomic filesystem commit. |
| `mulib-nle-timeline.h` | Caller-backed timeline with exact integer ticks and unique clip IDs. | No automatic overlap policy; source and sequence use same tick units. |
| `mulib-nle-track.h` | Track lookup and mute/solo policy. |  |
| `mulib-nle-clip.h` | Clip source mapping and bounded slip. |  |
| `mulib-nle-interval.h` | Half-open range intersection and bounded output query. | Linear scan; replace with augmented interval index for large timelines. |
| `mulib-nle-edit.h` | Split, trim and atomic adjacent roll editing. |  |
| `mulib-nle-ripple.h` | Validate-then-apply ripple shift. | Caller resolves overlaps and locked tracks. |
| `mulib-nle-transition.h` | Linear and smooth crossfade weights. |  |
| `mulib-nle-retime.h` | Piecewise-linear source-time mapping, including reverse and hold. |  |
| `mulib-nle-snap.h` | Nearest clip boundary snapping with deterministic ties. |  |
| `mulib-nle-evaluate.h` | Visit active clips at a timeline tick. | Track compositing order is caller policy. |
| `mulib-nle-edl.h` | Write lossless neutral tab-separated edit list. | Not CMX3600; implement SMPTE/drop-frame interchange separately. |
| `mulib-nle-otio.h` | OpenTimelineIO RationalTime numeric bridge. | No OTIO JSON schema serialization. |
| `mulib-img-core.h` | Float RGBA strided image view and pixel math. | get/put are unchecked internal primitives; validate and bound indices first. Linear light, straight alpha by convention. |
| `mulib-img-tile.h` | Zero-copy tile views and row-major tile enumeration. |  |
| `mulib-img-sampler.h` | Nearest, bilinear, Catmull-Rom and Lanczos3 samplers. | Clamp borders; straight alpha interpolation, premultiply if filtering transparent images. |
| `mulib-img-resample.h` | Center-aligned image reconstruction. | Disjoint buffers; no scale-dependent antialiasing for strong minification. |
| `mulib-img-convolve.h` | General odd-kernel correlation with clamped boundaries. | Disjoint buffers; flip weights for true convolution. |
| `mulib-img-blur.h` | Separable Gaussian blur with caller-provided scratch. | Scratch must not overlap either image. |
| `mulib-img-morphology.h` | Square alpha erosion and dilation. | Disjoint buffers. |
| `mulib-img-edge.h` | Sobel gradient magnitude image. | No Canny hysteresis or Laplacian. |
| `mulib-img-transform.h` | Inverse homography image transformation with transparent exterior. | Disjoint buffers. |
| `mulib-img-warp.h` | Displacement map warp; field R/G are source offsets in pixels. | Clamp border; disjoint buffers. |
| `mulib-img-matte.h` | Luminance-derived soft alpha matte. |  |
| `mulib-img-keying.h` | Soft chroma-distance alpha key. | No spill suppression or foreground color recovery. |
| `mulib-img-blend.h` | Straight-alpha source-over compositing and blend modes. |  |
| `mulib-img-pyramid.h` | Half-resolution box-filter pyramid level. | Not a Gaussian/Laplacian reconstruction pyramid. |
| `mulib-img-opticalflow.h` | Exhaustive local motion estimation by block matching. | Integer displacement only; no dense multiscale flow. |
| `mulib-img-motionblur.h` | Velocity-map shutter integration. | No occlusion handling. |
| `mulib-img-denoise.h` | Spatial bilateral RGB denoising. | No temporal denoise. |
| `mulib-img-tracking.h` | Track a set of points by block matching with confidence rejection. | No feature detection or subpixel refinement. |
| `mulib-color-transfer.h` | Linear/sRGB/Rec.709/PQ/HLG transfer functions. | PQ uses normalized 10000-nit luminance; HLG is scene OETF, no OOTF. |
| `mulib-color-primaries.h` | 3x3 color matrices and D65 Rec.709/Rec.2020/P3 matrices. |  |
| `mulib-color-adaptation.h` | Bradford XYZ white-point adaptation. | Supply normalized XYZ white points. |
| `mulib-color-gamut.h` | Neutral-axis gamut compression into bounded RGB. | Simple RGB heuristic, not perceptually uniform. |
| `mulib-color-lut1d.h` | Clamped linear 1D LUT interpolation. |  |
| `mulib-color-lut3d.h` | Trilinear and tetrahedral 3D LUT evaluation, red-fastest order. |  |
| `mulib-color-cube.h` | Read standard unit-domain 3D .cube LUTs. | No combined 1D/3D or arbitrary domains; partial scratch on failure. |
| `mulib-color-tonemap.h` | Reinhard and ACES-inspired fitted filmic curves. | Not an official ACES rendering transform. |
| `mulib-color-grade.h` | ASC-CDL-style slope, offset, power and saturation. | Unclamped positive output; specify CDL clamp mode for interchange. |
| `mulib-color-wheels.h` | Luminance-weighted three-way grading offsets. | Simple overlapping weights, not a Resolve-compatible wheel model. |
| `mulib-color-curves.h` | Validated piecewise-linear grading curves. | No monotone cubic or cyclic hue interpolation. |
| `mulib-color-qualifier.h` | Soft luminance/saturation selection mask. | Hue and spatial cleanup not implemented. |
| `mulib-color-vectorscope.h` | BT.709 Cb/Cr density histogram to caller-owned square bins. |  |
| `mulib-color-waveform.h` | Luma or RGB waveform density bins. |  |
| `mulib-color-pipeline.h` | Fused per-pixel pipeline of caller-supplied color stages. | Callbacks trusted and synchronous. |
| `mulib-nodegraph-core.h` | Borrowed graph topology with indexed edges and typed single outputs. |  |
| `mulib-nodegraph-types.h` | Exact port-type equality and edge validation. |  |
| `mulib-nodegraph-validate.h` | Validate node identity, port types and unique incoming edges. | Cycle rejection is in nodegraph-compile; unconnected ports allowed. |
| `mulib-nodegraph-compile.h` | Kahn topological ordering and cycle detection. | O(VE) traversal; add adjacency index. |
| `mulib-nodegraph-eval.h` | Execute a topologically ordered graph through an operation provider. | Callback manages values and cleanup; no parallel execution. |
| `mulib-nodegraph-cache.h` | Revision-keyed non-owning intermediate result cache. | No eviction; caller releases values. |
| `mulib-nodegraph-dirty.h` | Propagate invalidation downstream to fixed point. | Quadratic worst case; use adjacency and queue. |
| `mulib-nodegraph-region.h` | Backward ROI union with per-node filter halo. | Requires valid topological order; transform-specific ROI mapping remains. |
| `mulib-nodegraph-temporal.h` | Checked temporal dependency window expansion. |  |
| `mulib-nodegraph-fusion.h` | Conservative candidate detection for unary operator fusion. | Does not rewrite graph; verify execution semantic compatibility. |
| `mulib-nodegraph-serialize.h` | Versioned textual graph topology writer. | No reader or node properties. |
| `mulib-nodegraph-layout.h` | Layered DAG layout with deterministic rows. | Valid topological order required; no crossing minimization. |
| `mulib-mesh-core.h` | Caller-owned indexed triangle mesh and vector math. | Triangle-only initial model; integrate canonical cgeom/linalg types later. |
| `mulib-mesh-halfedge.h` | Triangle half-edge construction and nonmanifold edge rejection. | O(E²), no nonmanifold vertex detection. |
| `mulib-mesh-adjacency.h` | Vertex-to-face incidence query. | Linear scan; add CSR adjacency. |
| `mulib-mesh-primitives.h` | Box and regular grid primitive generation. |  |
| `mulib-mesh-attributes.h` | Checked dense per-element attribute channels. |  |
| `mulib-mesh-edit.h` | Single-face extrusion with cap and side walls. | No region extrusion, bevel or inset. |
| `mulib-mesh-triangulate.h` | Ear clipping for simple planar polygons. | No exact predicates or holes; assumes non-self-intersection. |
| `mulib-mesh-subdivide.h` | Shared-edge linear triangle subdivision. | Not Loop or Catmull-Clark smoothing; disjoint meshes. |
| `mulib-mesh-decimate.h` | Shortest-edge selection and index collapse with degenerate face removal. | Not QEM; may change topology; leaves unused vertices. |
| `mulib-mesh-remesh.h` | Jacobi Laplacian smoothing with fixed vertices. | Face-weighted neighbors; no isotropic split/collapse/flip remeshing. |
| `mulib-mesh-boolean.h` | Point-in-closed-mesh parity classification, a boolean building block. | No mesh boolean output; edge degeneracies and robust intersections unresolved. |
| `mulib-mesh-repair.h` | Remove zero-area or below-threshold triangles. | No hole filling, welding or orientation repair. |
| `mulib-mesh-normals.h` | Area-weighted smooth vertex normals. | No tangent frames or split normals. |
| `mulib-mesh-uv.h` | Normalized planar UV projection. | No seam generation, LSCM or atlas packing. |
| `mulib-mesh-skin.h` | Four-weight normalized linear blend skinning. |  |
| `mulib-mesh-obj.h` | Wavefront OBJ position and triangle export. | No import, material, normal or UV records. |
| `mulib-mesh-ply.h` | ASCII PLY positions and triangle export. | No binary PLY or import. |
| `mulib-mesh-gltf.h` | GLB 2.0 JSON and BIN chunk extraction. | No glTF JSON/accessor/material import or export. |
| `mulib-geom-bezier.h` | Arbitrary-degree de Casteljau curve evaluation. | Scratch must have n elements; no surface API. |
| `mulib-geom-bspline.h` | De Boor B-spline curve evaluation and span search. |  |
| `mulib-geom-nurbs.h` | Rational B-spline curve evaluation using homogeneous coordinates. | Positive weights, degree <=32; no tensor-product surface yet. |
| `mulib-geom-tessellate.h` | Regular parametric surface tessellation via callback. | Not adaptive; add curvature-based refinement and crack prevention. |
| `mulib-geom-sdf.h` | Sphere, box and smooth-union signed distance functions. |  |
| `mulib-geom-metaball.h` | Compact-support polynomial metaball field. |  |
| `mulib-geom-isosurface.h` | Marching tetrahedron cell polygonization. | Requires grid traversal, consistent winding and vertex welding. |
| `mulib-geom-cloner.h` | Grid and radial instance positions; linear is grid with two dimensions =1. |  |
| `mulib-geom-deform.h` | Twist, taper and cylindrical bend point deformation. |  |
| `mulib-geom-csg.h` | Implicit-field CSG union, intersection and difference. | Not exact boundary-representation CSG. |
| `mulib-anim-keyframe.h` | Sorted integer-time scalar keyframes and insertion. |  |
| `mulib-anim-curve.h` | Step, linear and cubic Hermite animation interpolation. | Tangents in value per tick. |
| `mulib-anim-easing.h` | Six bounded easing functions. |  |
| `mulib-anim-quaternion.h` | Quaternion normalization, multiplication, shortest-path SLERP and vector rotation. |  |
| `mulib-anim-transform.h` | TRS point transformation and animation interpolation. | No matrix/shear decomposition. |
| `mulib-anim-channel.h` | Evaluate multiple scalar animation property channels. | Partial output on invalid later channel. |
| `mulib-anim-blend.h` | Normalized weighted scalar blending. |  |
| `mulib-anim-skeleton.h` | Parent-ordered skeleton forward point transformation. | No matrix palette generation. |
| `mulib-anim-ik.h` | FABRIK position-chain inverse kinematics. | False may still return best-effort pose; no joint limits. |
| `mulib-anim-morph.h` | Weighted blend-shape delta application. | Caller guarantees nonoverlapping delta/output arrays. |
| `mulib-anim-driver.h` | Ordered, acyclic scalar dependency execution. | Caller supplies topology and expression engine. |
| `mulib-anim-bake.h` | Bake a scalar curve into uniform integer-tick samples. | Partial output on error. |
| `mulib-anim-exposure.h` | Midpoint stratified shutter-time samples. |  |
| `mulib-physics-rigidbody.h` | Semi-implicit Euler for translational sphere rigid bodies. | No rotation, torque, inertia tensor or angular momentum. |
| `mulib-physics-broadphase.h` | Sphere AABB overlap candidate generation. | O(n²); validate bodies before use. |
| `mulib-physics-narrowphase.h` | Sphere-sphere normal and penetration depth. | No convex or mesh collision. |
| `mulib-physics-manifold.h` | Match previous sphere contacts by stable body indices and normal. | No manifold points or warm-start application. |
| `mulib-physics-solver.h` | Normal collision impulse and positional correction. | Single contact, no friction or angular effects. |
| `mulib-physics-world.h` | Sphere-world integrator with iterative contact resolution. | O(n²), no CCD or persistent broadphase; partial state on numeric failure. |
| `mulib-physics-joint.h` | Position-based distance joint. | Does not update velocities or support angular constraints. |
| `mulib-physics-particle.h` | Particle advection, exponential drag and lifetime expiration. |  |
| `mulib-physics-cloth.h` | Position-based cloth stretch constraint projection with pinning. | No integration, bending or self-collision. |
| `mulib-render-material.h` | Validated linear-light PBR material parameter representation. |  |
| `mulib-render-scene.h` | Triangle scene, material table and Moller-Trumbore closest-hit traversal. | Brute-force scene hit; validate scene once before render. |
| `mulib-render-bvh.h` | AABB slab intersection and caller-stack BVH traversal. | False conflates miss and invalid traversal; builder-generated tree assumed. |
| `mulib-render-bvhbuild.h` | Balanced centroid-partition BVH builder. | Not SAH or Morton LBVH; caller provides 2N-1 nodes. |
| `mulib-render-camera.h` | Pinhole look-at camera with normalized pixel rays. |  |
| `mulib-render-sampling.h` | Deterministic RNG, radical inverse and cosine-weighted hemisphere samples. |  |
| `mulib-render-bsdf.h` | Lambert BRDF, GGX normal distribution and Schlick Fresnel primitives. | No full GGX sampling, masking term or transmission. |
| `mulib-render-light.h` | Inverse-square point-light evaluation. | No area-light sampling. |
| `mulib-render-shadow.h` | Ray-tested visibility to a point on a light. | No shadow maps; requires validated scene. |
| `mulib-render-integrator.h` | Direct Lambert illumination from point lights with visibility. | Validated scene required; no specular direct lighting. |
| `mulib-render-pathtrace.h` | Diffuse multi-bounce stochastic path tracing. | Validated scene required; no MIS, specular BSDF, Russian roulette or BVH integration. |
| `mulib-render-raster.h` | Screen-space Gouraud triangle rasterization with depth test. | No clip space, perspective correction or top-left fill rule. |
| `mulib-render-texture.h` | Bilinear sampling with mip-level interpolation and repeat coordinates. | Caller generates mip levels; repeat seams use clamped bilinear. |
| `mulib-render-film.h` | HDR sample accumulation and resolve. | No AOV multiplexing; not thread-safe. |
| `mulib-render-denoise.h` | Normal-guided 5x5 bilateral render denoising. | Normals in signed RGB, no temporal accumulation. |
| `mulib-compute-core.h` | Device-neutral dispatch and fence contract. |  |
| `mulib-compute-device.h` | Select from real provider-supplied device capability descriptors. | No platform enumeration. |
| `mulib-compute-buffer.h` | Checked host buffer allocation and staging copies. | CPU memory only, not device allocation. |
| `mulib-compute-image.h` | Aligned-row host storage image allocation. | No GPU texture memory. |
| `mulib-compute-pipeline.h` | Pipeline binding layout validation. | Provider owns actual pipeline creation. |
| `mulib-compute-dispatch.h` | Validated dispatch to provider and ceil-division workgroup sizing. | Actual execution delegated. |
| `mulib-compute-sync.h` | Fence provider wait and monotonic software timeline accounting. | Caller serializes operations; no native barriers. |
| `mulib-compute-memory.h` | Aligned suballocation offsets for a preallocated staging arena. | Not device memory allocation; fence before reuse. |
| `mulib-compute-cache.h` | Device/revision-keyed bounded pipeline handle cache. | No eviction or handle ownership. |
| `mulib-compute-spirv.h` | SPIR-V header/instruction framing validator and instruction emitter. | Structural only; not semantic verification or compiler. |
| `mulib-compute-vulkan.h` | Checked Vulkan queue provider attachment and dispatch. | Adapter only; no native vulkan device, command creation or driver code. Wire JoltFX backend before advertising support. |
| `mulib-compute-metal.h` | Checked Metal command queue provider attachment and dispatch. | Adapter only; no native metal device, command creation or driver code. Wire JoltFX backend before advertising support. |
| `mulib-compute-d3d12.h` | Checked D3D12 command queue provider attachment and dispatch. | Adapter only; no native d3d12 device, command creation or driver code. Wire JoltFX backend before advertising support. |
| `mulib-compute-webgpu.h` | Checked WebGPU queue provider attachment and dispatch. | Adapter only; no native webgpu device, command creation or driver code. Wire JoltFX backend before advertising support. |
| `mulib-compute-cpu.h` | Bounded deterministic CPU workgroup dispatch. | Serial; use taskpool for parallelism. |
| `mulib-audio-mixer.h` | Allocation-free multitrack mono/stereo mixing with pan and gain. | Inputs must not overlap output; no limiter or solo handling. |
| `mulib-audio-resample.h` | Interleaved linear sample-rate conversion; ratio=input/output rate. | No anti-alias filter, streaming phase history or sinc quality. |
| `mulib-audio-channelmap.h` | Arbitrary interleaved channel mixing matrix. | Caller specifies layout matrix; disjoint input/output. |
| `mulib-audio-envelope.h` | Linear or power-interpolated per-frame gain envelope. |  |
| `mulib-audio-filter.h` | Stateful DF-II transposed biquad, lowpass design and causal FIR. | FIR zero initial history; one biquad state per channel. |
| `mulib-audio-eq.h` | RBJ peaking EQ design and cascaded mono processing. |  |
| `mulib-audio-dynamics.h` | Peak envelope compressor and hard limiter. | No lookahead, RMS detection, gate or linked channels. |
| `mulib-audio-delay.h` | Allocation-free feedback delay ring. | Initialize state before per-sample call. |
| `mulib-audio-reverb.h` | Parallel feedback comb reverb. | Set comb wet=1; no diffusion allpass or damping filter. |
| `mulib-audio-timestretch.h` | Offline windowed overlap-add time stretching. | No WSOLA alignment or phase vocoder; pitch preservation is approximate, mono only. |
| `mulib-audio-loudness.h` | Streaming RMS dBFS and sample-peak measurement. | Not LUFS/ITU BS.1770 or oversampled true peak. |
| `mulib-audio-device.h` | Audio provider lifecycle and partial-frame writes. | No native device provider; bridge existing miniaudio or platform audio. |
| `mulib-kernel-ir.h` | Typed straight-line SSA expression IR with explicit result. | Executable subset is f32 and bool; no CFG, memory instructions or vector values. |
| `mulib-kernel-types.h` | Scalar type and packed vector byte-size helpers. | GPU alignment is backend dependent; no matrix/image types. |
| `mulib-kernel-validate.h` | Validate SSA dominance/types and interpret scalar expressions. | No execution budgets beyond bounded instruction count; callers enforce finite input if needed. |
| `mulib-kernel-uniformity.h` | Conservative SSA uniformity propagation from parameter annotations. | No divergent control-flow or barrier analysis. |
| `mulib-kernel-memory.h` | Overflow-safe interval bounds and alias checks for allocation IDs. | No pointer provenance inference or address-space lowering. |
| `mulib-kernel-vectorize.h` | Portable lane-wise batched SSA execution. | Scalar loop, not actual SIMD code generation. |
| `mulib-kernel-fusion.h` | Fuse two pure SSA expressions by parameter substitution and ID remapping. | Out and map require disjoint buffers; no side-effect semantics. |
| `mulib-kernel-glsl.h` | Emit GLSL scalar function from validated SSA. | No compute entry point, descriptors or driver compilation; locale must be C. |
| `mulib-kernel-wgsl.h` | Emit WGSL scalar function with typed let bindings. | No entry point, resources, or shader validation by native compiler. |
| `mulib-kernel-legalize.h` | Finite constant folding as a first legalization pass. | No backend-specific capability lowering; verify float semantic parity. |
| `mulib-asset-core.h` | Stable 128-bit identity and borrowed asset metadata. |  |
| `mulib-asset-catalog.h` | Caller-backed asset catalogue with identity and substring query. | Linear, case-sensitive search; strings borrowed. |
| `mulib-asset-dependency.h` | Transitive dependent-asset invalidation. |  |
| `mulib-asset-cache.h` | Digest- and variant-keyed non-owning asset cache. | Caller computes SHA-256; no eviction or persistent store. |
| `mulib-asset-thumbnail.h` | Aspect-preserving thumbnail from an RGBA8 frame. | Nearest-neighbor; no video seek or cache. |
| `mulib-project-store.h` | Transactional provider interface with lifecycle guards. | Provider supplies durability and recovery, e.g. existing KVDB. |
| `mulib-project-journal.h` | CRC32-framed journal append and valid-prefix recovery. | Memory serialization only; no fsync, atomic commit or transaction markers. |
| `mulib-project-migrate.h` | Ordered monotonic schema migration execution with ambiguity rejection. | Each step must be transactional; earlier successful steps remain committed. |
| `mulib-project-history.h` | Bounded command undo/redo history with branching truncation. | In-memory callbacks only; no persistence or cleanup of discarded commands. |
| `mulib-project-package.h` | Portable relative archive path validation and checked writer dispatch. | Not an archive implementation or sufficient extraction security; handle symlinks, DOS reserved names and collisions. |
