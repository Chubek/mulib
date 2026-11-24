#!/usr/bin/env python3
"""Maintain the exact 193-kernel media inventory plus one reviewed PCM helper.

--check is read-only. --write-docs regenerates only this tool's own documents.
--sync-manifest is an intentional, additive MANIFEST edit, never implicit.
"""
import argparse
import json
from pathlib import Path
import re
import sys

import manuals

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = "examples/media-pipeline.c"
TEST = "tests/media-integration-test.c"
SUPPORT = {"mulib-container-core.h", "mulib-qvm.h", "mulib-lprowse.h"}
SUPPLEMENTAL = {
    "mulib-audio-core.h": (
        "Checked borrowed PCM byte extents, finite-sample scans and conservative alias helpers.",
        "Low-level caller-backed helpers; no audio allocation, device, codec or safe On1x admission.",
    ),
}

FAMILIES = {
    "media": ("Media", "Byte spans, frame strides and capacities are bytes; dimensions are pixels. "
              "PTS values use the accompanying rational timebase (seconds per tick). Parsed payloads "
              "borrow the input; keep its backing storage alive. Provider callbacks are trusted; "
              "codec, device, synchronization and filesystem policy are not supplied by these helpers."),
    "nle": ("Editing", "Positions, lengths and source offsets are integer ticks in the timeline timebase, "
            "not seconds or frame indices. Clip intervals are half-open. Items and track arrays are "
            "caller-owned; edits do not own source media. Track locking, overlap policy and grouped "
            "undo remain application responsibilities."),
    "img": ("Image", "Images are linear-light, straight RGBA float views. Width/height are pixels; stride "
            "and capacity count float elements, not bytes. A row needs width*4 floats and the live extent "
            "is (height-1)*stride+width*4. Keep all backing storage alive. Supply finite coordinates and "
            "parameters. HDR RGB is not implicitly display-referred; transparent filtering needs explicit "
            "premultiplication/unpremultiplication. Distinct pointers alone do not prove nonoverlap."),
    "color": ("Color", "RGB/XYZ values and matrices are floating-point values, not byte colors. Image "
              "views use linear-light straight RGBA with float-element strides/capacities. Arrays, LUTs "
              "and stage contexts are borrowed. Supply finite parameters and an explicitly chosen "
              "transfer/primaries/alpha model; no ICC/OCIO provider or official ACES transform is implied."),
    "nodegraph": ("Graph", "Graphs borrow nodes, edges and input-type arrays. Edge endpoints and "
                  "topological orders are node indices, not persistent node IDs. Parallel arrays are "
                  "indexed by graph.count. ROI coordinates are pixel bounds; temporal ranges use caller "
                  "ticks. Callback values and cache handles remain caller-owned; graph helpers do not "
                  "free intermediate results or establish thread-safe lifetimes."),
    "mesh": ("Geometry", "Meshes borrow indexed triangle and double-precision vec3 storage. "
             "vertex_count/vertex_capacity count vertices; triangle_count/triangle_capacity count "
             "triangles, not indices or bytes. Triangle indices are uint32_t. Use consistent caller-defined "
             "length units and finite positions; helpers do not supply exact predicates or full CAD topology."),
    "geom": ("Geometry", "Control points and output positions use double-precision vec3 values in "
             "caller-selected length units. Parameters, weights and knots are scalars. Arrays and "
             "callback contexts are borrowed; output mesh capacities count vertices and triangles, "
             "not bytes. Supply finite inputs; these are approximate numerical operations, not exact CAD."),
    "anim": ("Animation", "Key times and sample steps are int64 ticks in one caller-selected timebase; "
             "curve tangents are value per tick. Transform rotations are quaternions; angles are radians. "
             "Storage and callback contexts are borrowed. Use finite values and retain arrays for the "
             "whole synchronous operation; these helpers do not implement an application animation ABI."),
    "physics": ("Physics", "Use a consistent caller-defined length/mass/time system; dt is time in that "
                "system, velocities are length/time, force is mass*length/time², and inverse mass zero "
                "marks a fixed body. Body/contact/link arrays are caller-owned. Finite inputs and valid "
                "indices are required. Sphere translation and position constraints are not an angular "
                "rigid-body or continuous-collision engine."),
    "render": ("Rendering", "Scene triangles/materials, traversal scratch and RNG states are caller-owned. "
               "RGB is linear-light radiance; geometry uses a consistent length unit. Validate scenes "
               "before low-level ray/integrator calls; finite normalized directions are the normal use "
               "case. Ray bounds are ray-parameter distances. No provider GPU renderer is bundled."),
    "compute": ("Compute", "Buffer sizes/offsets and arena alignment are bytes; grids are workgroup "
                "counts. Fences/device IDs/handles are provider-defined integers, not owned pointers. "
                "Serialize context and software-timeline access externally; provider lifetimes and "
                "GPU synchronization must outlive dispatches. Host buffers do not imply GPU allocations."),
    "audio": ("Audio", "PCM is interleaved float unless stated otherwise. A frame has one sample per "
              "channel; frames*channels counts floats, not bytes. Rate and frequency are Hz; times are "
              "seconds where used for attack/release. Inputs, states and scratch are caller-owned. "
              "Supply finite samples/parameters. Initialize state outside realtime callbacks; avoid "
              "allocation, blocking, disk I/O and ordinary locks on the realtime thread."),
    "kernel": ("Compiler", "IR instruction/result IDs and parameter indices are zero-based. Counts and "
               "scratch capacities count typed elements, not bytes. The executable subset is f32/bool, "
               "with straight-line SSA and no memory/control flow. Inputs, code, scratch and output "
               "buffers are borrowed. Set explicit work budgets and finite-input policy; this IR does "
               "not claim compatibility with external language or bytecode ABIs."),
    "asset": ("Assets", "Asset identity is 128 bits; cache digests are 32 bytes of caller-computed SHA-256. "
              "Catalog strings and cache handles are borrowed. Array capacities count records or output "
              "indices, not bytes; source files and intermediate resources remain application-owned."),
    "project": ("Project", "Byte spans and journal offsets/capacities count bytes; history/migration "
                "arrays count commands/steps. Providers and callback contexts are borrowed. Trusted "
                "callbacks must define transactions, lifetime and thread affinity. These helpers do not "
                "establish durable filesystem storage or safe archive extraction."),
}

# Per-header contracts complement the exact section-7 scope and current API.
# They deliberately state caller requirements even where the low-level C API
# cannot prove backing allocations or reject every possible alias.
CONTRACTS = {
    "audio-core": "bytes checks frames*channels*sizeof(float) into a byte count. finite reads "
                  "count live floats and rejects nonfinite samples; count is not frames. disjoint "
                  "takes byte extents on flat-address targets and does not prove pointer provenance. "
                  "float converts a double DSP result, retaining NaN and saturating out-of-float-range "
                  "results to signed infinity. Helpers never allocate or own PCM storage.",
    "media-core": "Checked size/int64 arithmetic writes through non-NULL out pointers on success. "
                  "be32/be64 are unchecked reads requiring at least 4/8 readable bytes. A span "
                  "with nonzero size needs a valid readable data allocation; span validation is "
                  "not proof that a caller-reported allocation size is truthful.",
    "media-timebase": "Normalize requires strictly positive numerator and denominator. Rescale computes "
                      "v*from.num*to.den/(from.den*to.num), truncating toward zero; overflow returns false "
                      "without a new result. The non-__int128 path can reject additional large intermediates.",
    "media-pixelformat": "layout returns row bytes and plane-row count for one plane index. YUV420 "
                         "chroma dimensions round up for odd pixel dimensions. Allocate each plane's "
                         "checked row*rows bytes (or a larger checked strided extent).",
    "media-frame": "Zero-initialize owned frames before allocation. Each active plane must cover "
                   "(rows-1)*stride+row_bytes; active planes must not intersect. Copy needs matching "
                   "dimensions/format and either identical layout or disjoint storage. release frees "
                   "all plane pointers: call only for frames from the matching allocator, never borrowed views.",
    "media-packet": "clone deep-copies in->size bytes into a fresh destination; do not overwrite a live "
                    "owned destination. release is only for matching clones. Encoded bytes and timestamp "
                    "metadata do not establish codec validity or implicit frame ownership.",
    "media-stream": "Read/write callbacks receive byte counts and must not report more than requested. "
                    "read_exact/write_all advance across short blocking operations; zero progress is "
                    "not a successful full transfer. Storage must cover n bytes for the whole call.",
    "media-clock": "ns is a monotonic host timestamp in nanoseconds; clock.rate is ticks per nanosecond. "
                   "Paused position remains anchored. The caller supplies clock state and must reject "
                   "discontinuous host clocks; this is not an audio drift PLL.",
    "media-bufferpool": "Initialize a zeroed pool; n is the number of separately allocated frames. "
                        "acquire returns a borrowed lease, NULL when none is free. Return only a current "
                        "lease to its original pool. Return every lease before destroy; do not release "
                        "individual pool frames or access the pool concurrently.",
    "media-metadata": "a has cap metadata-item slots and *n initialized entries. Keys/values are "
                      "borrowed NUL-terminated strings. Replacement stores the new borrowed value; "
                      "lookups do not transfer ownership. All strings must outlive their table use.",
    "media-convert": "Destination must be valid RGBA8 with source-matching pixel dimensions and "
                     "disjoint storage. Float input is quantized to 8-bit; YUV420 uses limited-range "
                     "BT.709 coefficients, not a general color-managed transfer conversion.",
    "media-demux": "Populate the provider and open state before dispatch. next returns a provider-defined "
                   "packet whose ownership/release contract must be agreed separately; seek uses stream "
                   "PTS units. close invalidates the provider; no default FFmpeg bridge is installed.",
    "media-mux": "write borrows a packet for the synchronous provider call. finish is guarded against "
                 "duplicate finalization; callers must still inspect failure. close does not make a "
                 "failed export atomic or supply filesystem rollback.",
    "media-codec": "Send borrows non-NULL packets/frames; a NULL input requests draining. Receive fills "
                   "provider-managed outputs and may return AGAIN/EOF. The provider must specify frame/"
                   "packet release, backpressure and flush behavior; no codec is bundled.",
    "media-isobmff": "offset indexes input bytes. A successful box borrows payload bytes within the "
                     "input scope and advances offset; 32-bit, extended and remaining-scope sizes are "
                     "framing only. Depth/box-count budgets belong to callers traversing nested boxes.",
    "media-matroska": "off is a byte cursor. vint reads at most eight bytes; element payloads borrow "
                      "the supplied scope. Unknown-sized elements consume the remainder of that scope. "
                      "Callers must bound nesting, element count and scope; no lacing is decoded.",
    "media-mpegts": "parse requires the actual available byte length n and at least one 188-byte "
                    "packet. The returned payload borrows that packet. Header/adaptation parsing does "
                    "not check a transport program or reassemble PES data.",
    "media-h264": "nal returns a borrowed Annex-B NAL span and advances a byte cursor. rbsp writes "
                  "into out.size bytes and reports written bytes; provide capacity at least the "
                  "NAL payload size for the worst case. Helpers do not decode or verify full syntax.",
    "media-h265": "NAL output borrows the Annex-B input. type/layer/temporal are integer header fields, "
                  "not decoded frame metadata. Keep the input alive and check iterator success before "
                  "using outputs; no HEVC decode, encode or full header validation is supplied.",
    "media-vp9": "The bit-reader position counts bits, while input.size counts bytes; requested fields "
                 "are bounded by unsigned output width. parse reads only the uncompressed prefix. "
                 "Successful prefix parsing is not validation of the remaining compressed frame.",
    "media-av1": "off and LEB cursors count bytes; successful OBU payloads borrow input. Length fields "
                 "must fit the remaining byte span. LEB decoding is bounded; callers must impose an OBU "
                 "count budget. No AV1 sequence/frame decode is performed.",
    "media-seek": "e has n entries sorted by PTS in one timebase. find returns an array index for a "
                  "keyframe at or before the target, not an IO seek or decoded presentation frame.",
    "media-scale": "Source and destination must be valid RGBA8 views and have disjoint storage. "
                   "Destination dimensions define the scale; nearest-neighbor has no antialiasing "
                   "or transfer/alpha conversion.",
    "media-proxy": "Destination must be empty/zeroed before allocation. max_w/max_h are nonzero "
                   "pixel bounds. Success creates an owned aspect-preserving RGBA8 frame; release "
                   "with media_frame_release. Failure is not creation of a persistent proxy cache.",
    "media-export": "The remux loop uses blocking demux/mux callbacks and a packet-release callback. "
                    "Progress returning false cancels; inspect the final status. The application owns "
                    "provider closure, temporary output, rollback and atomic commit. It is not encoding.",
    "nle-timeline": "items points to capacity item slots with count initialized entries and unique "
                    "nonzero IDs. insert needs one spare slot; find returns SIZE_MAX on miss. "
                    "Removing shifts items and invalidates borrowed item pointers.",
    "nle-track": "t has n initialized track records; index must be in range. audible applies mute/"
                 "solo policy without altering tracks; find returns SIZE_MAX on miss. Clip ownership "
                 "and track-lock enforcement are not supplied.",
    "nle-clip": "source maps a tick only inside the item's half-open interval. slip takes delta and "
                "asset_length in the same tick units and must preserve the clip's source bounds.",
    "nle-interval": "query writes at most cap item indices but returns the total matching count, "
                    "which can exceed cap. out may be omitted only for a zero-capacity count query. "
                    "The query interval and clip intervals are half-open.",
    "nle-edit": "split requires a free timeline slot, an interior cut tick and a unique new ID. "
                "trim/roll source lengths are ticks. Roll expects adjacent clips. Review each return "
                "before treating application state as a committed transaction.",
    "nle-ripple": "from/delta are signed ticks and track selects one track ID. All affected shifts "
                  "are validated before application. This arithmetic atomicity does not resolve "
                  "collisions, linked clips or track locks.",
    "nle-transition": "length must be positive; start/t are ticks. Outputs a/b are scalar weights "
                      "for linear or smooth crossfade, not a media compositing operation. Both "
                      "output pointers must be live.",
    "nle-retime": "k has n ordered timeline/source key pairs. out is a double-valued source tick, "
                  "allowing reverse and holds. Source-time interpolation is not exact rational "
                  "playback-rate segmentation or decoder seeking.",
    "nle-snap": "tolerance is a nonnegative tick distance. ignore is a clip ID excluded from "
                "boundary candidates. Check false for no candidate; snapping does not alter a "
                "clip or enforce overlap/track policy.",
    "nle-evaluate": "Visitor callbacks borrow each active item synchronously and can reject "
                    "evaluation. Do not mutate/reallocate the timeline during visitation. Track "
                    "compositing order and resource cleanup remain caller policy.",
    "nle-edl": "The caller owns FILE and checks the integer result for write failure. Output "
               "columns are id, asset, track, start, duration, source in neutral tab-separated "
               "form; neither CMX3600 nor SMPTE/drop-frame formatting is implemented.",
    "nle-otio": "RationalTime value/rate are doubles; base is seconds per integer tick. Supply "
                "positive finite rate and a positive rational base. The bridge can round time; "
                "it does not read/write OTIO JSON or preserve schema metadata.",
    "img-core": "get/put are unchecked: first validate the view and ensure x<width, y<height. "
                "clamp_index requires a nonzero n. luma/lerp operate on values, not on a color-managed "
                "display image. Validators cannot prove a caller-supplied backing allocation.",
    "img-tile": "Tile rectangles use pixel origin and dimensions. tile_view borrows a subregion "
                "without allocation; tile_next uses a caller-maintained row-major tile cursor "
                "and a positive pixel edge length. Output views do not own storage.",
    "img-sampler": "Sample positions are source pixel coordinates; border policy is clamp. "
                   "Modes select nearest/bilinear/Catmull-Rom/Lanczos3. Supply valid finite "
                   "coordinates and image storage; interpolation of straight alpha is not alpha-aware.",
    "img-resample": "Destination/source storage must be disjoint. Mode selects center-aligned "
                    "reconstruction; image dimensions supply the scale. The kernel is not widened "
                    "for strong downsampling, so high-frequency inputs can alias.",
    "img-convolve": "k has at least w*h floats; w/h are positive odd kernel dimensions, not image "
                    "dimensions. Source/destination must be matching, disjoint image views. "
                    "This is correlation with clamped borders: flip weights for true convolution.",
    "img-blur": "d/tmp/s must have matching dimensions; tmp must not overlap either image. "
                "radius is 0..127 pixels and sigma is positive finite pixels. Caller scratch is "
                "a full image view. Each separable pass has disjoint source/destination storage.",
    "img-morphology": "d/s are matching disjoint views. radius counts pixels; dilate selects max "
                      "rather than min alpha over the square neighborhood. This changes alpha, "
                      "not a binary morphology topology or premultiplied RGB policy.",
    "img-edge": "d/s must be matching disjoint views. Output is Sobel luminance-gradient magnitude "
                "with clamped boundaries, not Canny hysteresis or a full edge-tracking pipeline.",
    "img-transform": "inv has 9 row-major doubles for destination-to-source homography. Matching "
                     "pixel sizes are not required, but storage is disjoint. Invalid homogeneous "
                     "coordinates/exterior use transparent output; supply finite matrix entries.",
    "img-warp": "field is a matching RGBA view whose R/G components are source pixel offsets. "
                "Destination must not overwrite source or field storage during sampling. "
                "Coordinates clamp at the source edge; no occlusion model is applied.",
    "img-matte": "Source/destination dimensions match; low/high are luminance thresholds with "
                 "low<high. Output alpha is a soft luminance mask, optionally inverted. "
                 "Neighborhood scratch is not needed; protect against partial aliasing.",
    "img-keying": "kr/kg/kb are key RGB; near/far are chroma-distance thresholds with near<far. "
                  "Matching views use finite values. The soft alpha result does not recover "
                  "foreground RGB or suppress spill.",
    "img-blend": "d/a/b dimensions match. blend_pixel treats b as the over/source input for "
                 "source-over. Exact pixelwise alias may be useful, but shifted/partial overlap "
                 "must not corrupt later reads; caller must follow the current header's alias checks.",
    "img-pyramid": "Destination is the header-defined half-resolution level (ceil dimensions "
                   "for odd source sizes). Storage is disjoint. The 2x2 box average is one "
                   "downsample step, not a reconstructible Laplacian pyramid.",
    "img-opticalflow": "x/y, patch and search are integer pixel quantities. dx/dy/error are live "
                       "output pointers. Search is exhaustive local block matching with integer "
                       "displacement; bound search/patch work before processing untrusted requests.",
    "img-motionblur": "velocity is a matching view with R/G pixel displacements over the shutter; "
                      "samples is a bounded positive sample count. Destination must not overlap "
                      "source/velocity. Integration has no visibility/occlusion recovery.",
    "img-denoise": "Matching d/s storage is disjoint. radius is pixels; space is positive spatial "
                   "sigma and range positive RGB-distance sigma. The bilateral operation is "
                   "spatial only and requires a caller-selected work budget.",
    "img-tracking": "p has n mutable point records with pixel positions. patch/search are pixel "
                    "bounds; threshold is a matching-error rejection threshold. Return is the "
                    "accepted point count; inspect per-point confidence/state after processing.",
    "color-transfer": "sRGB/Rec.709 use normalized code/linear values; PQ linear values are normalized "
                      "to 10000 nits. HLG is the scene OETF/inverse, not display OOTF. Define negative/"
                      "HDR/NaN policy explicitly; these functions do not attach metadata.",
    "color-primaries": "Matrices contain 9 doubles and apply to 3-element RGB/XYZ vectors. D65 "
                       "Rec.709/Rec.2020/P3 constructors are fixed constants. Inverse reports "
                       "singular matrices; array arguments need 3 live doubles even without lengths.",
    "color-adaptation": "src/dst each contain 3 normalized XYZ white-point doubles. out is one "
                        "matrix. Supply positive usable white responses; Bradford adaptation "
                        "does not perform ICC profile parsing.",
    "color-gamut": "ceiling is a positive finite RGB bound. Compression pulls RGB toward a "
                   "neutral axis and preserves alpha; it is a heuristic, not a perceptually "
                   "uniform gamut mapper or a scene-to-display rendering transform.",
    "color-lut1d": "a contains n scalar float entries. x is normalized input clamped to the LUT "
                   "domain. The same scalar table can be applied per channel by callers; "
                   "no separate RGB layout is inferred.",
    "color-lut3d": "rgb holds at least 3*edge*edge*edge floats (red-fastest indexing), with all "
                   "products checked before allocation. LUT capacity counts floats. tetra selects "
                   "tetrahedral rather than trilinear interpolation; alpha is preserved.",
    "color-cube": "FILE starts at the beginning and remains caller-owned. rgb has cap floats "
                  "(not RGB triples); accepted LUT size needs 3*edge^3 floats. Parsing can mutate "
                  "scratch before failure. Unit domain and standalone 3D LUT only. Decimal parsing "
                  "is locale-independent; line_budget bounds input lines, each at most 511 bytes.",
    "color-tonemap": "exposure is stops, converted to gain with exp2(exposure). "
                     "Alpha is preserved. Reinhard and ACES-inspired fitted RGB "
                     "curves do not constitute official ACES/OCIO transforms.",
    "color-grade": "CDL slope/offset/power have 3 floats each; saturation is nonnegative. "
                   "power must be positive; image sizes match. Alpha is preserved and output "
                   "is not display-clamped. Choose interchange clamp behavior separately.",
    "color-wheels": "shadow/mid/high each hold 3 RGB-offset floats. Luminance-dependent overlapping "
                    "weights select offsets; alpha is preserved. These controls are not a "
                    "Resolve-compatible wheel transfer model.",
    "color-curves": "p has n ordered (x,y) float points and out is live. Evaluation clamps outside "
                    "the endpoints and linearly interpolates inside. No tangent storage or cyclic "
                    "hue wrap is inferred.",
    "color-qualifier": "Luminance/saturation thresholds and soft width are scalar float units. "
                       "mask returns a scalar selection weight, not an allocated matte; hue "
                       "qualification and spatial cleanup are absent.",
    "color-vectorscope": "bins must hold edge*edge uint32_t counters; any explicit capacity "
                         "argument counts counters, not bytes. The function clears/fills density "
                         "bins. Precheck multiplication and choose a bounded histogram edge.",
    "color-waveform": "bins must hold cols*levels uint32_t counters; any explicit capacity "
                      "argument counts counters. channel=0 selects luma and 1/2/3 select R/G/B. "
                      "Columns/levels are histogram dimensions, not byte pitches.",
    "color-pipeline": "st contains n trusted synchronous stage callbacks plus borrowed contexts. "
                      "d/s image sizes match. Each stage maps one pixel, so there is no "
                      "neighborhood ROI or temporal-resource lifetime engine.",
    "nodegraph-core": "nodes has count records and edges has edge_count records. input_types "
                      "for each node has input_count entries. find returns SIZE_MAX on miss. "
                      "Basic shape validation alone does not prove port compatibility or acyclicity.",
    "nodegraph-types": "Types are numeric tags compared for exact equality. Edge destination "
                       "port is an index into that node's input_types. No implicit numeric "
                       "conversion or multi-output routing is performed.",
    "nodegraph-validate": "Validation checks IDs, typed edge endpoints and duplicate destination "
                          "ports. Unconnected ports are allowed. It does not reject cycles; "
                          "run compile before evaluating.",
    "nodegraph-compile": "order and degree each need graph.count size_t elements. On success, "
                         "order is a topological permutation; false can leave partial scratch. "
                         "The Kahn implementation scans edges repeatedly, so bound graph size.",
    "nodegraph-eval": "order and values need graph.count entries; input scratch needs at least "
                      "the largest input_count. Scratch contains upstream node indices or "
                      "SIZE_MAX for unconnected inputs. Callback owns produced values and cleanup.",
    "nodegraph-cache": "a has n cache slots; initialize validity state before insert. Node/revision/"
                       "key must all match. Entries borrow uintptr_t values; replacement/clear "
                       "does not destroy resources and there is no eviction.",
    "nodegraph-dirty": "dirty is a mutable graph.count-element bool array. Initially mark seed "
                       "nodes; propagation reaches downstream fixed point. Caller owns cache "
                       "release and graph-revision consistency.",
    "nodegraph-region": "order, r and halo each need graph.count entries. Supply a valid "
                        "topological permutation; r contains half-open pixel rectangles and "
                        "halo contains pixel radii. Transform inverse mappings remain caller work.",
    "nodegraph-temporal": "before/after are nonnegative tick extents. Expansion checks signed "
                          "overflow and writes one time range. It does not evaluate frames "
                          "or schedule temporal cache retention.",
    "nodegraph-fusion": "pure has graph.count bool entries; edge is an edge-array index. The "
                        "test requires adjacent unary pure nodes and a single consumer. "
                        "A true result only nominates a candidate, not an executable fused kernel.",
    "nodegraph-serialize": "FILE remains caller-owned. Writer emits versioned topology/IDs/types "
                           "and reports I/O failure; it does not serialize properties, callbacks "
                           "or cached resource ownership and has no matching reader.",
    "nodegraph-layout": "order, pos and rows require graph.count entries and a valid topological "
                        "permutation. Layout coordinates are deterministic UI-layer units; "
                        "it does not minimize edge crossings or own editor state.",
    "mesh-core": "append_vertex/append_tri require unused capacity and preserve uint32 index "
                 "representability. Vector arithmetic helpers are low-level finite-input "
                 "primitives, not allocation/bounds-checked foreign imports.",
    "mesh-halfedge": "e needs 3*triangle_count halfedge records; cap counts records. Twin misses "
                     "use SIZE_MAX. Construction rejects nonmanifold edges but does not prove "
                     "manifold vertices; it is quadratic in edge count.",
    "mesh-adjacency": "v is a vertex index. out holds cap face indices; return is the total "
                      "incident-face count and can exceed cap. Treat zero-capacity queries "
                      "as count-only, not evidence that no faces exist.",
    "mesh-primitives": "Box needs 8 vertices and 12 triangles. A grid with nx/ny cells needs "
                       "(nx+1)*(ny+1) vertices and 2*nx*ny triangles. half is positive half-extent; "
                       "width/height are lengths. Check all capacity products first.",
    "mesh-attributes": "elements counts elements, components counts floats per element, and capacity "
                       "counts floats. set reads components floats from v and writes element i. "
                       "Attributes do not automatically track mesh topology edits.",
    "mesh-edit": "Extruding one face needs 3 extra vertices and 6 extra triangles; face is "
                 "a triangle-array index and offset a length vector. The original triangle "
                 "becomes the cap; this is not shared-region extrusion.",
    "mesh-triangulate": "p has n planar points; scratch needs n uint32_t entries and output "
                        "n-2 triangle records. Any explicit scratch/output capacities count "
                        "elements. Polygon must be simple, no holes; winding may be CW or CCW.",
    "mesh-subdivide": "For T source triangles, e/ids need 3*T records each; destination needs "
                      "4*T triangles and source vertices plus one vertex per unique edge "
                      "(at most source_vertex_count+3*T). Source, destination and scratch "
                      "storage must not overlap. Linear subdivision is not Loop smoothing.",
    "mesh-decimate": "shortest returns two vertex indices and a length. collapse mutates indices/"
                     "positions, removes degenerate triangles and leaves unused vertices. "
                     "Caller must rebuild adjacency/attributes and check topology validity.",
    "mesh-remesh": "scratch has vertex_count vec3 entries and optional fixed has vertex_count "
                   "bool entries. Scratch must not overlap vertices; factor is a finite "
                   "relaxation scalar. This is one Jacobi smooth step, not isotropic remeshing.",
    "mesh-boolean": "Input must be a closed triangle mesh. out is a live bool classification. "
                    "Surface/edge points are ambiguous under parity ray testing; success "
                    "does not produce boolean output geometry.",
    "mesh-repair": "min_double_area is a nonnegative threshold in length². Repair compacts "
                   "triangle storage and returns removed-count semantics from the current "
                   "header; it does not weld vertices, fill holes or fix winding.",
    "mesh-normals": "out has at least vertex_count vec3 entries; cap counts vec3 records. "
                    "Output is area-weighted smooth vertex normals. It must not overwrite "
                    "input geometry while accumulating.",
    "mesh-uv": "uv has 2*vertex_count doubles; cap counts doubles, not UV pairs. axis selects "
               "the dropped coordinate (0..2). Projection normalizes bounding-box coordinates "
               "but does not construct seams or an atlas.",
    "mesh-skin": "weights contain four bone indices and four scalar weights. bones has n "
                 "matrices, each with the header's affine element layout. Output is one "
                 "normalized linear-blend position; no skeleton palette is allocated.",
    "mesh-obj": "FILE is caller-owned; write borrows a valid mesh and reports failure. "
                "Only positions and triangle faces are exported; no materials, normals, "
                "UV records or import parser are supplied.",
    "mesh-ply": "FILE is caller-owned; output is ASCII PLY positions and triangle indices. "
                "No binary payload or arbitrary attribute channel is generated.",
    "mesh-gltf": "Input size counts actual GLB bytes. Parsed JSON/BIN chunks borrow input; "
                 "they are not NUL-terminated strings by contract. Version/chunk framing "
                 "does not validate glTF JSON, buffer views or accessors.",
    "geom-bezier": "p and scratch each have n vec3 elements; explicit capacities, if present, "
                   "count vec3 records. Scratch may alias control points but not output; "
                   "aliased controls are overwritten. t must be finite and in [0,1].",
    "geom-bspline": "v has n control points; knot vector k has n+p+1 doubles with nondecreasing "
                    "knots, degree p<=32 and n>p. Explicit knot capacities count doubles. "
                    "Parameter t must lie in the supported knot domain.",
    "geom-nurbs": "v/weights have n entries; knots have n+p+1 doubles, degree p<=32 and "
                  "n>p. Weights must be positive finite values. Output is one rational "
                  "curve point, not a tensor-product NURBS surface.",
    "geom-tessellate": "nu/nv count grid cells; mesh requires (nu+1)*(nv+1) vertices and "
                       "2*nu*nv triangles. Surface callback is trusted and synchronous "
                       "with normalized u/v; callback failure may leave partial mesh data.",
    "geom-sdf": "Sphere radius/box half-extents and smooth-union k are lengths; choose "
                "positive shape sizes and a positive smoothing radius. Return is a "
                "signed field value, not a tessellated boundary.",
    "geom-metaball": "a has n balls with caller-defined position/radius/strength. Evaluate "
                     "returns a compact-support scalar field at p; output does not own "
                     "or generate isosurface geometry.",
    "geom-isosurface": "p/v contain 4 tetrahedron vertices/scalars. out needs 6 vec3 entries "
                       "for up to two unwelded triangles; return counts triangles (0, 1 or 2). "
                       "Grid traversal, welding and consistent winding are not supplied.",
    "geom-cloner": "Grid output needs nx*ny*nz vec3 entries; cap counts vec3 records. "
                   "Radial output has n records, radius is length and phase radians. "
                   "Outputs are instance positions only, not object ownership or transforms.",
    "geom-deform": "twist uses radians per Z length, taper uses scale slope per Z length, "
                   "and bend curvature has inverse-length units. These pointwise functions "
                   "do not update normals, indices or bounding structures.",
    "geom-csg": "Inputs are compatible signed field values; enum chooses min/max-based "
                "union/intersection/difference. No boundary representation, meshing or "
                "exact intersection construction is performed.",
    "anim-keyframe": "k has cap key slots and *n initialized, strictly time-ordered keys. "
                     "Insert needs room for a new key unless replacing an existing tick; "
                     "floor operates on valid ordered keys and returns a key-array index.",
    "anim-curve": "k has n valid time-ordered keys. mode chooses step/linear/Hermite; "
                  "tangents are value per tick, not per second. out receives one scalar "
                  "and no channel state is allocated.",
    "anim-easing": "t is normalized progress, bounded to the easing domain. Enum selects "
                   "one of six scalar curves. This function does not sample keyframes "
                   "or implement an application-specific easing catalogue.",
    "anim-quaternion": "Quaternion fields are x/y/z/w; interpolation follows shortest path. "
                       "Use finite, nonzero quaternions for meaningful normalization. "
                       "Rotation acts on one vector; no tangent or squad channel is supplied.",
    "anim-transform": "TRS uses position/scale vec3 and quaternion rotation. Point application "
                      "does not allocate matrices; interpolation scalar is normalized progress. "
                      "No shear decomposition or hierarchy palette is performed.",
    "anim-channel": "ch has n channel descriptors; out needs n doubles. Every descriptor "
                    "borrows its key array. A later invalid channel can leave earlier "
                    "output values written; evaluation is not an atomic transaction.",
    "anim-blend": "v/w each have n doubles. Weights must be finite and nonnegative with "
                  "positive total; out receives their normalized weighted scalar blend. "
                  "No quaternion/TRS blend interpretation is inferred.",
    "anim-skeleton": "local/parent each have n entries, with parent earlier than child "
                     "or SIZE_MAX for root. bone is an array index. Output transforms "
                     "one point through its ancestor chain; no matrix palette is allocated.",
    "anim-ik": "p has n chain points and len has n-1 positive segment lengths. Root "
               "is fixed; iterations is a work bound and tolerance a length. False "
               "may still leave a best-effort pose; joint limits are absent.",
    "anim-morph": "out/base each hold vertices vec3 values; delta has targets*vertices "
                  "target-major vec3 values and weights has targets doubles. Ensure "
                  "output does not overlap delta; checked products precede allocation.",
    "anim-driver": "offset has n+1 offsets into deps[dep_count]; every dependency refers "
                   "to an earlier result. values needs n doubles and scratch enough "
                   "for the largest dependency span. Trusted callback consumes borrowed inputs.",
    "anim-bake": "out has n double slots; start and positive step are ticks. "
                 "Overflow/invalid channel can fail after earlier output writes; callers "
                 "need staging if bake publication must be atomic.",
    "anim-exposure": "out has n doubles. center/duration use the same caller-selected "
                     "time unit; duration is nonnegative and n positive. Samples are "
                     "midpoint-stratified times, not integrated motion-blur frames.",
    "physics-rigidbody": "Each body is a translational sphere with position, velocity, "
                         "force, inverse mass, radius and restitution. dt is positive "
                         "finite time; integration is semi-implicit Euler, without rotation.",
    "physics-broadphase": "b has n valid spheres; out has cap pair records. Return "
                          "is the required candidate count and may exceed cap; "
                          "candidates are sphere-AABB overlaps, not narrowphase contacts.",
    "physics-narrowphase": "a/c are indices into b[n] and must differ. out is one "
                          "contact. False may mean no sphere overlap or invalid "
                          "input; it is not a general convex/mesh collision query.",
    "physics-manifold": "now has n contacts and old has count contacts. Stable body "
                        "indices and compatible normals are required for matching. "
                        "Transferred contact state is not a persistent multipoint manifold.",
    "physics-solver": "b has n bodies and c refers to two body indices. correction "
                      "is a positional correction scalar. The solver applies one "
                      "normal impulse and correction, not friction or angular impulse.",
    "physics-world": "b is a mutable n-body sphere array. iterations bounds repeated "
                     "pair resolution and dt is positive finite time. Numeric failure "
                     "can leave partial state; substepping/CCD and persistence are caller work.",
    "physics-joint": "a/b are distinct bodies; rest is nonnegative distance and "
                     "stiffness a bounded scalar. Position projection does not "
                     "update velocities or impose angular constraints.",
    "physics-particle": "p has n mutable particles; a is acceleration, dt time "
                        "and drag an inverse-time decay rate. Return is live-particle "
                        "count semantics from the header; dead storage remains caller-owned.",
    "physics-cloth": "p/inv_mass each have n entries and links has count links "
                     "with valid endpoints/rest lengths. iterations bounds projection "
                     "work; zero inverse mass pins a point. No integration or collisions.",
    "render-material": "Material RGB/reflectance/emission are linear values; roughness/"
                       "metallic are bounded scalars as checked in the header. Validation "
                       "does not mean every represented PBR parameter is used by integrators.",
    "render-scene": "Triangles and materials have the scene's advertised counts and "
                    "triangle material indices must be valid. Validate once before "
                    "low-level hits. Closest-hit traversal is brute force; false conflates miss/invalid.",
    "render-bvh": "Use builder-produced nodes/indices. indices needs one entry per "
                  "scene triangle; stack needs at least the traversal's pending-node "
                  "depth (node count is a conservative bound), with cap in size_t slots. "
                  "Prefer hit_checked for distinct HIT/MISS/INVALID; the bool compatibility "
                  "wrapper conflates miss/invalid. Visited-node checks do not prove full tree bounds.",
    "render-bvhbuild": "For N>0 triangles, nodes needs 2*N-1 records and ids needs "
                       "N size_t entries. cap/idcap count those records; used receives "
                       "actual node count. Builder mutates scratch and is not a semantic tree validator.",
    "render-camera": "fov is vertical radians, aspect width/height. eye/target/up "
                     "must form a usable camera basis. ray u/v are normalized image "
                     "coordinates; a camera ray is not a decoded or displayed pixel.",
    "render-sampling": "RNG state is one mutable uint64_t; keep independent states "
                       "per thread. Radical inverse needs base>=2. Cosine sampling "
                       "uses a valid surface normal and normalized u/v sample scalars.",
    "render-bsdf": "Normals/cosines/roughness use scalar geometric conventions; "
                   "RGB inputs are linear values. Helpers expose Lambert, GGX "
                   "distribution and Fresnel terms only, not a full GGX eval/sample/pdf.",
    "render-light": "Point-light position and query p use length units; output "
                    "direction is normalized, distance length, radiance linear RGB "
                    "after inverse-square falloff. Outputs are caller-owned scalars/vectors.",
    "render-shadow": "Supply a validated scene, point/normal, normalized light "
                     "direction and positive distance. Visibility is a bounded "
                     "ray test; no shadow-map resource or cache is created.",
    "render-integrator": "lights has count point-light records; scene must already "
                         "be validated. Output is one direct Lambert radiance sample "
                         "with visibility, not a complete specular/PBR illumination model.",
    "render-pathtrace": "Scene must be validated; rng points to mutable seed/state "
                        "and bounces bounds diffuse transport work. Output is one "
                        "linear RGB sample. Accumulation, parallelism and BVH traversal are separate.",
    "render-raster": "v contains exactly 3 screen-space colored vertices. depth "
                     "has at least width*height floats and cap counts floats; initialize "
                     "to a far depth. Image/depth/vertices must not overlap.",
    "render-texture": "levels has n valid image views; u/v are repeating normalized "
                      "coordinates and lod is fractional mip level. Caller creates "
                      "and retains mips. Seam sampling uses each level's clamped bilinear path.",
    "render-film": "p is one accumulation record with sum and sample count. Add "
                   "requires finite radiance and a representable count; resolve "
                   "returns a mean. Initialize to zero and synchronize concurrent writers externally.",
    "render-denoise": "d/s/normal are matching views; signed RGB encodes guidance "
                      "normals and sigma controls guidance weighting. Destination "
                      "must be disjoint from inputs. Fixed 5x5 filter has no temporal history.",
    "compute-core": "Contexts hold borrowed provider callbacks plus provider-owned "
                    "state according to close policy. Validate group dimensions against "
                    "provider maxima; close releases through the provider, not a native SDK here.",
    "compute-device": "d has n real provider-supplied descriptors; min_memory is bytes. "
                      "Return is a descriptor-array index or SIZE_MAX. fallback "
                      "allows selection policy, not fabricated GPU discovery.",
    "compute-buffer": "Zero-initialize an owner before alloc; n is host bytes. "
                      "Upload/download require off+n within size and live readable/"
                      "writable spans. free only matching allocations; no device memory is allocated.",
    "compute-image": "w/h count pixels; pixel is bytes per pixel and align a positive "
                     "power-of-two row-byte alignment. Zero-initialize before allocation "
                     "and free only matching host allocations; row pitch can include padding.",
    "compute-pipeline": "h has n resource handles matching the pipeline's declared "
                        "binding count and types. Layout validation does not create "
                        "a shader pipeline or prove that handles reference live resources.",
    "compute-dispatch": "groups counts workgroups; handles has n bindings. fence "
                        "receives a provider token. ceil computes ceil(count/local) "
                        "without wrapping; local must be positive. Real execution is delegated.",
    "compute-sync": "timeout_ns is nanoseconds; provider wait owns its blocking "
                    "semantics. Submit/complete operate on a serialized monotonic "
                    "software timeline and do not insert native memory barriers.",
    "compute-memory": "Arena capacity/cursor/n/off are bytes and align a positive "
                      "power of two. alloc returns an aligned offset, not a pointer "
                      "or GPU allocation. Reset only after all users/fences are complete.",
    "compute-cache": "a has n initialized cache slots keyed by source key/device/"
                     "revision. Pipeline IDs are non-owning provider handles. No "
                     "eviction/destruction occurs; invalidation and lifetime are caller work.",
    "compute-spirv": "w has n uint32_t words; emit cap/count also count words, not "
                     "bytes. Emit needs one instruction word plus count operands. "
                     "Structural validation is not SPIR-V semantic validation.",
    "compute-vulkan": "Attach requires a matching real Vulkan provider and an empty "
                      "destination. Ownership moves on success; do not close a "
                      "moved provider again. Submit delegates; no native device creation is provided.",
    "compute-metal": "Attach requires a matching real Metal provider and empty "
                     "destination. Ownership moves on success. Submit delegates "
                     "only; CPU fallback must not be advertised as Metal dispatch.",
    "compute-d3d12": "Attach requires a matching real D3D12 provider and empty "
                     "destination. Ownership moves on success. Submit delegates "
                     "only; no device, command list or driver setup is implemented.",
    "compute-webgpu": "Attach requires a matching real WebGPU provider and empty "
                      "destination. Ownership moves on success. Submit delegates "
                      "only; no browser/native WebGPU device or command encoder is created.",
    "compute-cpu": "grid product must fit the explicit uint64 work limit. Callback "
                   "runs serially for each workgroup coordinate and false cancels. "
                   "This deterministic CPU path must not be reported as GPU execution.",
    "audio-mixer": "out holds 2*frames stereo floats; each track has its stated "
                   "frames*channels floats, channels 1 or 2. Inputs and output "
                   "must not overlap. Mono pan is equal power; no limiter is applied.",
    "audio-resample": "out/in hold out_frames*ch/in_frames*ch floats. ratio is "
                      "input_rate/output_rate and phase is source-frame position. "
                      "Storage is disjoint; linear interpolation has no antialias filter/history.",
    "audio-channelmap": "out has frames*nc floats, in frames*ni, matrix nc*ni "
                        "row-major mixing weights. All products must fit allocations; "
                        "output does not overlap input/matrix. Channel layout is caller policy.",
    "audio-envelope": "p is an in-place frames*ch float array. from/to are scalar "
                      "gains; equal_power changes interpolation. No envelope state "
                      "or future samples are allocated.",
    "audio-filter": "Each biquad state serves one channel. lowpass uses positive "
                    "rate/hz/Q within Nyquist constraints. FIR out/in each have "
                    "n floats and k taps floats; output is disjoint and initial history is zero.",
    "audio-eq": "rate/hz/Q/db configure one peaking biquad outside processing. "
                "process mutates n mono samples through count independent band "
                "states; do not share a band state across channels or threads.",
    "audio-dynamics": "threshold is a linear amplitude, ratio a compression "
                      "ratio, attack/release seconds and rate Hz. Initialize one "
                      "state per signal before tick; no lookahead or linked-channel detector.",
    "audio-delay": "buf has n float samples; init zeroes it and establishes "
                   "cursor/feedback/wet. tick requires that initialized state "
                   "and a live ring. n is delay samples, not milliseconds or frames across channels.",
    "audio-reverb": "p has n mono samples and comb has count initialized "
                    "independent delay states/rings. Set each comb wet=1 "
                    "for the documented parallel-comb model; rings do not overlap signal or each other.",
    "audio-timestretch": "out/norm each have cap floats and in n mono floats; "
                         "all three are disjoint. window and analysis/synthesis hops "
                         "are samples. written counts produced samples; norm is caller scratch.",
    "audio-loudness": "a has n scalar samples, not interleaved-frame count. "
                      "Initialize accumulator to zero; RMS/peak outputs are dBFS "
                      "with silence represented by negative infinity. This is not LUFS/true peak.",
    "audio-device": "Provider write takes frames n, not floats; storage covers "
                    "n*device.channels floats. written reports partial frames. "
                    "Start/stop/close lifecycle is provider-owned; no native device is bundled.",
    "kernel-ir": "IR code has count instructions, result indexes one instruction "
                 "and param_count bounds PARAM indices. Arity is an opcode "
                 "helper only; use validate before execution or emission.",
    "kernel-types": "size/vector_bytes return packed host byte sizes. lanes "
                    "counts scalar lanes; GPU alignment/layout is not inferred "
                    "from this multiplication.",
    "kernel-validate": "params needs ir.param_count floats and scratch needs "
                       "ir.count floats; cap counts scratch floats. Output is "
                       "one scalar. Validation checks dominance/types, not finite inputs or budgets.",
    "kernel-uniformity": "uniform_param has ir.param_count bools; out has "
                         "ir.count bools and cap counts bool elements. "
                         "Propagation is conservative straight-line SSA, not barrier-uniformity analysis.",
    "kernel-memory": "Region allocation IDs identify common backing objects "
                     "and offset/length/capacity count bytes. Alias checks "
                     "use IDs/intervals only, not C pointer provenance or actual allocation validation.",
    "kernel-vectorize": "p is parameter-major with ir.param_count*lanes floats "
                        "at p[param*lanes+lane]; out has lanes floats. "
                        "scratch needs ir.count floats; p_cap/cap/out_cap count floats. "
                        "This is scalar batching, not SIMD codegen.",
    "kernel-fusion": "out needs a.count+b.count instruction slots (a conservative "
                     "bound); map needs b.count uint32_t entries. Output/map/"
                     "source code buffers are disjoint. b must have one parameter; no side effects are fused.",
    "kernel-glsl": "Caller owns FILE, uses C numeric locale and validates IR. "
                   "Emitter writes a scalar function; partial I/O output can "
                   "remain on failure. No shader entry point/resources or native compiler is installed.",
    "kernel-wgsl": "Caller owns FILE and sets C numeric locale. Emitter "
                   "uses typed scalar bindings from validated SSA; partial "
                   "output may remain on failure. Native WGSL validation is a separate toolchain step.",
    "kernel-legalize": "code is a mutable ir.count-instruction copy/view and "
                       "must correspond to the IR being folded. Finite constants "
                       "are folded while preserving SSA IDs; no backend capability lowering.",
    "asset-core": "Records borrow NUL-terminated paths/names; identity "
                  "validation only checks the representation's nonzero policy. "
                  "No UUID generation or file content hashing is performed.",
    "asset-catalog": "a has cap records and *n initialized entries. Query "
                     "writes at most cap result indices but can return total "
                     "count beyond cap. Strings remain borrowed and matching is case-sensitive.",
    "asset-dependency": "e has count edges indexed into n assets; dirty "
                        "has n bool entries. Seed dirty flags before fixed-point "
                        "dependent propagation. No resource/cache destruction occurs.",
    "asset-cache": "a has n initialized slots; digest points to exactly "
                   "32 readable bytes. Values are borrowed uintptr_t handles "
                   "keyed with variant; caller owns hashing, eviction and cleanup.",
    "asset-thumbnail": "out must be zeroed/empty; src is a valid frame "
                       "and max_edge a positive pixel bound. Success owns "
                       "an RGBA8 thumbnail released with media_frame_release; no seek/cache is performed.",
    "project-store": "begin/put/commit/rollback guard provider transaction "
                     "lifecycle. put borrows key/value byte spans during "
                     "the callback. Durability, recovery and failure atomicity belong to the provider.",
    "project-journal": "Records are 4-byte little-endian payload length, "
                       "4-byte CRC32 and payload. Append needs 8+payload.size "
                       "free bytes; next returns a borrowed verified payload. CRC is not authentication.",
    "project-migrate": "s has n migration steps; version is mutable "
                       "schema version and target an increasing goal. "
                       "Each callback must be transactional; prior successful steps remain committed.",
    "project-history": "History uses capacity command slots; push records "
                       "an already-applied command. Undo/redo callbacks retain "
                       "borrowed contexts. Branch truncation does not clean discarded command resources.",
    "project-package": "path is a NUL-terminated relative portable member "
                       "name and data borrows bytes for synchronous writer "
                       "dispatch. Validation alone cannot stop symlink escapes, DOS-name collisions or unsafe extraction.",
}


def inventory(root=ROOT):
    section = (root / "tools/media_scope.md").read_text()
    rows = re.findall(r"^\| `(mulib-[^`]+\.h)` \| (.*?) \| (.*?) \|$", section, re.M)
    if len(rows) != 193 or len({row[0] for row in rows}) != 193:
        raise ValueError("section 7 must contain exactly 193 unique headers")
    if set(SUPPLEMENTAL) & {row[0] for row in rows}:
        raise ValueError("supplemental helper duplicates original section-7 inventory")
    rows += [(name, scope, limitations) for name, (scope, limitations) in SUPPLEMENTAL.items()]
    records = []
    for header, scope, limitations in rows:
        if not re.fullmatch(r"mulib-[a-z0-9-]+\.h", header) or header in SUPPORT:
            raise ValueError("invalid bundle header: " + header)
        stem = header[6:-2]
        family = stem.split("-", 1)[0]
        if stem not in CONTRACTS or family not in FAMILIES:
            raise ValueError("missing per-header contract: " + header)
        path = root / "kernels" / header
        source = path.read_text()
        if "static inline " not in source or manuals.IMPLEMENTATION.search(source):
            raise ValueError("expected static-inline-only review header: " + header)
        uncommented = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
        deps = sorted(set(re.findall(
            r'^\s*#\s*include\s*["<](mulib-[^">]+\.h)[">]', uncommented, re.M)))
        for dep in deps:
            if not (root / "kernels" / dep).is_file():
                raise ValueError("missing dependency: " + dep)
        records.append(dict(header=header, scope=scope, limitations=limitations,
                            dependencies=deps))
    return records


def signatures(source):
    # These review headers use ordinary function declarators, not macro-generated
    # declarations. Capture balanced parameter lists through the opening body.
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    result = []
    for match in re.finditer(r"\bstatic\s+inline\s+", source):
        start = match.start()
        end = source.find("{", match.end())
        if end < 0:
            raise ValueError("unterminated inline declaration")
        result.append(re.sub(r"\s+", " ", source[start:end]).strip() + ";")
    return "\n".join(result)


def document(record, records, root=ROOT):
    name = record["header"]
    stem = name[6:-2]
    family = stem.split("-", 1)[0]
    area, common = FAMILIES[family]
    source = (root / "kernels" / name).read_text()
    current_scope = re.search(r"^\s*\*\s*Scope:\s*(.+)$", source, re.M)
    comments = [
        re.sub(r"\s+", " ", re.sub(r"^\s*\* ?", "", text, flags=re.M)).strip()
        for text in re.findall(r"/\*(?!\*)(.*?)\*/", source, re.S)
    ]
    comments = [text for text in comments if text]
    reference_record = dict(header=root / "kernels" / name,
                            example=root / EXAMPLE, text=source, switch=None)
    by_header = {root / "kernels" / row["header"]:
                 dict(manual=root / "manual" / (row["header"][:-2] + ".md"))
                 for row in records}
    limitation = record["limitations"] or (
        "The scope above is the implemented baseline, not a claim of complete format, "
        "numerical, security or application conformance.")
    parts = [
        "# " + name[:-2], "",
        "<!-- Generated by tools/media_inventory.py; edit CONTRACTS or section-7 scope, then --write-docs. -->",
        "", "**Area:** " + area + "  ",
        "**Implementation:** C11 `static inline`; no implementation switch or external ABI export  ",
        "**Status:** bounded review candidate; **not safe On1x admitted**", "",
        "## Purpose and limits", "", record["scope"], "", limitation, "",
        "Retain existing upstream functionality and platform providers; this header is not "
        "a replacement for a complete media/editor/rendering stack.", "",
        "## Units, ownership and capacities", "", CONTRACTS[stem], "", common, "",
        "Capacities describe live backing storage supplied by the caller, not merely numeric "
        "metadata. Check return values before using outputs. A false result is not a universal "
        "no-mutation guarantee; low-level value helpers and bool miss/error results require "
        "the documented preconditions. Bound untrusted counts and work before calling.", "",
        "## Current callable surface", "",
        "Signatures extracted from this header; dependency types and complete checks are in "
        f"[the source](../kernels/{name}). Helpers listed here are not automatically safe foreign imports.", "",
        "```c", signatures(source), "```", "",
    ]
    if current_scope and current_scope[1].strip() != record["limitations"]:
        # Original section-7 limitations are retained for provenance. Make a
        # strengthened current interface explicit instead of implying the old
        # restriction is its only behavior.
        position = parts.index("Retain existing upstream functionality and platform providers; this header is not "
                               "a replacement for a complete media/editor/rendering stack.")
        parts[position:position] = ["Current header refinement: " + current_scope[1].strip(), ""]
    if comments:
        parts += ["## Additional source contracts", ""]
        parts += ["- " + comment for comment in comments]
        parts += [""]
    parts += [
        "## Validation and integration", "",
        "The [shared CPU example](../" + EXAMPLE + ") illustrates a small image/audio/timebase "
        "slice; it does **not** exercise every API in this header. "
        "The [integration oracles](../" + TEST + ") and "
        "[parser boundary probes](../tests/media-parser-boundary-test.c) are representative "
        "regressions, not exhaustive coverage.", "",
        "```sh", "python3 tools/check_media.py", "```", "",
        "The runner checks the 193 original kernels plus the shared PCM helper, isolated and "
        "in aggregate C11 builds with GCC/Clang, including canonical-header composition, "
        "separate-TU wrappers, an expected missing-static-inline-export link failure, a "
        "no-`__int128` path, and ASan/UBSan regressions. It does not validate native GPU/codec/audio "
        "execution, real-world compatibility, C++ or On1x ownership/admission. Explicit foreign "
        "wrappers, independent safety review and platform integration are still required.", "",
        manuals.marker("reference"),
        manuals.reference(reference_record, by_header),
        manuals.marker("reference", True), "",
    ]
    return "\n".join(parts)


def manifest_with_additions(text, records):
    boundaries = list(re.finditer(r"^project_files:[ \t]*$", text, re.M))
    if len(boundaries) != 1:
        raise ValueError("expected exactly one project_files section")
    before, after = text[:boundaries[0].start()], text[boundaries[0].start():]
    blocks = re.findall(r"^- name: .*?(?=^- name: |\Z)", before, re.M | re.S)
    known_headers, known_names = {}, set()
    for block in blocks:
        header = re.search(r"^  header: (.+)$", block, re.M)
        name = re.search(r"^- name: (.+)$", block, re.M)
        if not header or not name:
            raise ValueError("existing manifest entry is incomplete")
        if header[1] in known_headers or name[1] in known_names:
            raise ValueError("duplicate existing manifest name/header")
        known_headers[header[1]] = block
        known_names.add(name[1])
    additions = []
    for record in records:
        header = record["header"]
        name = header[6:-2]
        key = "kernels/" + header
        if key in known_headers:
            old = known_headers[key]
            if not re.search(r"^  implementation: static-inline$", old, re.M) or not re.search(
                    r"^  status: bounded$", old, re.M):
                raise ValueError("refuse to replace a non-bounded existing entry: " + key)
            continue  # Never rewrite working/upstream or concurrent module edits.
        if name in known_names:
            raise ValueError("module-name collision: " + name)
        purpose = record["scope"]
        if record["limitations"]:
            purpose += " Limit: " + record["limitations"]
        else:
            purpose += " Bounded review candidate; no safe On1x admission."
        deps = [dep[6:-2] for dep in record["dependencies"] if dep not in SUPPORT]
        additions.append("\n".join([
            "- name: " + name,
            "  header: " + key,
            "  manual: manual/" + header[:-2] + ".md",
            "  implementation: static-inline",
            "  purpose: " + json.dumps(purpose),
            "  dependencies: [" + ", ".join(deps) + "]",
            "  status: bounded",
            "  example: " + EXAMPLE,
            "  test: " + TEST,
            "",
        ]))
    total = len(blocks) + len(additions)
    bounded = sum(bool(re.search(r"^  status: bounded$", block, re.M)) for block in blocks)
    bounded += len(additions)
    for field, value in [("module_count", total), ("bounded_module_count", bounded)]:
        if len(re.findall(r"^" + field + r": \d+$", before, re.M)) != 1:
            raise ValueError("missing or duplicate manifest count: " + field)
        before = re.sub(r"^" + field + r": \d+$", field + ": " + str(value), before, flags=re.M)
    return before + "".join(additions) + after


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="read-only docs/inventory/manifest check")
    parser.add_argument("--write-docs", action="store_true", help="generate 193 kernel manuals plus PCM helper and JSON")
    parser.add_argument("--write-inventory", action="store_true", help="generate only the JSON inventory")
    parser.add_argument("--sync-manifest", action="store_true", help="explicitly add missing bounded modules")
    args = parser.parse_args()
    if not any((args.check, args.write_docs, args.write_inventory, args.sync_manifest)):
        parser.error("choose --check, --write-docs, --write-inventory or --sync-manifest")
    if args.check and (args.write_docs or args.write_inventory or args.sync_manifest):
        parser.error("--check cannot be combined with a write option")
    try:
        records = inventory()
        generated = {ROOT / "manual/media-kernels.json": json.dumps(records, indent=2) + "\n"}
        if args.write_inventory:
            for path, content in generated.items():
                path.write_text(content)
        generated.update({ROOT / "manual" / (r["header"][:-2] + ".md"):
                          document(r, records) for r in records})
        if args.write_docs:
            for path in generated:
                if path.exists() and path.suffix == ".md" and (
                        "<!-- Generated by tools/media_inventory.py;" not in path.read_text()):
                    raise ValueError("refuse to overwrite an unfamiliar manual: " + str(path))
            for path, content in generated.items():
                if not path.exists() or path.read_text() != content:
                    path.write_text(content)
            print("Generated 194 bounded manuals (193 original kernels + PCM helper) and JSON inventory.")
        manifest_path = ROOT / "MANIFEST.yaml"
        current = manifest_path.read_text()
        updated = manifest_with_additions(current, records)
        if args.sync_manifest:
            for path in [ROOT / EXAMPLE, ROOT / TEST, *generated]:
                if not path.is_file():
                    raise ValueError("missing integration artifact: " + str(path))
            if current != updated:
                manifest_path.write_text(updated)
            print("Manifest synchronized additively; existing entries/project_files preserved.")
        if args.check:
            stale = [str(p.relative_to(ROOT)) for p, content in generated.items()
                     if not p.is_file() or p.read_text() != content]
            if updated != current:
                stale.append("MANIFEST.yaml (explicit --sync-manifest required)")
            failures = manuals.check_links({p: content for p, content in generated.items()
                                            if p.suffix == ".md"})
            if stale or failures:
                raise ValueError("\n".join(["stale/missing: " + p for p in stale] + failures))
            print("Exact 193-kernel inventory + PCM helper, manuals, links and manifest registration passed.")
        return 0
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
