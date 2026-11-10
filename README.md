# MuLIB

MuLIB is a modular C11 utility library with **1080 registered modules**, from
containers and parsers to compiler tooling, graphics, networking, and virtual
machines. Choose the modules you need and compile their implementations into
your application.

Most modules are single headers with an explicit `MULIB_IMPL_<MODULE>` switch.
Optional compiled runtimes and supporting profiles live in `library/`. The core
has no mandatory third-party library dependencies; individual modules may use
native operating-system APIs or optional runtime-loaded libraries.

[Installation](INSTALL.md) · [Usage guide](GUIDE.md) ·
[Manuals](manual/README.md) · [Complete library index](manual/index.md)

See the [compiler and VM guide](manual/compiler-tools.md) for the expression,
control-flow, register-allocation, and bytecode modules and their checked examples.

## Quick start

Save this as `hello.c` in the repository root:

```c
#define MULIB_IMPL_VEC
#include <mulib-vec.h>
#include <stdio.h>

int main(void)
{
    mulib_vec_t values;
    int value = 42;
    mulib_vec_init(&values, sizeof value);

    if (!mulib_vec_push(&values, &value)) {
        mulib_vec_free(&values);
        return 1;
    }

    printf("%d\n", *(int *)mulib_vec_at(&values, 0));
    mulib_vec_free(&values);
    return 0;
}
```

Compile and run with a C11 compiler:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude hello.c -o hello
./hello
```

This prints `42`. For a ready-made multi-module example:

```sh
cc -std=c11 -Iinclude examples/quickstart.c -lm -o quickstart
./quickstart
```

Define each implementation switch in **exactly one translation unit** and
include declarations normally elsewhere. Modules can depend on other modules;
use the implementation-switch recipe in the module's manual or example. See
the [usage guide](GUIDE.md) for a complete multi-file application.

## Explore the library

| Area | Starting points |
| --- | --- |
| Containers and memory | [Vectors](manual/mulib-vec.md), [maps](manual/mulib-map.md), [arenas](manual/mulib-arena.md), [allocation tracking](manual/mulib-memtkx.md) |
| Parsing and configuration | [Parsec](manual/mulib-parsec.md), [ASTs](manual/mulib-ast.md), [JSON](manual/mulib-json.md), [YAML](manual/mulib-yaml.md), [TOML](manual/mulib-toml.md) |
| Languages and compilers | [Mune](manual/mulib-mune.md), [MicroCC](manual/mulib-microcc.md), [MetaPP](manual/mulib-metapp.md), [ArchML](manual/mulib-archml.md), [YANTLR](manual/mulib-yantlr.md) |
| Files, storage and binary formats | [VFS](manual/mulib-vfs.md), [SQLite-file engine](manual/mulib-sqlite.md), [binary formats](manual/binary-formats.md), [storage/server guide](manual/storage-server.md) |
| Processes and task execution | [Exec](manual/mulib-exec.md), [Git](manual/mulib-git.md), [PolyTask](manual/mulib-polytask.md), [BuildGraph](manual/mulib-buildgraph.md), [distributed computing](manual/mulib-distribcomp.md) |
| Networking and automation | [HTTP](manual/mulib-http.md), [DNS](manual/mulib-dns.md), [authentication](manual/mulib-auth.md), [Prowse browser automation](manual/mulib-prowse-control.md) |
| Terminals and editors | [Readline](manual/mulib-readline.md), [editing buffers](manual/mulib-edbuf.md), [syntax highlighting](manual/mulib-termsyn.md), [terminal UI](manual/mulib-tui.md) |
| Graphics, text and layout | [Linear algebra](manual/mulib-linalg.md), [vector graphics](manual/mulib-vectgfx.md), [ShaderX](manual/mulib-shaderx.md), [font shaping](manual/mulib-fontshape.md), [page layout](manual/mulib-pglayout.md) |
| Publishing baselines | [Integration and limitations](manual/publishing-integration.md), [shared primitives](manual/mulib-typecore.md), [document IR](manual/mulib-docir.md), [cross-target example](examples/build-publication.c) |
| Semantic publishing | [Owned DocIR pipeline](manual/docir-publishing.md), [end-to-end example](examples/docir-publication.c), [source-to-LaTeX/HTML/EPUB contracts](manual/docir-publishing.md#format-contracts-and-tests) |
| Documentation | [Docstrings and DocstrIL](manual/mulib-docstring.md), [Lua extensions/filters](library/ldocstr/README.md), [Pandoc AST](manual/mulib-pandocast.md), [graphs](manual/mulib-gvizz.md), [sequence diagrams](manual/mulib-dyegram.md) |
| Notation and visualization | [Tables](manual/mulib-tbl.md), [symbols](manual/mulib-symbol.md), [math/logic equations](manual/mulib-eqn.md), [chemistry](manual/mulib-chem.md), [Lua-extensible charts](manual/mulib-chart.md), [paleography and script fonts](manual/mulib-paleography.md) |
| Algorithms and numerical methods | [Statistics](manual/mulib-stats.md), [pricing and SAT/SMT](manual/mulib-pricing-suite.md), [geometry constraints](manual/mulib-geosolve.md), [Cassowary](manual/mulib-cassowary-solver.md), [statistical/text fixtures](library/statistical-text/) |
| Native runtimes and systems | [Dynbox](manual/mulib-dynbox.md), [Shiraz](library/shiraz/README.md), [container stack](manual/container-stack.md), [VMM](manual/mulib-vmm.md), [OIX](manual/mulib-oix.md), [boot manager](manual/mulib-bootmgr.md) |

The [module guide](manual/modules.md) groups related APIs, the
[dependency strata](manual/strata.md) describe their composition, and
[`MANIFEST.yaml`](MANIFEST.yaml) records module names, switches, and dependencies.
Each implemented module has a manual and a runnable example in
[`examples/modules/`](examples/modules/) or, for the publishing additions,
`examples/mulib-*.c` as recorded in its manifest entry. These additions implement
documented subsets, not a complete TeX/LaTeX engine or every named standard.

## Supported profiles

Each manual defines the module's supported syntax, platforms, ownership rules,
and limits. Format and protocol names describe the area of functionality;
check the documented profile when you need standards compatibility. For example,
the SQLite-file engine implements a defined SQL/storage subset, and HTTP/2,
HTTP/3, and QUIC modules provide bounded frame codecs.

Native calls, JIT backends, firmware, and device access have architecture-specific
requirements. Network transports and authentication policies are often supplied
by the application. The Unicode/font stack loads optional ICU, Fontconfig,
HarfBuzz, and FreeType runtimes on demand. See [installation](INSTALL.md) and the
individual manuals for backend requirements.

## Extensive examples and test coverage

Build and run the repository test suite:

```sh
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

[INSTALL.md](INSTALL.md#verification) covers standalone examples, strict checks,
sanitizers, and focused stack verification. The [hardening guide](manual/hardening.md)
describes allocation-failure checks, malformed-input tests, and optional fuzzing.

## Repository layout

| Path | Contents |
| --- | --- |
| `include/` | Public single-header modules and support headers |
| `library/` | Compiled runtimes, architecture data, DSL libraries, and profiles |
| `examples/` | Quick starts, composition examples, and module examples |
| `manual/` | API contracts, source references, and stack guides |
| `tests/` | Functional, integration, interoperability, and regression tests |
| `tools/` | Verification, documentation, and generation tools |
| `.deprecated/` | Preserved implementations used by public compatibility headers |

## Contributing and license

See [CONTRIBUTING.md](CONTRIBUTING.md) for module requirements and validation,
and the [manual authoring guide](manual/authoring.md) for documentation conventions.

MuLIB is [MIT-licensed](LICENSE). Its code is original, inspired by CTL and Klib
rather than vendored from their reference archives.
