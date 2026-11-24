# MuLIB BuildParse

[Manuals](README.md) · [Library index](index.md)

**Header:** `kernels/mulib-buildparse.h`  
**Switches:** `MULIB_IMPL_BUILDPLAN`, `MULIB_IMPL_BUILDPARSE`

BuildParse supplies bounded, language-specific adapters from common build files
to [BuildPlan](mulib-buildplan.md). The supported extractors are based on the
Make, Ninja, CMake, Meson and Bazel adapters in `.sot/buildsys`.

## Supported extraction profiles

| Format | Extracted constructs |
| --- | --- |
| Make | Explicit target/prerequisite rules, escaped words and continuations |
| Ninja | `build outputs: rule inputs` declarations |
| CMake | `add_executable`, `add_library`, `add_dependencies`, `target_sources` |
| Meson | Literal `executable`, `library`, `shared_library`, `static_library` calls and literal dependency lists |
| Bazel | Rule calls with literal `name`, `deps` and `srcs` attributes |

These are dependency extractors, **not interpreters**. They do not evaluate Make
variables, Ninja variables, CMake control flow, Meson expressions, Starlark,
generated files, includes or configuration-time scripts. Dynamic constructs are
ignored rather than guessed. Applications requiring authoritative build semantics
must consume the native build tool's query/file API.

`mulib_buildparse` parses a length-delimited buffer and returns a new owned plan.
`mulib_buildparse_file` is the explicit bounded filesystem adapter. AUTO format
detection uses conservative text heuristics for buffers and known filenames for
files. Select an explicit format when correctness matters.

## Safety and limits

Embedded NULs and malformed supported constructs are rejected with offset,
line and column diagnostics. Configurable limits bound source bytes, targets,
dependency edges, token bytes and nesting. Defaults are intended for ordinary
project files, not unbounded generated input. `*out` must be NULL and remains
NULL on failure.

## Example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
  examples/modules/buildparse.c -o /tmp/opencode/buildparse
/tmp/opencode/buildparse
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-buildparse.h`](../kernels/mulib-buildparse.h) |
| Implementation switch | `MULIB_IMPL_BUILDPARSE` |
| Repository-local includes | [mulib-buildplan](mulib-buildplan.md) |
| Complete example | [`examples/modules/buildparse.c`](../examples/modules/buildparse.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/buildparse.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
