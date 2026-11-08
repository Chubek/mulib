# Installing and building MuLIB

[Overview](README.md) · [Usage guide](GUIDE.md) · [Manuals](manual/README.md)

MuLIB's core is consumed directly from its headers. You can vendor the source
tree, add it to a CMake project, or compile selected implementation files with
your existing build system.

## Requirements

- A C11 compiler and the platform's C development environment. The verification
  tools support GCC and Clang.
- CMake **3.16 or newer** and a supported build tool for CMake builds.
- Python 3 for repository verification and generation tools; it is unnecessary
  for compiling ordinary C consumers. `tools/manuals.py` uses Python 3.9+ APIs.
- A C++17 compiler if running the C++ declaration checks.

There are no mandatory third-party libraries for the core headers. Linker and
runtime requirements depend on the selected modules. Shell examples below use
Unix syntax; commands with `-ldl` target Linux.

## Use the source tree directly

Obtain a checkout or source archive and run these commands from its root:

```sh
cc -std=c11 -Iinclude examples/quickstart.c -lm -o quickstart
./quickstart
```

Expected output:

```text
MuLIB ready
```

For an application that vendors MuLIB under `third_party/mulib/`, add
`third_party/mulib/include` to the compiler's include path. Compile the module
implementations once in your own `.c` file, as shown in [GUIDE.md](GUIDE.md).
There is no prebuilt core archive to link.

Keep the source layout when vendoring:

- Copy `include/` with its support headers, rather than only the top-level
  header for a module with dependencies.
- Retain the sibling `.deprecated/` directory for OCI/imgtracker compatibility
  headers, which forward to preserved implementations there.
- Retain relevant `library/` directories when using compiled runtimes, external
  profiles, or DSL libraries; their manuals specify required assets.
- Include [`LICENSE`](LICENSE) with redistributed source.

The top-level CMake project currently provides no install rules or exported
`find_package` configuration. Use source-tree integration instead of
`cmake --install` for the core library.

## Integrate with CMake

This example assumes `app.c` and `mulib_impl.c` from the
[multi-file JSON example](GUIDE.md#use-mulib-in-multiple-source-files), with the
MuLIB checkout at `third_party/mulib/`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES C)

set(MULIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MULIB_BUILD_MODULE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MULIB_BUILD_SHIRAZ OFF CACHE BOOL "" FORCE)
set(MULIB_BUILD_VIRTIO OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/mulib)

add_executable(app app.c mulib_impl.c)
target_link_libraries(app PRIVATE mulib)
if(NOT WIN32)
    target_link_libraries(app PRIVATE m)
endif()
```

Build from your application directory:

```sh
cmake -S . -B build
cmake --build build --parallel 2
```

The `mulib` interface target supplies include paths, C11 compile requirements,
and `Threads::Threads`. It does **not** emit module implementations or supply
math and dynamic-loader libraries for consumers. Add `${CMAKE_DL_LIBS}` to your
target when its selected modules need dynamic loading.

## Build the repository

From the MuLIB root:

```sh
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The default build includes tests, the separate Virtio libraries, and the Shiraz
runtime on Linux. Adjust the parallel job count to available memory; combined
implementation tests compile large translation units.

### CMake options

| Option | Default | Effect |
| --- | --- | --- |
| `MULIB_BUILD_TESTS` | `ON` | Build tests and register them with CTest |
| `MULIB_BUILD_MODULE_EXAMPLES` | `OFF` | Build standalone module and top-level examples; register them with CTest when tests are enabled |
| `MULIB_BUILD_SHIRAZ` | Unix host: `ON` | Build Shiraz libraries and tools when the target system is Linux |
| `MULIB_BUILD_VIRTIO` | `ON` | Build separate Virtio protocol, backend, and frontend static libraries |
| `MULIB_STRICT_WARNINGS` | `ON` | Enable strict GCC/Clang warnings as errors for test/example targets |
| `MULIB_SANITIZERS` | `OFF` | Enable GCC/Clang AddressSanitizer and UndefinedBehaviorSanitizer instrumentation on test/example targets |
| `MULIB_BUILD_FUZZER` | `OFF` | Build the hostile-input libFuzzer target; requires Clang |

See [`CMakeLists.txt`](CMakeLists.txt) for the complete target configuration.

### Optional compiled layers

- **Shiraz:** Linux PE/COFF execution and translation runtime. CMake builds
  `mulib_shiraz`, `mulib_shiraz_shared`, `mulib_shiraz_win32`,
  `mulib_shiraz_win32_shared`, `shiraz`, and `shiraz-reg`. The
  [runtime README](library/shiraz/README.md) also documents its Makefile build.
- **Virtio:** link `mulib_virtio_os` and/or `mulib_virtio_guest` for the backend
  and frontend; they depend on `mulib_virtio_protocol`. Follow the
  [Virtio integration manual](manual/mulib-virtio-os.md) and
  [device protocol](MULIB_VMMPROTO.md) for implementation placement and device
  coverage.
- **YANTLR standard library:** `mulib_yantlr_stdlib` is an opt-in interface
  target. Its implementation placement and provider registration are described
  in [`library/yantlr/README.md`](library/yantlr/README.md).

## Platform libraries and runtime backends

| Feature | Requirement |
| --- | --- |
| Numerical modules | `-lm` on platforms with a separate math library |
| Native threads and task pools | `-pthread` on POSIX; propagated by the CMake `mulib` target |
| Dynamic library loading | Platform loader APIs; commonly `-ldl` on Linux or `${CMAKE_DL_LIBS}` in CMake |
| Unicode normalization, names, bidi and boundaries | Runtime-loaded ICU |
| Font discovery, shaping and drawing | Runtime-loaded Fontconfig, HarfBuzz and FreeType, respectively |
| Wayland/Xlib connections | Their native client runtimes, loaded on demand |
| Linux device modules | Relevant kernel interfaces, devices, and access permissions |
| Native JIT, Dynbox, firmware and KVM | The module's documented architecture, ABI, and host support |

Text backends load their libraries without requiring their development headers
or link-time dependencies. Missing backends return explicit failures. See
[text-stack verification](manual/text-hardening.md) for backend checks.

Platform support is module-specific; CMake support for a compiler or host does
not imply that every device or native backend works there. For C++ applications,
compile implementation files as C11 and link them with the C++ application as
shown in the [guide](GUIDE.md#use-mulib-from-c).

## Verification

Check documentation inventory, generated references, and manual links:

```sh
python3 tools/manuals.py --check
```

Build all examples as well as tests:

```sh
cmake -S . -B build-examples -DMULIB_BUILD_MODULE_EXAMPLES=ON
cmake --build build-examples --parallel 2
ctest --test-dir build-examples --output-on-failure
```

Run the independent strict checker, including examples, focused tests,
standalone headers, combined implementations, and C/C++ declarations:

```sh
python3 tools/check.py
```

For Clang and sanitizers:

```sh
CC=clang CXX=clang++ python3 tools/check.py --sanitizers
```

Or enable sanitizers in a separate CMake build:

```sh
cmake -S . -B build-sanitize -DMULIB_SANITIZERS=ON
cmake --build build-sanitize --parallel 2
ctest --test-dir build-sanitize --output-on-failure
```

Focused stack checkers include `tools/check_graphics.py`,
`tools/check_storage_server.py`, `tools/check_container_stack.py`,
`tools/check_virtio.py`, `tools/check_osdev.py`, and `tools/check_prowse.py`.
Consult each tool's `--help` and the corresponding manual for options and
optional interoperability tools. To compile all manual examples without running
them, use `python3 tools/manuals.py --compile-examples`.

Ordinary builds use the checked-in architecture metadata snapshot.
`python3 tools/check_snapshot.py` validates it without third-party packages;
full regeneration optionally uses developer-only PyYAML and jsonschema.

## Troubleshooting

| Symptom | Resolution |
| --- | --- |
| Header not found | Add the MuLIB `include/` directory to the include path and preserve support headers |
| Undefined `mulib_*` symbol | Link one implementation file with the module's required switches and dependency implementations |
| Duplicate `mulib_*` symbols | Emit each implementation in only one translation unit; remove global implementation compile definitions |
| Undefined math, thread, or loader symbols | Add the required platform libraries, after source/object files where link order matters |
| Missing OCI/imgtracker include | Preserve `.deprecated/` alongside `include/` |
| C++ implementation errors | Compile the implementation as C11 and link the resulting object with C++ |
| Example skips operations in Release | Keep assertions enabled; use `-UNDEBUG` for GCC/Clang when compiling repository examples manually |
| Runtime backend unavailable | Check the module's supported platform and install its required native runtime |
