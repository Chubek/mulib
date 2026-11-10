# Testing the MuLIB networking add-on

## Compiler and VM modules

The eight `test-compiler-*.c` suites cover all 60 compiler/VM additions and patched
modules, including allocation failures, arithmetic boundaries, corrupted bytecode,
exhaustive small-graph dominance, and AST traversal budgets. They are discovered
by Make and CMake. Run `make check-compiler` for separate-translation-unit checks,
C++ linkage, all regression suites, and all 60 standalone examples.

```sh
python3 tools/check_compiler.py --cc clang --cxx clang++ --sanitizers
```

See [compiler integration and contracts](../manual/compiler-tools.md).

`CC=cc ./tests/run.sh` builds and runs the repository's CMake/CTest suites,
including the networking tests. `BUILD_DIR` and `JOBS` select the build directory
and parallelism. Use a separate build directory when changing compilers.
The networking cases include **symbol/link smoke tests for every module**,
packet codec checks, an offline stack datapath, D-Bus mock replies, and
`test_network_hardening.c` / `test_netlink_transactions.c` regressions. They do
not change routes, interfaces, DNS, Wi-Fi or firewall rules.

For Clang and sanitizers:
`CC=clang CXX=clang++ BUILD_DIR=tests/build/clang SANITIZERS=ON ./tests/run.sh`.
`./tests/check-cxx.sh` checks a separately compiled C11 implementation with a
C++17 consumer; CTest also includes that case. `python3 tools/check.py` adds
isolated-header and inventory checks. These checks are not a substitute for
fuzzing, privileged namespace integration, or real-service compatibility tests.

## Data, search, and formal-concept modules

The ten `test_data_*.c` suites cover the 59 data/search additions and patched
modules. CMake/CTest and `tools/check.py` discover them automatically. Coverage
includes exhaustive small FCA contexts, an independent Levenshtein oracle,
codec boundaries, sorted-set operations, numeric extremes, ownership, and
1,152 individually injected allocation failures. Each affected module also has
a checked standalone example in `examples/modules/`.

See [data/search integration and contracts](../manual/data-search.md) for build
commands and links to module manuals. To select just these tests after building:

```sh
ctest --test-dir tests/build -R '^mulib_test_data_' --output-on-failure
```
