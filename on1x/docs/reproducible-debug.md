# Reproducible project debug builds

On1x project profiles that request debug information keep `-g` and use the C
compiler's `-fdebug-prefix-map`. Both normal and bubblewrap builds keep their
private staging directories. Their actual filesystem paths are not changed;
only the paths recorded in DWARF are normalized:

| DWARF prefix | Source |
| --- | --- |
| `/__on1x__/generated` | Generated translation units such as `main.c` and `mulib.c` |
| `/__on1x__/build` | Project staging; `/__on1x__/build/cwd` is the compilation directory |
| `/__on1x__/sdk` | On1x SDK/runtime |
| `/__on1x__/stdlib` | Selected standard library |
| `/__on1x__/mulib` | MuLIB native source and header root |

Debugging is at the generated C level; this is not an On1x source map. Use
`on1x-build.py --emit-only` with the same On1x compiler, input and module search
options to regenerate `main.c` when needed. In GDB, map the virtual source paths
to your local copies with `set substitute-path`, for example:

```text
set substitute-path /__on1x__/generated /path/to/generated-c
set substitute-path /__on1x__/sdk /path/to/on1x-sdk
```

The regression verifies executable byte equality on Linux across equivalent
source and SDK trees and independent staging. It uses the same On1x compiler,
native compiler, assembler/linker, flags, target, sysroot/system libraries, and
build environment for each pair. GCC and Clang can produce different bytes
from one another. The project driver uses a fixed subprocess environment
(including `LC_ALL=C`, `TZ=UTC`, and `SOURCE_DATE_EPOCH=0`); the sandbox does not
pin the host toolchain or sysroot. Paths within the sysroot and paths compiled
into program data (for example, by a C `__FILE__` macro) are not remapped.
Roots passed to the prefix map must not contain `=` because GCC and Clang
parse that separator differently. This change does not cover separately
supplied native objects, arbitrary compiler wrappers, or MuLIB provenance
reports that intentionally bind inputs to absolute paths. Release project
builds and direct single-file builds retain their previous default behavior.

Run the regression against both C toolchains:

```sh
python3 on1x/tests/test_reproducible_debug.py --on1xc build/on1x/on1xc --cc gcc
python3 on1x/tests/test_reproducible_debug.py --on1xc build/on1x/on1xc --cc clang
```

The test compares whole executables and project build markers, checks retained
debug metadata, runs both executables, and checks direct single-file builds.
The sandbox case explicitly skips when bubblewrap or its namespaces are
unavailable.
