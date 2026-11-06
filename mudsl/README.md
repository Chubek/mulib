# MuDSL

A single-header C++20 and single-module D toolkit for building languages,
including the embeddable **MuLL** interpreter.

- C++: `#include "MuDSL.hpp"`; toolkit namespace `dsl` (alias `mudsl`).
- D: `import MuDSL;`.
- MuLL: `mull::Runtime` in C++, `MuLL.Runtime` in D.
- Native MuLL packages: [`mull/stdlib`](mull/stdlib).
- [Language and embedding reference](mull/README.md).
- [Toolkit manual](manual/README.md) and [70-chapter book](book/README.md).

```sh
cmake -S . -B build -DBUILD_EXAMPLES=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/mudsl_27-mull mull/stdlib
```

CMake automatically tests the D implementation when LDC, DMD, or GDC is
available. The installed CMake target is `MuDSL::MuDSL`.

```mull
val Printf <- G:LoadLib("printf") or G:DieWith("Import Error: $E?");
val Terminal <- G:LoadLib("terminal") or G:DieWith("Import Error: $E?");
Printf:SetMainSink(G:GetStdout());
Printf:Println("Hello, MuLL!");
```

Initialize a runtime, install the native libraries, and configure package
roots before running this source. See `examples/27-mull.cpp` and
`examples/27-mull.d` for complete hosts.
