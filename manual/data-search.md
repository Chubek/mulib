# Data, search, and formal-concept modules

## Integration and ownership

These APIs are C11 single headers. Define each required `MULIB_IMPL_*` switch
in exactly one translation unit **before** including the headers. Dependencies
need their own implementation switches; the standalone module examples show
complete configurations. Other translation units include declarations only.
Link `-lm` for mathematical functions on systems that require it.

Initialize owned structures with `{0}`. Set configuration fields such as vector
dimension or lattice attribute count before use. Do not shallow-copy owners,
modify internal counts/pointers, or reinitialize live allocations. Use clone
APIs where available. `free` accepts NULL, releases ownership and zeros state;
it may be repeated. Borrowed pointers expire on owner mutation/free unless a
module explicitly says otherwise. No mutable structure is internally synchronized.

Pointer/length inputs require accessible storage for the stated length; NULL
is allowed only where the module permits an empty range. Input and output arrays
must not overlap unless explicitly supported. Output capacities count elements,
except byte-buffer APIs. `SIZE_MAX` error returns are not required-capacity queries.
Check each API's return convention: some use 1 for success, while the memtable
and inverted-index family uses 0. Callbacks must not mutate the object being
traversed or retain temporary borrowed buffers.

## Validation

The focused suites are `tests/test_data_{codecs,records,columns,text,search,
sketches,lattices,indexes,patches,alloc}.c`. They include codec round trips,
all small FCA contexts, an independent edit-distance oracle, set-operation
oracles, numeric extremes, callback termination, and allocation-failure cleanup.
CMake discovers these suites and all `examples/modules/*.c` automatically:

```sh
cmake -S . -B build/data -DMULIB_BUILD_MODULE_EXAMPLES=ON -DMULIB_SANITIZERS=ON
cmake --build build/data -j4
ctest --test-dir build/data --output-on-failure
```

For a single focused suite:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/test_data_search.c -lm -o build/test-data-search
./build/test-data-search
```

Each module manual below includes its complete public declarations, example,
and build command. Statistical sketches are approximate; ranking and vector
algorithms use floating-point arithmetic. FCA enumeration can be exponential
even though a context has at most 64 objects and attributes.
