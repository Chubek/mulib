# MuLIB Depfile

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-depfile.h`  
**Switch:** `MULIB_IMPL_DEPFILE`

Depfile parses the GCC/Ninja dependency-file profile used to communicate
compiler-discovered source dependencies to build systems. It is based on
`.sot/buildsys/depfile.py` and supports multiple targets, escaped whitespace,
escaped special bytes, CRLF/LF input, comments and backslash line
continuations. It is not a general Make evaluator.

## Streaming callback

`mulib_depfile_parse` accepts length-delimited text and invokes a callback once
per logical rule. Target and dependency strings are temporary and borrowed only
for that callback. Repeated targets remain repeated rules so the application can
select its own merge policy. A callback's nonzero return value stops parsing and
is propagated unchanged.

`mulib_depfile_parse_file` is the explicit filesystem adapter. The core parser
performs no I/O. Both entry points reject embedded NUL bytes, missing separators,
targetless rules and unterminated continuations. Diagnostics include byte offset,
line, column and a static message.

## Limits and trust boundary

Inputs should be treated as untrusted. The configurable limits bound source
bytes, logical rules, aggregate words and bytes per word. Defaults are 16 MiB,
65,536 rules, 262,144 words and 4095 bytes per word. Zero option fields select
defaults. All arithmetic and allocations are checked.

## Example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude \
  examples/modules/depfile.c -o /tmp/opencode/depfile
/tmp/opencode/depfile
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-depfile.h`](../include/mulib-depfile.h) |
| Implementation switch | `MULIB_IMPL_DEPFILE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/depfile.c`](../examples/modules/depfile.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/depfile.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
