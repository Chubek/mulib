# The `mull` command

`mull` is a Python 3.9+ command driver with independently compiled **C++** and
**D** workers. C++ (`MuDSL.hpp`) is the default; `--engine=d` uses `MuDSL.d`.
Both workers parse, analyze, lower, and optimize their own input. The D worker
does not delegate compilation to C++.

## Build and install

From the repository root:

```sh
cmake -S mudsl/mull -B build/mull -DMULL_ENABLE_MULIB=OFF
cmake --build build/mull --parallel 4
ctest --test-dir build/mull --output-on-failure
cmake --install build/mull --prefix "$HOME/.local"
```

The standalone entry point defaults to the core build (the MuLIB bridge is
opt-in) and includes the existing toolkit build and tests. The
normal `cmake -S mudsl` entry point also builds the CLI. CMake installs `bin/mull`,
private workers/configuration in `libexec/mull`, native runtime archives in
`lib/mull` (or the configured GNU libdir), package data in `share/mull/stdlib`,
and `mull(1)`/`mull(5)` in the man hierarchy. The installation descriptor is
prefix-relative and works with `cmake --install --prefix` and `DESTDIR` staging.
Add your prefix's `bin` and `share/man` to `PATH` and `MANPATH` when necessary.

Options:

* `MULL_BUILD_CLI=OFF`: retain the toolkit without command-line targets.
* `MULL_BUILD_D=OFF`: omit the optional D worker/runtime; no silent host fallback.
* `MULL_D_COMPILER=/path/to/ldc2`: choose the D compiler. LDC/DMD support both the
  worker and native runtime; GDC currently supports the worker only.
* `MULL_ENABLE_MULIB=ON`: include the existing optional MuLIB provider in both
  hosts, when the accompanying MuLIB headers/runtime sources are available.

Native execution currently targets **Linux x86-64, ELF, System V AMD64**. It
requires `cc` to assemble and the selected C++/D compiler to link. Ordinary
interpreter use does not launch a compiler. `mull --list-engines` reports the
installed workers. There are no Python dependencies outside the standard library.

## Run and inspect

```sh
mull -p -e 'return 6*7;'
mull --engine=d -p app.mull -- input.txt -option
mull --check app.mull
mull --stop-after=mullier app.mull > app.mullier
mull --emit=mu3ac -O0 app.mull > app.mu3ac
mull -S -o app.s app.mull
mull -c -o app.o app.mull
mull --emit=exe -p -o app app.mull
mull --backend=x86-64 -p app.mull
```

`--emit` accepts `ast`, `mullier`, `mu3ac`, `asm`, `obj`, and `exe`.
`--stop-after` accepts `parse`, `mullier`, `mu3ac`, `assembly`, `object`, and
`link`. No compile-only stage executes the entry program or preloads packages.
`--check` still cannot resolve every late-bound name, import, or dynamic type.
Mu3AC output is an inspection format, not a separate source-input mode.

Without an emission option, the interpreter runs the source; the native backend
compiles a temporary executable and runs it. `--run` also runs an explicitly
emitted executable. `-p` prints the final value; otherwise only explicit program
output is written. Process success is zero, regardless of a numeric MuLL return.
`Args` is the immutable list of arguments following `--`, without the filename.
An input file named `-` reads stdin. Interactive mode is a persistent interpreter
with one complete program per line and `:quit` to exit.

`-O0` disables transformations in both IRs, **not semantic analysis**. `-O1` is
the default. See [`../mu3ac/README.md`](../mu3ac/README.md) for the IR, ABI, and
the restrictions imposed by bounded lowering.

## Packages versus native libraries

`-I DIR` prepends a MuLL package root. Explicit roots precede nonempty `MULL_PATH`
entries and the installed stdlib root. `--no-default-paths` omits environment and
installation roots; `--no-stdlib` omits native base-package registration, not G.
`--load NAME` preloads a package at execution time. Native artifacts snapshot
these roots/preloads into their configuration; package files are not embedded
and must remain accessible at those paths. Compiled imported packages currently
use the host's existing interpreter and loader, including cached results and
script/native callbacks.

`-L DIR`, `-l NAME`, and `--link-object FILE` are **native linker** options, not
MuLL discovery roots. To expose a linked symbol to MuLL, explicitly declare its
narrow C ABI:

```c
/* custom.c */
double custom_square(double value) { return value * value; }
```

```sh
cc -c custom.c -o custom.o
ar rcs libcustom.a custom.o
mull --backend=x86-64 -L . -lcustom \
  --extern=Square=custom_square:1 -p -e 'return Square(7);'
```

For shared libraries use `--rpath DIR` or the platform loader configuration.
Foreign functions return `double` and take exactly 0–8 `double` parameters.
C++ definitions need `extern "C"`; wrong signatures are unsafe. The binding
checks MuLL arity/numeric inputs/finite output but cannot infer C signatures.
No arbitrary structs, varargs, pointers, or cross-ABI exceptions are supported.
The existing typed MuLIB wrapper remains a separate, richer provider.

## Diagnostics and trusted configuration

`--help` lists all options, including resource budgets, `--mullier-rules DIR`,
`--mu3ac-rules FILE`, assembler/linker selection, and `--verbose` argument tracing.
Rules, linked libraries, compiler executables, and `--link-arg` are trusted inputs.
Commands are invoked as argument vectors, never shell command strings. Neither
execution backend is a sandbox or a wall-clock timeout.

The CTest CLI suite compares both interpreters and both native runtimes at `-O0`
and `-O1` against the existing language fixtures. It also checks stop stages,
source limits, output/source collisions, persistent REPL bindings, binary/object
emission, argument propagation, unusual paths, actual third-party symbol linking,
and runtime error propagation through native unwind frames.

The authoritative installed references are [`mull.1`](mull.1) and
[`mull.5`](mull.5); the latter is a single standard-library catalogue.
