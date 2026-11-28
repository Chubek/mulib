# MuLIB

MuLIB is a collection of reusable C11 kernels, compiled libraries, language
tools, and runtime interfaces. Select individual modules rather than adopting
an application framework. The repository also contains Armory and MuLL, the
On1x compiler and SDK, and the On1xRT embedding interface.

[Usage guide](GUIDE.md) · [Build and installation](INSTALL.md) ·
[Module catalogue](manual/index.md) · [Contributing](CONTRIBUTING.md)

## Kernels and conventions

Public kernels live in `kernels/mulib-*.h`. Most expose declarations on ordinary
inclusion and emit implementations when their documented `MULIB_IMPL_*` switch
is defined in exactly one C translation unit. Some bounded families use
`static inline` definitions instead; these do not automatically provide an
external archive ABI. Dependency implementations are not enabled implicitly.

Each API has its own ownership, capacity, return-value, threading, and platform
contract. Read its manual and runnable example before composing it with another
module. Similar layouts do not make distinct representations interchangeable.

## Explore by task

The [module guide](manual/modules.md), [manifest](MANIFEST.yaml), and
[dependency strata](manual/strata.md) describe the available inventory:

- Containers, allocation, data structures, hashes, numerical methods, and
  statistics.
- Parsers, compiler IR, bytecode, native object formats, and code generation.
- Files, databases, archives, compression, networking, and protocol helpers.
- Text, Unicode, fonts, document models, layout, and publishing.
- Graphics, GLX, ImmGTK/RetGTK, image/color operations, geometry, animation,
  audio, media, and compute provider contracts.
- Circuit and digital simulation, hardware design, verification, and routing.
- Platform interfaces, virtualization, containers, sandbox primitives, and
  catalogue-driven composition.

These names describe families, not blanket conformance or production-readiness
claims. Codec framing is not decoding, a compute adapter is not a native GPU
backend, and a parser or reference algorithm may implement only a bounded
subset. See the [media review](manual/media-review.md) and individual manuals.

## Libraries and language tools

[`library/`](library/) contains separately integrated libraries and runtimes, including
Shiraz, Virtio, YANTLR, and higher-level compositions. Their build and dependency
requirements are separate from the core header model.

[Armory](armory/README.md) supplies language-building facilities;
[MuLL](armory/mull/README.md) includes a native MuLIB bridge. Generated declaration
inventory and callable wrappers are distinct capabilities.

[On1x](on1x/docs/architecture.md) is a statically compiled language with a C11 backend.
Its [SDK](on1x/sdk/README.md) provides compiler extension interfaces, while
[On1xRT](on1x/runtime-sdk/README.md) provides native embedding. These are
different interfaces with different trust and lifecycle contracts. LibStone
integration and optional backends have their own dependency requirements.

The [KernelBind guide](on1x/docs/kernel-bind.md) describes MuLIB declaration
discovery, explicit raw bindings, and review-gated imports. Discovery, native
execution, and independently approved safe admission must not be conflated.
The repository does not grant production approval merely by shipping a test.
See the [FFI migration guide](on1x/docs/ffi-migration.md) for what stays
compatible and what is changing.

[Catalogue-driven design](manual/mulib-cdd-catalogue.md) provides bounded
descriptors, registries, queries, planning, and host-dispatched execution.
Catalogue selection does not authorize arbitrary symbol loading or establish
safe foreign admission; the host retains dispatch and capability policy.

## Quick start

Compile a checked-in example from the repository root:

```sh
cc -std=c11 -Ikernels examples/quickstart.c -lm -o quickstart
./quickstart
```

The [usage guide](GUIDE.md) explains implementation placement, byte containers,
JSON composition, ownership, and C++ consumers. See [INSTALL.md](INSTALL.md) for
CMake integration and optional compiled layers.

## Build, test, and platform limits

```sh
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Run `python3 tools/manuals.py --check` for documentation checks and
`python3 tools/check.py` for the strict kernel checker. Focused family gates,
sanitizers, optional device tests, and dependencies are documented in
[INSTALL.md](INSTALL.md). The compiler and language subprojects also have
their own verification suites.

Support is module-specific. Portable C11 algorithms coexist with Linux/POSIX,
architecture-specific, and provider-backed modules. A successful host build
does not establish cross-platform coverage, live device execution, realtime
safety, or full standards conformance. Historical results and exclusions belong
in the relevant verification manuals, including
[cross-layer integration](manual/integration-verification.md),
[On1x verification](on1x/VERIFICATION.md) and
[sandbox verification](manual/sandbox-verification.md).

## Contribute

Preserve reusable mechanisms, explicit contracts, and existing working APIs.
Add bounded-input and ownership tests, update the manifest and manuals, and
report exactly what was tested. See [CONTRIBUTING.md](CONTRIBUTING.md) and
[LICENSE](LICENSE).
