# mulib-shebang-sh

[Manuals](README.md) · [Library index](index.md)

`mulib-shebang-sh.h` is a bounded line-oriented text processor with named macros,
ordered built-in filters, optional [Mune](mulib-mune.md) callbacks, and a POSIX
shebang-compatible CLI. It writes transformed text by default; `--exec` explicitly
selects a program to execute a transformed script.

## Build and minimal use

Define `MULIB_IMPL_SHEBANG_SH` in one C11 translation unit. For standalone text
processing, define `MULIB_SHEBANG_SH_NO_MUNE` consistently in every translation
unit. This switch changes the context layout. POSIX CLI execution needs
`_POSIX_C_SOURCE=200809L` defined before system headers.

```c
#define _POSIX_C_SOURCE 200809L
#define MULIB_SHEBANG_SH_NO_MUNE
#define MULIB_IMPL_SHEBANG_SH
#include "mulib-shebang-sh.h"

int main(void) {
    mulib_shebang_sh_t processor;
    char *text = NULL;
    int rc = mulib_shebang_sh_init(&processor);
    if (rc) return 1;
    rc = mulib_shebang_sh_define(&processor, "NAME", "world");
    if (!rc) rc = mulib_shebang_sh_add_filter(&processor, "upper");
    if (!rc) rc = mulib_shebang_sh_transform(&processor, "Hello ${NAME}!", 1, &text);
    if (!rc) puts(text);
    free(text);
    mulib_shebang_sh_free(&processor);
    return rc ? 1 : 0;
}
```

Mune is enabled by default and needs its implementation/dependency switches.
[examples/shebang.c](../examples/shebang.c) contains a complete CLI build:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/shebang.c -pthread -lm -ldl -o shebang-sh
```

## Context and ownership

`init` initializes fresh storage. Free an existing context before reinitializing
it. Definitions and filter specifications are copied. `transform` returns a
malloc-owned string; free it with `free`. Its output pointer is NULL on error.
The source and output streams passed to `process` are borrowed and never closed.
`free` destroys all definitions, filters and the optional Lua state, then clears
the context; repeated calls are supported.

The context can process multiple lines/files and retain its definitions and Lua
state. It is not internally synchronized: give each concurrent user its own
context or serialize access. Do not recursively process the same context from a
callback or move it after initializing its Lua state, whose bindings retain the
context address.

Configure these public fields before processing:

| Field | Default | Contract |
| --- | --- | --- |
| `max_line` | 1 MiB | Maximum input bytes per line, excluding LF and NUL terminator |
| `max_expansion` | 8 MiB | Maximum bytes at each expansion/filter stage |
| `skip_shebang` | 1 | Remove a first line beginning exactly `#!` |
| `strict_macros` | 0 | Nonzero rejects undefined recognized macros |
| `error` | Empty | Bounded diagnostic for processing/Lua errors |

Zero limits permit only empty text at the corresponding stage. Limits are byte
counts, not character counts, and do not bound total stream size, Lua execution,
or the number of definitions/filters. Callback results are checked before being
copied into the processor. Processing and Lua entry points clear stale diagnostics
when beginning a valid request; always use the numeric return code as authoritative.

## Macros

`define(p, name, value)` adds or replaces an owned definition. Names contain
1–256 ASCII letters, digits or underscores; numeric names support positional
arguments. Values are NUL-terminated text and may be empty.

`lookup(p, name)` returns a borrowed value or NULL if missing. The value remains
valid until that definition is replaced/removed or the context is freed.
`undefine(p, name)` removes a definition; removing an absent valid name succeeds.

- `${NAME}` and `{{NAME}}` substitute a defined value.
- Substitution is one-pass: inserted values are not expanded again.
- `$$` emits one literal dollar sign, so `$${NAME}` emits literal `${NAME}`.
- Unknown complete macros are preserved unless `strict_macros` is set.
- Empty, overlong, or unterminated macro syntax is copied literally.
- A Lua `macro(name)` callback can supply values absent from the definition table.
- Environment variables are not imported automatically.

Substitution is textual, without shell or language-specific escaping. Use the
appropriate quoting filter when the output is intended to represent one quoted
value. CLI `${0}` is the source path (`-` for stdin); `${1}`, `${2}`, etc. are
script arguments. Positional definitions override earlier `-D` definitions;
Lua configuration loaded afterward can override them again.

## Built-in filters

`add_filter(p, spec)` appends a filter; filters run in insertion order **after**
macro expansion and **before** the optional Lua line filter. Unknown filters and
arguments on argumentless filters return `INVALID`.

| Specification | Effect |
| --- | --- |
| `upper`, `lower` | Byte-wise locale-sensitive C case conversion |
| `trim` | Remove leading/trailing C whitespace |
| `squeeze` | Replace each whitespace run with one ASCII space |
| `number` | Prefix the original one-based line number and `:` |
| `strip-comments` | Remove the first `#` and all following text |
| `json-escape` | Escape quotes, backslashes, and control bytes; no surrounding quotes |
| `shell-quote` | Produce a complete POSIX single-quoted shell word |
| `prefix:TEXT`, `suffix:TEXT` | Add literal text; empty text and further colons are allowed |

Comment stripping does not parse quotes. Case conversion is not Unicode case
folding. `json-escape` preserves bytes at least 0x20 except quotes/backslashes;
it does not validate UTF-8. Empty filter output is still a line, not a request
to drop the line.

## Line and stream processing

`transform(p, line, number, &out)` transforms one NUL-terminated string using a
positive one-based number. It does not append a newline or skip a shebang.
An input exceeding `max_line` returns `LIMIT`.

`process(p, source, output)` reads sequentially and transforms each line, removing
its LF before transformation and adding LF back afterward. It preserves a final
unterminated line without adding LF. CR is an ordinary input byte; CRLF therefore
remains CRLF unless a filter such as `trim` removes the CR. Blank lines are
processed, and an empty file produces no output. A skipped shebang still counts
as source line 1, so the next line is numbered 2. A shebang-only file without LF
also produces no output by default.

Binary NUL input is rejected. Source reads, output writes, and the final output
flush are checked. A failure can leave earlier transformed lines in the output;
stream processing is not transactional. In CLI execution mode the interpreter
is only started after the entire transformation succeeds.

## Lua callbacks

`lua(p, source, length)` creates a Mune state lazily and retains it across calls.
`lua_file(p, path)` reads at most 1 MiB from a seekable file. Native bindings are
`define(name, value)` and `add_filter(spec)`; embedded NULs are rejected.

The processor calls these optional globals synchronously:

```lua
define('GREETING', 'hello')
add_filter('trim')

function macro(name)
    if name == 'TARGET' then return 'world' end
    return nil
end

function filter(line, number)
    return tostring(number) .. ': ' .. line
end
```

`macro` runs only for undefined macros; nil/no result leaves them unresolved.
`filter` runs once after built-in filters; nil/no result keeps the current line.
Only the first return value is used. Non-string/non-nil results or non-function
callback globals return `LUA`. Embedded NUL results return `INVALID`, oversized
strings return `LIMIT`, and callback exceptions return `LUA` with a diagnostic.
Lua side effects are retained if a later callback or processing stage fails.

See [examples/shebang.lua](../examples/shebang.lua) and
[examples/script.template](../examples/script.template).

## Command line and kernel shebangs

`mulib_shebang_sh_main(argc, argv)` supports:

```text
-F SPEC, -FSPEC, --filter SPEC, --filter=SPEC
-D NAME=VALUE, -DNAME=VALUE, --define NAME=VALUE, --define=NAME=VALUE
--strict-macros, --keep-shebang
--max-line N, --max-line=N
--max-expansion N, --max-expansion=N
--lua FILE, --lua=FILE
--lua-code SOURCE, --lua-code=SOURCE
--exec PROGRAM, --exec=PROGRAM
--help
--                         end option parsing
SCRIPT|- [ARGS...]          source path or stdin, followed by script arguments
```

Limit values are decimal nonnegative integers with overflow checking. Options
stop at the script; following arguments are positional values even if they begin
with `-`. Use `--` for a source filename beginning with a hyphen.

Linux generally passes an entire optional shebang argument as one argv token.
The CLI recognizes bundled options such as:

```text
#!/absolute/path/shebang-sh -D WORD='hello world' -F upper
${WORD} ${1}
```

Make the file executable and invoke it with an argument to emit, for example,
`HELLO WORLD FRIEND`. The mini-parser supports whitespace separation, single
and double quotes, backslash escaping outside single quotes, and empty quoted
values. It performs no shell expansion. Unclosed quotes/escapes are errors.

Ordinary filenames containing spaces and attached definitions such as
`'-DWORD=hello world'` remain single arguments. Bundles are inferred when the
first argument contains a separated option/value or another option outside
quotes. Some strings are inherently ambiguous between a kernel bundle and a
standalone value. Use separate `-D 'NAME=value'` arguments for such values, or
explicitly request bundle parsing in standalone invocations:

```sh
shebang-sh --shebang-args "-D WORD='hello world' -F upper" script friend
printf 'hello\n' | shebang-sh -F upper -
```

## Explicit interpreter execution

`--exec PROGRAM` writes the transformed script to a private `mkstemp` file,
closes it, and invokes `PROGRAM temporary-path ARGS...` with `execvp`.
`PROGRAM` is one executable name/path, not a command line. PATH lookup applies
to a slashless name. Script arguments retain their argv boundaries.

Temporary files use `$TMPDIR`, falling back to `/tmp`, and are unlinked on normal
completion and handled errors. The interpreter's native `$0`/script filename is
the temporary path; `${0}` macro substitution still refers to the original
source. Preprocessing failures do not execute the interpreter.

The CLI returns 0 for successful text processing, 1 for processing failure, 2
for invalid options/setup input, or the interpreter's exit status. Launch/wait
setup failure uses 126, exec failure uses 127, and signals use `128 + signal`.
Text processing works without POSIX process support; explicit execution does not.

## Status codes and tests

The C API returns `OK=0`, `INVALID=-1`, `NOMEM=-2`, `IO=-3`, `LIMIT=-4`, `LUA=-5`,
or `UNSUPPORTED=-6`. Mune loading returns `UNSUPPORTED` in `NO_MUNE` builds.

Run `sh tests/run_tests.sh` or `make check-shell`. Tests cover every built-in
filter, macro ownership and escaping, exact byte limits, stream failures,
allocation failures, whitespace-bearing argv values, real kernel shebangs,
interpreter statuses, and actual Mune callbacks. CMake/CTest also registers the
focused C tests and CLI regression script.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-shebang-sh.h`](../include/mulib-shebang-sh.h) |
| Implementation switch | `MULIB_IMPL_SHEBANG_SH` |
| Repository-local includes | [mulib-mune](mulib-mune.md) |
| Complete example | [`examples/shebang.c`](../examples/shebang.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/shebang.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
