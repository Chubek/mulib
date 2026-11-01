# MuLIB

A C11 modular single-header utility library with **323 implemented modules**.
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

## Integration

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

The term *header-only* here means no prebuilt or separately shipped `.c` files; implementations live in headers behind feature switches. These modules implement focused, documented APIs, not every feature implied by a format's name. In particular, configuration, markup, terminal, protocol, and grammar modules document their supported syntax and limits; they are not claims of full YAML, TOML, XPath, HTML5, CSS, CommonMark, VT, or DBus conformance. Unsupported features must not be mistaken for supported conversions. These modules are not complete substitutes for a production Tcl distribution, POSIX manuals toolchain, JSON Schema validator, full Unicode database or cryptographic engine. `mulib-microtcl.h` `loadall()` registers **all MuLIB MicroTcl built-ins**, not external Tcl packages. Documented support and actual APIs determine feature completeness. All names and responsibilities are enumerated in `MANIFEST.yaml`; every header has a dedicated manual in `manual/` and embedded Doxygen text.

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

Compiler utilities cover typed control-flow MIR, a resumable register VM,
helper-based native JIT and C AOT, rewriting, selection, scheduling,
emission, relocation and a focused native JIT. Architecture metadata comes from
the repository's [`dsymspec/`](dsymspec/) directory, not an external architecture
database. Consult individual manuals for supported instruction encodings and
platforms. A checked-in metadata snapshot keeps builds dependency-free;
`python3 tools/check_snapshot.py` verifies its source/schema hashes. Full
regeneration uses the optional developer-only PyYAML and jsonschema packages.
`mulib-inet6.h` is the canonical IPv6 header; `mulib-inte6.h` remains
compatible. The existing spellings `impaclient` and `dnsnmeserver` are retained.

`mulib-yantlr.h` and `mulib-amd64.h` are reserved filenames.
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

## Graphics and geometry

The graphics stack combines [linalg](manual/mulib-linalg.md),
[SIMD math](manual/mulib-simdmath.md), [computational geometry](manual/mulib-cgeom.md)
and [collision queries](manual/mulib-collision.md) with
[GLX software surfaces](manual/mulib-glx.md) backed by memory or DRM dumb buffers.
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
The explicit support matrix covers all 48 requested IDs: 21 bounded,
service-assisted profiles and 27 unimplemented IDs requiring pinned specifications.
`mulib-vmm.h` adds lifecycle, guest-call dispatch and per-device DMA isolation;
`mulib-vmm-memory.h` validates RAM access, `mulib-vmm-host.h` provides RAM block
storage and system entropy, and `mulib-vmm-kvm.h` runs native Linux/x86 guests.
See [`manual/mulib-vmm.md`](manual/mulib-vmm.md) for integration. Run
`python3 tools/check_virtio.py --sanitizers` for focused verification.

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
