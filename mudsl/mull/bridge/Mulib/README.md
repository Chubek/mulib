# MuLIB bindings for MuLL

```mull
mulib Parsec <- G:MulibWrap("Parsec")
```

`mulib` is a distinct value kind in both MuLL implementations. Its declaration
creates an immutable binding and checks the initializer's type. A semicolon is
optional at the declaration's end-of-line, closing brace, or end-of-source.
Native data referenced by the binding can still be mutated by library calls.

`Inventory.json` covers all public header names and records C function
declarations visible in the Linux compiler profile. It separates `declarations`
from `functions`, the subset exported by the C++ and D hosts' shared bridge.
`LegacyExports.json` pins the existing **5,373 raw functions**, **8,927 type
descriptors**, and **2,802 constants**. These are compiler-typed C calls, not
safe foreign bindings. Load only trusted code: raw calls still require the
caller to obey C contracts for lengths, ownership, callbacks and effects.
This catalogue is not an On1x admission mechanism.

Newly discovered APIs do not get wrappers, callbacks, storage constructors, or
constants. A `discovery-only` library exposes metadata (`LibraryName`, `Header`,
`BindingProfile`, `Symbols`, `Libraries`); its `Symbols` is empty. A library can
remain `legacy-raw` while new declarations in its header remain unavailable.
For example, `media-frame` is metadata-only; `Parsec` retains its raw API.

MuW3, its Mune Weblet engine, and MicroChan are registered as `muw3`, `muw3-mune`
and `microchan`. Their complete C APIs, option records and callback types are
available through the same wrappers; see the MuLIB module manuals for ownership.
Native TLS and its dependencies are registered as `tls`, `x509`, `signature`,
`der`, `x25519`, `aes-gcm`, `hkdf`, and `entropy`.

OpenAI protocol support is registered as `openai`, `openai-client`, and
`openai-server`, with the incremental event codec as `sse`. The wrappers expose
all operation metadata, client/server records, multipart and live-streaming
callbacks, pagination and tool-workflow APIs. See the
[client](../../../../manual/mulib-openai-client.md) and
[server](../../../../manual/mulib-openai-server.md) manuals and the shared
`tests/mull/mulib/openai.mull` fixture for ownership and examples.

The CMake bridge enables `MULL_MULIB_NATIVE_FONTSHAPE` by default, making the
explicit native fontshape constructors available alongside the unchanged
HarfBuzz default. Set this option OFF to omit that implementation. The native
backend is a [bounded ASCII/Latin profile](../../../../manual/mulib-fontshape.md#native-latin-implementation),
not complex-script shaping or a general replacement for the runtime backend.

`MULL_MULIB_DOCIR` defaults ON for the owned tree/validation/normalization/
serialization/source-span and LaTeX/HTML/EPUB adapters. It enables
`MULIB_ENABLE_DOCIR` in the implementation TU; disabling it preserves the legacy
APIs while tree adapter calls report unsupported. See the
[DocIR ownership and publication contract](../../../../manual/docir-publishing.md).

## Build and install

Shiraz's header-only loader/interpreter and separately compiled Windows services
are both exposed by `G:MulibWrap("Shiraz")`. The bridge builds `runtime.c`,
`kernel32.c` and `registry.c` from the accompanying `library/shiraz` directory.
Set `MULL_SHIRAZ_SOURCE_DIR` if those sources are located elsewhere. The
[Shiraz manual](../../../../manual/mulib-shiraz.md) covers PE/COFF execution,
C/ELF translation and registry operations; both C++ and D hosts use this runtime.

From the MuDSL root:

```sh
cmake -S . -B build -DMULL_ENABLE_MULIB=ON \
  -DMULL_MULIB_INCLUDE_DIR=/mnt/nvme/mulib/include
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

The option defaults on when CMake finds MuLIB headers. It builds
`libmull-mulib-bridge.a` and exports `MuDSL::Mulib`; `MuDSL::MuDSL` links it
automatically in an enabled build. The snapshot targets the supplied Linux
headers, with GCC/Clang C11 extensions, pthreads, libdl, and libm. Clang's Python
bindings are needed only for regeneration, not normal builds.

C++ applications using the CMake target can call the usual
`mull::stdlib::install(runtime)` to install all packages including `Mulib`.
For explicit installation, include `mull/bridge/Mulib/Native.hpp`, link the
bridge, and call `mull::stdlib::installMulib(runtime)`.

D applications can import `mull.bridge.Mulib.Native` and call
`installMulib(runtime)`. To include it in the aggregate standard-library installer:

```sh
ldc2 -I. -d-version=MULL_ENABLE_MULIB \
  examples/27-mull.d MuDSL.d mull/stdlib/Stdlib.d \
  mull/mullier/*.d mull/native/*/Native.d mull/bridge/Mulib/*.d \
  build/libmull-mulib-bridge.a -L-lpthread -L-ldl -L-lm -of=mull-example
```

Use `-version=MULL_ENABLE_MULIB` with DMD or `-fversion=MULL_ENABLE_MULIB`
with GDC and their corresponding linker options. `G:MulibWrap` itself needs
no package search path once the provider is installed. Importing the helper
package uses the ordinary standard-library search root:

```mull
val Mulib <- G:LoadLib("Mulib") or G:DieWith("Import Error: $E?");
mulib Parsec <- Mulib:Wrap("Parsec");
val names <- Mulib:Libraries();
```

## Library and member names

Library names ignore ASCII case; unique punctuation-folded aliases also work:
`Parsec`, `parsec`, and `mulib-parsec.h` identify the same wrapper. Exact library
names take priority, preserving distinct libraries such as `container-stub` and
`containerstub`. Unknown or ambiguous aliases raise an error.
Retained legacy C functions and constants are accessible by exact C name.
Exported functions also have prefix-stripped, case/punctuation-insensitive aliases:

```mull
mulib P <- G:MulibWrap("Parsec")
val a <- P:mulib_parsec_byte(65);
val b <- P:Byte(65);
G:Assert(a:kind == P:MULIB_PARSEC_BYTE);
```

Use exact C names to resolve alias collisions or names reserved by helpers.
`library:Symbols` lists the exact exported names; `BindingProfile` is
`legacy-raw` or `discovery-only`. `New`, `Array`, `Buffer`, `Callback`, and
`SizeOf` are available only on legacy wrappers and in the global `Mulib`
helper package; they do not support discovered types.

Legacy exports include selected enum values, public global constants and
object-like numeric/string macros. `preprocessor_only` lists probed legacy
preprocessor constructs that are not runtime constants. Discovery records are
not portable ABI descriptions: even two headers can declare incompatible
functions with the same spelling.

## Parsec example

```mull
mulib Parsec <- G:MulibWrap("Parsec")
val digit <- Parsec:Range(48, 57);
val digits <- Parsec:Repeat(digit, 1, 100);
val grammar <- Parsec:Sequence([digits, Parsec:End()], 2);
val result <- Parsec:New("result");
G:Assert(Parsec:Run(grammar, "12345", 5, nil, result) == 1);
G:Assert(result:consumed == 5);
Parsec:ResultFree(result);
```

Native function arguments follow the C declaration, including explicit byte
lengths, capacities, output pointers, and status returns. MuLL numbers map to
range-checked native scalars; Booleans map to 0/1. Native integer results beyond
the exact MuLL integer range (±9,007,199,254,740,991) use decimal strings.
Decimal strings also initialize exact 64-bit integers. Nonfinite results fail.

## Native storage

| Operation | Result |
| --- | --- |
| `New("uint32_t", 42)` | Zero-initialized scalar/struct storage with optional initializer |
| `New("mulib_parsec_result_t")` | Native output struct |
| `New("void *")` | Null pointer storage, usable as a `void **` output argument |
| `Array("uint32_t", [1, 2, 3])` | Contiguous typed native array |
| `Array("uint32_t", 10)` | Ten zeroed elements |
| `Buffer(64)` | 64 zeroed bytes |
| `Buffer("hello")` | Copied bytes plus a terminating NUL |
| `SizeOf("uint32_t")` | Native compiler's byte size |

Types accept full C typedef names, primitive aliases, pointer spellings already
present in the API, and local short aliases such as Parsec's `"result"`.
Public type members are also constructors: `P:mulib_parsec_result_t()`.
Records initialize structs by field name; lists initialize fixed arrays or
pointer arguments. Strings passed to native pointers are copied and NUL-terminated;
embedded NUL bytes remain present. `nil` is a null pointer. Returned null pointers
become `nil`; other pointers and structs are `mulib` values.

Native values expose:

- `value:field`: read a native field; nested structs/unions remain live views.
- `value:Set("field", replacement)`: write a field.
- `scalar:Get()` / `scalar:Set(value)`: read/write scalar storage.
- `array[i]`, `array:At(i)`, `array:Set(i, value)`, `array:Count`.
- `pointer:View(count)`: declare the accessible element count of a returned
  pointer. A raw native pointer initially has a one-element view.
- `pointer:As("uint32_t", count?)`: typed view of native memory, useful for
  `void *` results. Checks alignment and known buffer bounds.
- `buffer:String(length?)`: copy bytes; without a length, stop at NUL or the
  known buffer end. An external C string must be NUL-terminated within 1 MiB.
- `value:Type`: native type name. `G:Type(value)` remains `"mulib"`.

Const-qualified pointees and read-only native fields retain read-only views;
attempts to mutate them or pass them as mutable pointer arguments raise errors.

Wrapper allocations and copied arguments live with the provider, including
arguments borrowed by native functions after a call. The default storage budget
is 64 MiB per installed provider; pass another byte budget to `installMulib`.
Temporary argument/result storage also counts toward this budget.

Memory owned by MuLIB retains its original ownership contract: call the native
`Free`, `Destroy`, or corresponding release function exactly as the C API
requires. Wrapper-owned `New`/`Array`/`Buffer` storage is released by the provider;
native destructors may release its internal resources, not the backing allocation
itself. Borrowed pointers/views cease to be valid when their native owner is
freed or reallocates. Keep the C++ runtime alive while using its wrapped values.
External pointer lengths, capacity arguments, and resource lifetimes follow the
native library's contract; the bridge cannot infer them from a C pointer.

## Callbacks

```mull
mulib P <- G:MulibWrap("Parsec")
val predicate <- P:Callback("mulib_parsec_predicate_fn",
    fn(byte, user) { return byte >= 48 and byte <= 57; });
val parser <- P:Satisfy(predicate, nil, "digit");
val result <- P:New("result");
G:Assert(P:Run(parser, "7", 1, nil, result) == 1);
P:ResultFree(result);
```

A MuLL function can also be passed directly where a C function-pointer parameter
is declared. Explicit callbacks can be reused and called from MuLL. Compiler-typed
trampolines support native aggregate parameters/results. There are eight live
callback slots per native signature across the process; slots are reclaimed
when their provider is destroyed. Reuse explicit callback values for repeated
calls. Callback exceptions are caught at the C boundary and reported after the
native call returns. Callbacks must run synchronously on the runtime's thread;
native users must stop using them before provider destruction.

## Regenerate

```sh
python3 scripts/generate_mulib.py --include ../include
python3 scripts/generate_mulib.py --include ../include --check
```

The generator discovers additional headers in isolated declaration TUs, so
declarations with incompatible C signatures need not coexist. It rejects parse
errors and changes to the pinned legacy wrapper code. It emits `Generated.inc`,
`Implementation.c`, and `Inventory.json`. CMake compiles the legacy switches in
one implementation TU, separately from the bridge's declaration TU. The
companion Shiraz services remain separately built. Shared fixtures exercise
discovery-only rejection, legacy calls, Parsec,
buffers, integers, pointer outputs, containers, callbacks, and error cases in
both runtimes.

The dependency-free freshness check runs in both MuLIB and MuDSL CTest suites:

```sh
python3 scripts/check_mulib_inventory.py --include ../include
```

It verifies header/library coverage, the callable policy, the complete `include/`
fingerprint including local detail inputs, and the generated C artifacts' hashes.
Regenerate after a header input changes, then use `--check` for a compiler-backed
reproduction of the snapshot. Regeneration here was verified on Linux x86-64
using Clang/libclang 22.1.8. Do not edit the policy to make newly discovered
symbols callable without a separate review.
The `ebooks.mull` fixture exercises EPUB, ZIP, DEFLATE, MOBI, AZW3, PDF and MathML
through both C++ and D hosts, including native storage and explicit cleanup.
