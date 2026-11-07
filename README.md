# MuLIB

A C11 modular single-header utility library with **537 implemented modules**.
Inspired by the simplicity of CTL and Klib; code is original, not vendored from the provided archives.

## Correct implementation-switch syntax

The example shorthand `#MULIB_IMPL_STACK` is not valid C preprocessing syntax.
Use `#define MULIB_IMPL_STACK` **before** `#include <mulib-stack.h>`.

```c
#define MULIB_IMPL_SEXPR
#define MULIB_IMPL_MANPAGE
#define MULIB_IMPL_MICROTCL
#define MULIB_IMPL_ARGPARSE
#define MULIB_IMPL_STACK
#include <mulib-sexpr.h>
#include <mulib-manpage.h>
#include <mulib-microtcl.h>
#include <mulib-argparse.h>
#include <mulib-stack.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    mulib_sexpr_t *sexpr = mulib_sexpr_cont();
    mulib_manpage_t *manpage = mulib_manpage_linux();
    mulib_microtcl_t *microtcl = mulib_microtcl_loadall();
    mulib_argparse_t *args = mulib_argparse_load();
    mulib_stack_t *stack = mulib_stack_create();
    /* Use objects, then release in reverse or any safe order. */
    mulib_stack_destroy(stack);
    mulib_argparse_destroy(args);
    mulib_microtcl_destroy(microtcl);
    mulib_manpage_destroy(manpage);
    mulib_sexpr_destroy(sexpr);
    return 0;
}
```

Build: `cc -std=c11 -Iinclude examples/quickstart.c -lm -o quickstart`.
Run tests: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`.

## Architecture descriptions

[`mulib-archml.h`](manual/mulib-archml.md) provides validated architecture DSL
documents, register aliases, structured P-code, feature dependencies, compiler
metadata and instruction encoding/decoding. The
[`library/archml`](library/archml/README.md) profiles cover AMD64, AArch64, RV64,
PowerPC64, MIPS32, AVR32A and WebAssembly MVP
plus selected AMDGPU, ARM A32, NVPTX, SPIR-V, Xtensa, eBPF, DirectX/DXIL,
M68k and Hexagon forms. PTX/DXIL use non-binary operation metadata; other profiles
have explicit concrete-form and extension coverage. PARSEC retains its existing
API while adding bounded-work execution and an explicit parser stack.

## Command execution and Git workflows

[`mulib-exec.h`](manual/mulib-exec.md) provides fork/exec and POSIX spawn,
concurrent pipelines, redirection, binary capture, deadlines and a Mune execution
DSL. [`mulib-git.h`](manual/mulib-git.md) exposes arbitrary Git CLI operations
through that API. The [`library/git`](library/git/) DSL catalogue supplies named
command constructors and detailed runtime string docstrings, with generic dispatch
for additional Git commands and extensions.

## Integration

### Windows binaries on Linux

[`Shiraz`](manual/mulib-shiraz.md) loads AMD64 Windows PE/COFF for bounded
interpretation or helper-based C translation into ELF. Its
[`runtime libraries and Makefile`](library/shiraz/README.md) provide static/shared
ELF runtimes, Win32 console/file/heap imports, a persistent registry and the
`shiraz-reg` application. The initial freestanding instruction/API profile is
documented explicitly. Both MuLL hosts expose it with `G:MulibWrap("Shiraz")`.

### Native calls and terminal editors

[`dynbox`](manual/mulib-dynbox.md) provides dependency-free Linux x86-64 SysV
runtime-typed calls, variadics, natural C structures and native callback closures.
[`dynload`](manual/mulib-dynload.md) adds cross-platform library loading, typed
symbol lookup, native filenames and export helpers. Dynbox explicitly reports
unsupported ABIs; it does not claim all of libffi's architecture/type coverage.

[`edbuf`](manual/mulib-edbuf.md) shares transactional editing, marks and undo/redo
across gap, AVL-rope and piece-table storage. [`termstyle`](manual/mulib-termstyle.md)
maps CSS into shared terminal-cell styles and safe capability-backed output,
preserving its existing indexed API. [`termsyn`](manual/mulib-termsyn.md) adds
incremental line-state lexers, custom backends and CSS token themes over edbuf.
These are byte-oriented buffers and lexical colorizers, not full language parsers
or Unicode editor-layout engines.

### Firmware, initramfs and multicall utilities

[`efi`](manual/mulib-efi.md) handles `.efi` PE/COFF files;
[`uefi`](manual/mulib-uefi.md) binds firmware services and enforces exit lifecycle;
[`bios`](manual/mulib-bios.md) provides PC BIOS thunk and wire-format helpers.
[`uefiutils`](manual/mulib-uefiutils.md) handles device paths, load options and
Shell parameters. [`bootmgr`](manual/mulib-bootmgr.md) routes BIOS/UEFI entries.
[`cpio`](manual/mulib-cpio.md) reads/builds newc/crc archives and
[`initsysutils`](manual/mulib-initsysutils.md) validates/builds uncompressed Linux
initramfs and runs rollback-capable initialization steps.
[`shoebox-contract`](manual/mulib-shoebox-contract.md) defines the future
multicall applet interface; no Shoebox utilities are included yet.
Former firmware APIs in `mulib-efi.h` have moved; see its migration notes.

### Task execution and hermetic build data

[PolyTask](manual/mulib-polytask.md) executes concurrent native
[task plans](manual/mulib-task.md), including executable shebang files and explicit
shell strings, with an owned JSON batch format. [xtbb](manual/mulib-xtbb.md)
adds C11 task arenas, fork/join, parallel for/map and deterministic reductions.
[BuildGraph](manual/mulib-buildgraph.md) supports registered tasks and optional
PolyTask dispatch; [BuildVM](manual/mulib-buildvm.md) supplies a finite hermetic
artifact-only execution profile. [Artifacts](manual/mulib-artifact.md) include
versioned binary file/diff archives with metadata and an offset table. Native
commands and arbitrary callbacks are not sandboxed; only BuildVM's restricted
byte operations enforce the in-memory hermetic profile.

For multi-translation-unit use, define each implementation macro in exactly one `.c` file and include normal declarations everywhere else. Do not define implementation macros in globally distributed compiler flags. Headers should compile standalone. C++ clients should compile at least the implementation TU as C11 unless they specifically test C++ compatibility.

Modules reuse each other. Define the implementation switches for **all transitive
dependencies** before including any of their headers; dependency switches are
never enabled implicitly. Each manual and standalone example lists the required
switches. For example, the JSON parser uses MuLIB parser combinators, UTF-8
validation, and string builders:

```c
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#include <mulib-json.h>
```

YAML, TOML, templates, and JSON-RPC share the JSON value model instead of
maintaining competing trees. Other composition paths include allocation
tracking with memory pools, shared grammar/forest/stack machinery, and markup
builders and queries over a shared document model. See [the module guide](manual/modules.md).

Scannerless [PEG/EBNF loaders](manual/mulib-parsec.md#owned-grammar-loading)
build combinator parsers with named semantic actions and owned
[ASTs](manual/mulib-ast.md). Trees support S-expression, JSON and YAML output,
visitors, listeners and pull events. [Confuse](manual/mulib-confuse.md) adds
libconfuse-style configuration; [lazy streams](manual/mulib-lazystream.md)
provide on-demand map/filter/take/drop/fold pipelines.

## Scope and limitations

[FST](manual/mulib-fst.md) provides deterministic range transducers with optional
symbol emissions. [Machinebuild](manual/mulib-machinebuild.md) parses a small
automaton DSL and generates standalone JavaScript factories with step/reset/run
APIs; it is separate from the compiler-target `mulib-machine.h` module.

Core module implementations live in headers behind feature switches. Separately compiled runtime layers, including Shiraz and Virtio, ship source files under `library/`. These modules implement focused, documented APIs, not every feature implied by a format's name. In particular, configuration, markup, terminal, protocol, and grammar modules document their supported syntax and limits; they are not claims of full YAML, TOML, XPath, HTML5, CSS, CommonMark, VT, or DBus conformance. Unsupported features must not be mistaken for supported conversions. These modules are not complete substitutes for a production Tcl distribution, POSIX manuals toolchain, JSON Schema validator, full Unicode database or cryptographic engine. `mulib-microtcl.h` `loadall()` registers **all MuLIB MicroTcl built-ins**, not external Tcl packages. Documented support and actual APIs determine feature completeness. All names and responsibilities are enumerated in `MANIFEST.yaml`; every header has a dedicated manual in `manual/` and embedded Doxygen text.

There are no mandatory third-party library dependencies. Wayland/Xlib connection
helpers optionally load their native client runtimes; wlroots helpers provide
portable geometry/layout operations, not a compositor ABI binding.
Platform modules use operating-system
APIs and document platform restrictions; dynamic loading may need `-ldl`, and
numerical modules may need `-lm`. DRM refers to Linux Direct Rendering Manager.
Python and Ruby modules **generate ctypes/Fiddle binding source** rather than
embedding or linking an interpreter. Using their generated source naturally
requires the target language runtime and the described C shared library.

The networking extensions share DNS and HTTP models and expose bounded,
transport-independent client/server cores. Applications supply transport and
authentication policy; these modules do not implicitly add TLS or turn protocol
state machines into production-ready Internet services. Task pools and parallel
helpers use native threads; link with `-pthread` on POSIX systems (the CMake
`mulib` target propagates the thread dependency).

The [VCS and network extensions](manual/modules.md#vcs-network-and-ipc-extensions)
add Git object/pack formats, VCDIFF, packet codecs and policy, native IPC, and
composable transports. [Netfs](manual/mulib-netfs.md) carries the existing
FUSE adapter's operations over a network stream and exposes a remote MuLIB VFS
mount. Protocol manuals specify the supported profiles and required crypto
backends; the SSH helpers are identification/packet/authentication codecs.

Authentication helpers include [Basic/Bearer and API keys](manual/mulib-auth.md),
[OAuth2 PKCE and token policy](manual/mulib-oauth2.md),
[HOTP/TOTP with replay protection](manual/mulib-otpauth.md),
[salted password records](manual/mulib-pwdauth.md), and
[IPv4/IPv6 ACLs](manual/mulib-acl.md). `mulib-oauth.h` and `mulib-otpath.h`
provide alternate entry points. Applications still supply authenticated TLS,
cryptographic token verification, OS entropy, rate limits and durable synchronized
replay storage. The dependency-free cryptographic implementation is **unaudited**.

Compiler utilities cover typed control-flow MIR, a resumable register VM,
helper-based native JIT and C AOT, rewriting, selection, scheduling,
emission, relocation and a focused native JIT. Architecture metadata comes from
the repository's [`library/dsymspec/`](library/dsymspec/) directory, not an external architecture
database. Consult individual manuals for supported instruction encodings and
platforms. A checked-in metadata snapshot keeps builds dependency-free;
`python3 tools/check_snapshot.py` verifies its source/schema hashes. Full
regeneration uses the optional developer-only PyYAML and jsonschema packages.
[MicroCC](manual/mulib-microcc.md) adds a complete high-IR-to-assembly pipeline:
Sea of Nodes, ANF, gated/memory SSA, dependency graphs and superblocks lower to
native MIR, with local optimization, costed selection and dependence-safe
scheduling. Built-in targets emit ELF x86-64 SysV and RV64IM assembly, including
PHI destruction, spill slots and fixed-arity ABI calls. See its
[runnable example](examples/modules/microcc.c) and supported native profile.
[MetaPP](manual/mulib-metapp.md) adds token-based C/C++ preprocessing, macro
rescanning, variadics, conditional expressions, configurable includes, and plugins
for directives, pragmas and identifier expansion. Native ELF plugins are relocated
through MuLIB's linker; see the [CLI example](examples/modules/metapp.c) and
[plugin example](examples/plugins/metapp-count.c).
Its dependency index returns the same connected include graph from any member
file, including reverse dependencies, with deterministic Graphviz DOT output.

The [ISEL/OptML pipeline](manual/mulib-optml.md) adds memoized tree covering,
[annotated LIR](manual/mulib-lir.md), ArchML resource-aware scheduling, 75 guarded
[rewrite rules](library/optim/README.md), dead-write elimination and assembly
superinstruction macros. It emits AMD64 SysV or RV64IM assembly to owned text or
caller-provided sinks; see the [OptML example](examples/modules/optml.c).
`mulib-inet6.h` is the canonical IPv6 header; `mulib-inte6.h` remains
compatible. The existing spellings `impaclient` and `dnsnmeserver` are retained.

[YANTLR core](manual/mulib-yantlr.md) parses `.y1` generator specifications,
links explicit providers and evaluates them into staged artifacts. Its opt-in
[standard library](library/yantlr/README.md) adds value utilities, diagnostics,
staged output and PEG/EBNF parsing, not the proposed compiler services.

[Zephyr ASDL](manual/mulib-asdl.md) describes tree schemas through source text
or macro/inline declarations and emits S-expressions, provider-free YANTLR
schema records, and standalone C99 types with inline constructors.
[Vizigen](manual/mulib-vizigen.md) adds named enter/leave semantic actions to
the same schemas, compiling portable bytecode shared by parser-time event
[listeners](manual/mulib-evtlistener.md) and the
[AST walker](manual/mulib-walker.md). Bytecode supports
MuLIB binary readers/writers and zero-copy memory-mapped loading.

[Hyphenation](manual/mulib-hyphen.md) and [spellchecking](manual/mulib-spellcheck.md)
combine configurable dictionaries, orthographic/morphology data, Liang patterns,
finite-state machines and approximate matching. Portable [caches](manual/mulib-cache.md)
offer selectable sorted/HAT-trie/trie indexes; [DFA conversion](manual/mulib-dfa.md)
supports epsilon-NFAs. [Cassowary](manual/mulib-cassowary-solver.md) solves
prioritized linear constraints, while [2D path geometry](manual/mulib-2dgeom.md)
provides Bezier evaluation, splitting, curvature and a Parsec-backed path DSL.

The independent [SQLite-file engine](manual/mulib-sqlite.md) uses parsec for SQL
and separate native record/B-tree and journaled-pager headers. It reads and
writes an explicitly documented SQL/storage profile, not yet full SQLite
compatibility, and never links an external SQLite backend.
The [dependency strata](manual/strata.md) index
records every direct dependency and integration layer. New focused modules include
a bounded i32 WebAssembly runtime (`wask`), an independent Lua runtime with
tree, VM, JIT and C-AOT execution and module bundles (`mune`), inherited
method dispatch (`mop`), and backend-neutral window actions (`wmfunc`). Their manuals
state the supported profiles; these names do not imply full standards coverage.

The [storage, binary, document and container extensions](manual/extension-layers.md)
share existing byte buffers, parsers, ELF views and VFS models. Container modules
provide explicit-backend lifecycle and transport cores, not a privileged isolation
runtime. `tsl` requires an authenticated TLS backend; it supplies no cryptography.

[Binary-format tools](manual/binary-formats.md) provide checked ELF, COFF, PE and
thin Mach-O readers, builders, lookup, container-image packing and shared
objdump-style metadata/hex rendering. Terminal styling is explicitly enabled
through terminfo capabilities; these tools do not load or disassemble programs.

The [storage and server extensions](manual/storage-server.md) add JSON-profile
BSON/CBOR/MessagePack, Protobuf wire fields, schema-checked in-memory stores,
IDL source generators, native binary builders and composable HTTP policies.
HTTP/2, HTTP/3 and QUIC modules are bounded frame codecs, not complete network
stacks. Run `python3 tools/check_storage_server.py` for isolated dependency,
functional and interoperability checks; add `--sanitizers` for ASan/UBSan.

## Graphics and geometry

The graphics stack combines [linalg](manual/mulib-linalg.md),
[SIMD math](manual/mulib-simdmath.md), [computational geometry](manual/mulib-cgeom.md)
and [collision queries](manual/mulib-collision.md) with
[GLX software surfaces](manual/mulib-glx.md) backed by memory or DRM dumb buffers.
[Geosolve](manual/mulib-geosolve.md) adds extensible double-precision 2D/3D
constraint solving, rank/convergence diagnostics and analytic constructions.
[VectGFX](manual/mulib-vectgfx.md) draws paths, curves and splines;
[raytracing](manual/mulib-raytracing.md) renders lit, shadowed and reflective scenes.
[ShaderX](manual/mulib-shaderx.md) offers a typed API and Parsec-backed language
emitting GLSL/HLSL, while [ImmGTK](manual/mulib-immgtk.md) runs immediate-mode
widgets written in Mune. Each module has a standalone example and documented
ownership, numerical conventions and supported language profile.

Run `python3 tools/check_graphics.py` for independent strict graphics checks,
or `CC=clang CXX=clang++ python3 tools/check_graphics.py --sanitizers` for
ASan/UBSan. These include allocation failures, deterministic parser mutations,
scalar/SIMD numerical regressions, C++ declarations and optional GLSL/HLSL
validation with `glslangValidator`.

## Unicode, fonts and page layout

[Uniproc](manual/mulib-uniproc.md), [Uniname](manual/mulib-uniname.md),
[Bidi](manual/mulib-bidi.md) and [Unibreak](manual/mulib-unibreak.md) provide
strict UTF conversion and runtime ICU normalization, casing, names, directional
runs and text boundaries. [Sysfonts](manual/mulib-sysfonts.md) uses Fontconfig
for discovery; [Fontshape](manual/mulib-fontshape.md) and
[Fontdraw](manual/mulib-fontdraw.md) use HarfBuzz and FreeType for OpenType
shaping, glyph outlines and rasterization. Native libraries are loaded on demand;
development headers and link-time dependencies are unnecessary. Missing runtimes
produce explicit failures.

[Distfield](manual/mulib-distfield.md) generates Euclidean glyph-mask SDFs.
[Stylesheet](manual/mulib-stylesheet.md) implements a typed CSS cascade, and
[Pglayout](manual/mulib-pglayout.md) adds owned page/text/image/path objects,
block flow, Unicode wrapping and JSON/YAML source-model persistence. Its font-set
path preserves paragraph bidi context across wrapped lines, itemizes scripts,
selects whole-grapheme font fallback and caches drawable glyph runs. Manuals
specify the supported layout/style profiles and native extension APIs.
Run `python3 tools/check_text.py --require-backends`, or add `--sanitizers`.
Official Unicode corpus checks and backend limitations are described in
[text-stack verification](manual/text-hardening.md).

Public OCI/imgtracker compatibility headers forward to their preserved
implementations in `.deprecated/`, since existing public adapters still use
those APIs. When copying that compatibility group, retain the sibling
`.deprecated/` directory as well as `include/`; no archived source was removed.

## Artifacts, build graphs and watch daemons

[Artifacts](manual/mulib-artifact.md) provide immutable byte values, external
references and application objects reusable across builds and agent workflows.
[Buildgraph](manual/mulib-buildgraph.md) adds declared-input/output DAGs with opaque
Actions, deterministic execution and translation walkers, and transactional output
publication. [FSWatch](manual/mulib-fswatch.md) adds a small watch DSL, native/VFS
snapshot backends and [daemon](manual/mulib-daemon.md) execution. Mune configuration
helpers and the systemd compilation profile live in [`library/ldaemon`](library/ldaemon).

## Distributed computing

[Distribcomp](manual/mulib-distribcomp.md) orchestrates Mune task DAGs with a
required JSON/YAML configuration, explicit node placement, leases, retries and
daemon control. [MuMQ](manual/mulib-mumq.md) provides a bounded topic broker and
nonblocking stream clients with optional internal AES-256/HMAC encryption;
it is a versioned MuLIB protocol, not MQTT. [Cluster adapters](manual/mulib-distribcluster.md)
connect approved container runtimes, OCI lifecycle records and VMM resources.
Profiles and sample configurations live in [`library/ldistrib`](library/ldistrib).
Actions are at-least-once; use trusted programs, idempotent backends and properly
provisioned keys/ACLs. Run the local end-to-end example:

```sh
cc -std=c11 -Iinclude examples/modules/distribcomp.c -pthread -lm -ldl -o distribcomp
./distribcomp
```

## Container stack

The [container stack](manual/container-stack.md) adds transactional rootfs/image
builds, explicit QVM/JIT/OIX/KVM execution adapters, generation-tracked lifecycle
management, bounded IPC and client/server control, pinned image caches, VFS
exports, packet routing and binary logs. ELF and standard Zstandard raw/RLE
packaging round-trip images without mandatory external libraries. Every module
and new `include-mulib-*` dependency has its own manual and runnable example.
Run `python3 tools/check_container_stack.py` for focused strict compilation,
functional tests and codec interoperability; add `--sanitizers` for ASan/UBSan.

## Browser automation and Readline

[Prowse](manual/mulib-prowse-control.md) connects browser automation to Mune's Lua
runtime through `require('lprowse')`, with CSS extraction, interaction, screenshots,
waits, retries and bounded crawlers. Its [protocol layer](manual/mulib-prowse-protocol.md)
supports Chrome CDP, W3C WebDriver for Firefox/Chrome/WebKit, and Firefox Marionette.
[MuLIB Readline](manual/mulib-readline.md) supplies UTF-8 editing, highlighting,
completion and directives for the REPL; [`library/librl`](library/librl/README.md)
contains `.mlsyn`, `.mlac` and `.mldir` profiles for 15 languages.

```sh
cc -std=c11 -Iinclude examples/modules/prowse-control.c -pthread -lm -ldl -o prowse
./prowse --repl
python3 tools/check_prowse.py
```

## VMM device protocol and Virtio

[`MULIB_VMMPROTO.md`](MULIB_VMMPROTO.md) defines the implemented device boundary
for the type-2 MuLIB VMM. [`virtio-os`](library/virtio-os) is the backend;
[`virtio-guest`](library/virtio-guest) is the frontend. `include/virtio.h` exposes
the shared contract, backed by `mulib-vmmdev.h` message/DMA primitives.
The explicit support matrix covers all 48 registered IDs: 26 bounded,
service-assisted profiles and 22 unimplemented IDs requiring pinned specifications.
Supported devices have separate `.c` implementations in both directories; generated
single-header implementations remain available for compatibility.
`mulib-vmm.h` adds lifecycle, guest-call dispatch and per-device DMA isolation;
`mulib-vmm-memory.h` validates RAM access, `mulib-vmm-host.h` provides RAM block
storage and system entropy, and `mulib-vmm-kvm.h` runs native Linux/x86 guests.
See [`manual/mulib-vmm.md`](manual/mulib-vmm.md) for integration. Run
`python3 tools/check_virtio.py --sanitizers` for focused verification.

## Bare-metal OS development

The allocation-free OSDev headers cover [BIOS/EFI adapters](manual/mulib-efi.md),
[boot selection and backend handoff](manual/mulib-bootmgr.md),
[CPU inventory](manual/mulib-hwinfo.md), destructive [RAM tests](manual/mulib-memtest.md),
[GPU-driver stress orchestration](manual/mulib-gputest.md), and
[x86-64 descriptors](manual/mulib-descritors.md). Supporting headers provide
[port I/O](manual/mulib-io.md), [paging](manual/mulib-paging.md),
[PCI discovery](manual/mulib-pci.md), [ACPI validation](manual/mulib-acpi.md), and
[framebuffer patterns](manual/mulib-framebuffer.md). Each has a standalone example.
Privileged instructions are disabled unless explicitly enabled. Firmware mode
transitions, native EFI ABI adaptation and GPU drivers are application-provided;
these modules are building blocks, not complete bootable utilities.

Native EFI ABI bindings and static x86-64 ELF segment loading are implemented.
The stress modules include March C− RAM sweeps and a GLSL compute kernel with
CPU-reference verification. Hardware helpers include TSS initialization, page
table walking, PCI capability traversal, MADT iteration and CPUID cache geometry.

Run `python3 tools/check_osdev.py` (optionally `--sanitizers`) for strict
freestanding compilation, C++ linkage, examples and hosted regressions, or run
the focused hosted regression suite through CTest (`mulib_test_osdev`).
Never run destructive tests on memory containing live data.

## Statistical and text algorithms

The statistical/text modules add discrete Markov and hidden Markov models,
Viterbi/state-beam decoding, Bayesian updating/ranking, regression, z-score
anomalies, kernel mean matching, vector/set/string distances, radix-2 FFT/IFFT,
byte tries, BK/VP metric trees, corpus/n-gram helpers, ASCII Soundex and literal
orthographic rules. First-order term parsing and bounded rewriting share the
existing AST model. Each header has its own manual and standalone example;
the supplied historical filename spellings are preserved.

Original deterministic fixtures live in [`library/statistical-text`](library/statistical-text).
`tests/test_statistical_text.c` checks reference values, brute-force decoding,
metric-tree queries, direct DFT comparisons, malformed inputs and allocation
failures. These are focused algorithms, not Unicode linguistic processing or
a general machine-learning framework.

## Licensing

MuLIB's new source is MIT-licensed, with no code copied from the supplied CTL and Klib reference archives.

## Extensive examples and test coverage

Every module has a dedicated manual in `manual/` and an independently compilable
C example in `examples/modules/`. Run
`python3 tools/check.py` to compile all examples and focused tests with
`-O2 -Wall -Wextra -Wpedantic -Werror`, check headers independently, and test combined implementations
and declaration-only C/C++ inclusion. Use `CC=clang CXX=clang++` for Clang or
add `--sanitizers` for ASan/UBSan.
Optionally use `cmake -S . -B build -DMULIB_BUILD_MODULE_EXAMPLES=ON` to build and run them
under CTest. `tests/functional.c` covers multi-module usage and representative edge cases;
`tests/all_headers.c` checks that all implementations can coexist in a single C translation unit;
`tests/declarations.c` checks declaration-only inclusion.
Focused `tests/test_*.c` suites cover the new libraries. CMake also supports
`-DMULIB_SANITIZERS=ON`; test working directories are isolated for parallel CTest runs.
`tests/composition.c` verifies JSON → YAML → TOML → template composition and
tracked allocation/container/vector usage through a separate implementation
translation unit. `examples/dogfood.c` demonstrates parser-backed JSON feeding
vector algebra.

See [header hardening](manual/hardening.md) for failure-injection tests,
deterministic hostile-input coverage, optional libFuzzer builds and strengthened
ownership/validation contracts. These checks are not a security certification.

[OIX](manual/mulib-oix.md) compiles C, JSON, YAML, TOML, or Confuse configuration
into a tiny standalone x86 OS image, with cooperative tasks, console I/O, and
embedded read-only files. Its optional Linux x86 KVM runner needs no guest Linux,
BIOS, or external runtime library. See [the compiler/runner example](examples/modules/oix.c).
