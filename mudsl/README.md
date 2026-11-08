# MuDSL

A single-header C++20 and single-module D toolkit for building languages,
including the embeddable **MuLL** interpreter.

- C++: `#include "MuDSL.hpp"`; toolkit namespace `dsl` (alias `mudsl`).
- D: `import MuDSL;`.
- MuLL: `mull::Runtime` in C++, `MuLL.Runtime` in D.
- Native MuLL packages: [`mull/stdlib`](mull/stdlib).
- [Language and embedding reference](mull/README.md).
- [Toolkit manual](manual/README.md) and [70-chapter book](book/README.md).
- [Editor addons](addons/README.md): VS Code, Vim/Neovim, Sublime Text,
  JetBrains, and a shared MuLL/MuDSL/MuLIB language server.

```sh
cmake -S . -B build -DBUILD_EXAMPLES=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/mudsl_27-mull mull/stdlib
```

CMake automatically tests the D implementation when LDC, DMD, or GDC is
available. The installed CMake target is `MuDSL::MuDSL`.
When the sibling MuLIB `include` directory is available, CMake also builds and
installs the complete [MuLIB wrapper](mull/stdlib/Mulib/README.md), shared by the
C++ and D runtimes. Set `MULL_MULIB_INCLUDE_DIR` for another location, or
`MULL_ENABLE_MULIB=OFF` for the core-only build.

```mull
val Printf <- G:LoadLib("printf") or G:DieWith("Import Error: $E?");
val Terminal <- G:LoadLib("terminal") or G:DieWith("Import Error: $E?");
Printf:SetMainSink(G:GetStdout());
Printf:Println("Hello, MuLL!");
```

With the MuLIB provider installed:

```mull
mulib Parsec <- G:MulibWrap("Parsec")
val digit <- Parsec:Range(48, 57);
```

Initialize a runtime, install the native libraries, and configure package
roots before running this source. See `examples/27-mull.cpp` and
`examples/27-mull.d` for complete hosts.
