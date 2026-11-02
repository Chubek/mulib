# MuLIB MetaPP

**Header:** `include/mulib-metapp.h`  
**Implementation:** C11; declarations usable from C++17 and later.

MetaPP is a token-based C/C++ preprocessor with a persistent macro environment,
configurable includes, source diagnostics, and native plugins linked by
`mulib-linker`. It does not invoke a system preprocessor to implement its API.

## Integration

Define every switch below in exactly one C translation unit, before any includes:

```c
#define MULIB_IMPL_METAPP
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_LINKER
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_ELF
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_DSU
#include "mulib-metapp.h"
```

Other translation units include declarations normally. Implementation switches
are never enabled implicitly. No external libraries or compiler runtime are
required. The core and statically registered plugins are portable; native ELF
plugin linking currently requires Linux x86-64 with the LP64 ABI.

```c
mulib_metapp_options_t options = {0};
mulib_metapp_error_t error = {0};
mulib_strbuf_t output = {0};
mulib_metapp_t *pp = mulib_metapp_create(&options);
if (!pp) return 1;
int rc = mulib_metapp_define(pp, "SCALE(x) ((x) * 2)", &error);
if (!rc) {
    const char source[] = "int answer = SCALE(21);\n";
    rc = mulib_metapp_process(pp, "input.c", source, sizeof(source)-1,
                             &output, &error);
}
/* output.data is NUL-terminated when allocated; output.length excludes NUL. */
mulib_strbuf_free(&output);
mulib_metapp_destroy(pp);
```

`mulib_metapp_file` reads a bounded native file and calls the same preprocessor.
`process` accepts an explicit byte length; NULL source is permitted for length
zero. Source and source-name strings are borrowed only for the duration of a call.
Output is **appended only on success**. Initialize a builder to `{0}` and release
it with `mulib_strbuf_free`. Source/output aliasing is permitted because emission
is staged before the final append.

## Language profile

Set `cplusplus = 0` for the C17-oriented profile or `1` for C++20-oriented lexical
rules. This is a preprocessing profile, not a full compiler-conformance claim.

Supported features include:

- NUL-free UTF-8 input, an optional initial BOM, CR/CRLF normalization,
  backslash-newline splicing, line/block comments, and digraphs. Trigraphs are
  enabled explicitly with `trigraphs = 1` and are disabled by default.
- Preprocessing identifiers, pp-numbers, punctuation, ordinary/prefixed strings
  and character literals. C++ adds raw strings, literal suffixes, alternative
  operator tokens, and the `<::` lexical exception. Raw-string contents retain
  physical backslash-newlines. `\u`/`\U` names in identifiers become UTF-8;
  non-ASCII identifier characters are accepted without Unicode normalization or
  a Unicode identifier-category database.
- Object/function macros, empty arguments, argument prescan, rescanning,
  recursion suppression, `#` stringification, checked `##` concatenation,
  variadic `__VA_ARGS__`, and `__VA_OPT__`, including stringified optional groups.
  Concatenation must form one token; empty paste operands use placemarkers.
  Ordinary macro calls may span lines. An intervening directive terminates the
  pending token block; directives embedded in macro arguments are unsupported.
- `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`,
  `#include`, `#line`, `#error`, `#warning`, `#pragma`, and `_Pragma` with ordinary
  or `L`-prefixed strings. Empty directives are accepted.
- `defined`, integer/character conditional constants, unary and binary C integer
  operators, parentheses, and `?:`, with short-circuit evaluation. Undefined
  identifiers become zero; C++ `true`/`false` are recognized. Signed/unsigned
  comparisons use a fixed 64-bit model. Unsigned arithmetic wraps; signed
  addition/subtraction/multiplication also use two's-complement wrapping in this
  profile. Right shifts of negative signed values are arithmetic. Evaluated
  division by zero, signed division overflow, and shift counts outside 0–63 are
  errors. Skipped operands are still syntax checked. Conditional character
  constants use Unicode scalar values, with multi-character values packed into
  at most eight bytes; this is independent of a compiler's target execution
  character set. Other literal semantics belong to the downstream compiler.
- `__FILE__`, `__LINE__`, `__COUNTER__`, `__DATE__`, `__TIME__`, `__has_include`,
  C `__STDC__ = 1` / `__STDC_VERSION__ = 201710L`, or C++
  `__cplusplus = 202002L`. Predefined names cannot be redefined/undefined.

`__VA_OPT__`, omitted variadic tails, binary integer constants, digit separators,
`#elifdef`, `#elifndef`, and `#warning` are available as extensions in both profiles.
Identical macro redefinitions are accepted; incompatible ones are diagnosed.
Replacement/parameter spellings and intervening whitespace must agree.

`#line` changes diagnostics and subsequent `__FILE__`/`__LINE__`, while physical
include lookup remains relative to the actual source. Line numbers must be
1–2147483647. Its optional ordinary filename supports escaped quotes/backslashes;
other filename escape sequences are explicitly unsupported.

The output preserves preprocessing tokens and literal spellings, rather than
formatting or comments: normal tokens are separated by spaces, pending blocks
end in newlines, and unknown pragmas are emitted as `#pragma` lines. No source-map
or line-marker stream is emitted. `_Pragma` callbacks execute during expansion;
generated text is carried at the expansion position. Arguments are prescanned
once, so callback side effects occur once per prescan, while emitted pragma text
may be duplicated by argument substitution. Pragmas that emit text are intended
for ordinary source, not conditional expressions or include/line operands.

This profile does not implement header units/modules, `#include_next`, `#embed`,
GNU named variadics/comma-swallowing, compiler-specific intrinsic predicates,
target predefined macro sets, or a system include-path discovery mechanism.
Unrecognized active directives fail unless a plugin handles them. Standard
library headers depending on compiler extensions require matching extensions
and target configuration; the library does not promise to preprocess arbitrary
host SDK headers out of the box. Inactive groups skip directive execution, but
their source must still be lexically valid. Literal validation after preprocessing
and C/C++ parsing are the consumer's responsibility.

## Includes and lifetime

`mulib_metapp_include_path` copies up to 256 search directories, in order. Quoted
headers first search the physical source's directory, then the configured paths;
angle headers search only configured paths. Absolute paths are accepted. No
implicit system paths are added. Direct header names are lexed as header names,
so comment-like bytes inside them remain part of the path. Macro-generated
header names are expanded and then interpreted as quoted or angle names.

`mulib_metapp_include_resolver(pp, fn, user)` replaces native lookup for includes
and `__has_include` probes. It does not replace the root file reader. The callback
receives the including physical filename, requested header and angle flag. Return
zero with `data`, `length`, and a nonempty, stable identity `name`; return
`MULIB_METAPP_IO` for absence or another negative result for failure. Positive
results are invalid. Every successful open invokes its optional `release(user)`
exactly once, including probes, skipped-once files, and preprocessing failures.
On a failed open the callback remains responsible for its resources. Returned
data is borrowed until release. A NULL resolver restores native lookup.

Include guards work through ordinary macros. `#pragma once`/`_Pragma("once")`
use identities within one preprocessing call. Native identities normalize slash
paths lexically, including `.` and `..`; they do not resolve symlink/hardlink
aliases. A custom resolver can supply canonical filesystem or VFS identities.
An absent include is an error; `__has_include` turns only IO/absence into zero.

Definitions, search paths, resolver and plugins persist across calls. Macro
definitions copy names, parameters and replacement tokens. Once-state, counters
and scratch storage reset for each `process`/`file` call. Use a fresh context per
translation unit when include-guard and macro state should be independent.
Definitions established before a failure remain available. Default date/time
strings are reproducible `Jan  1 1970` and `00:00:00`; options can supply copied
11-byte/8-byte values instead of consulting a clock.

## Plugins

Register a copied `mulib_metapp_plugin_t` descriptor with `mulib_metapp_plugin`.
The ABI version must equal `MULIB_METAPP_PLUGIN_ABI`, and `size` must equal the
descriptor size. Names are nonempty and unique; at most 64 plugins may be
registered. Successful registration adopts `user`/`destroy`; failure adopts
nothing. Destruction runs callbacks in reverse registration order before unloading
their code. Static callback code and borrowed state must remain valid accordingly.

Each handler receives a versioned host table, the context, an event and an empty
`mulib_strbuf_t` replacement. Events cover:

| Event | Invocation | Handled replacement |
|---|---|---|
| `MULIB_METAPP_DIRECTIVE` | Unrecognized active `#name arguments` | Preprocessed source |
| `MULIB_METAPP_PRAGMA` | `#pragma name arguments` or `_Pragma` | Preprocessed source |
| `MULIB_METAPP_IDENTIFIER` | Identifier without a user/predefined macro | Rescanned tokens |

Return zero to pass, one to handle, or a negative value to fail. The first handler
wins. Passing with nonempty output is a plugin error. Event name/arguments are
NUL-terminated; file/line/column identify the occurrence. All event/output pointers
are callback-scoped. The host `append` function returns nonzero on success;
`define`/`undefine` return zero on success. Mutation is allowed in directive/pragma
callbacks, and returns BUSY in identifier callbacks. Generated identifier tokens
suppress the producing plugin during rescan, allowing self-referential replacement
without infinite recursion. Generated source starts at line 1 of the physical
source identity and consumes another source-nesting level.

For native plugins, export a C-linkage object with this exact name:

```c
const mulib_metapp_plugin_t mulib_metapp_plugin_v1 = {
    MULIB_METAPP_PLUGIN_ABI, sizeof(mulib_metapp_plugin_t),
    "my-extension", handle, NULL, NULL
};
```

`mulib_metapp_plugin_link` accepts borrowed ELF object bytes;
`mulib_metapp_plugin_file` reads an object first. The loader validates ELF64,
little-endian AMD64 ET_REL sections/symbols, lays out allocated sections, defines
symbols in `mulib-linker`, and applies its ABS64, ABS32 and PC32 relocation checks
(AMD64 PLT32 is resolved locally as PC32). Sections become R, RW or RX after
relocation; writable executable sections are rejected. The resulting image is
owned by the context, independently of the original object bytes.

Use PIC, freestanding-style callbacks and the host function table: undefined
imports, GOT/TLS/IFUNC/COMMON, 32S relocations, constructors, and section alignment
above the native page size are rejected. This is a single-object loader, not a
shared-library/ELF dynamic linker. Native callbacks execute with the host process's
ordinary privileges. The example uses local code/data and no imported libc calls.

## Entry-independent dependency graphs

MetaPP can index a project and return the complete **weakly connected include
component** containing any of its files. Selection follows both dependencies and
dependents, while the returned edges retain their direction: **including file →
included file**. A source file, shared header, or leaf header in the same component
therefore returns identical sorted filenames, node IDs, edges, and DOT output.
Unrelated components are excluded. Guarded include cycles and self-includes are
retained rather than forced into a DAG.

```c
mulib_metapp_graph_t *index = NULL, *component = NULL;
mulib_strbuf_t dot = {0};
int rc = mulib_metapp_dependencies_tree(pp, "project", NULL, &index, &error);
if (!rc)
    rc = mulib_metapp_graph_component(index, "project/include/common.h", &component);
if (!rc)
    rc = mulib_metapp_graph_dot(component, &dot);
/* Every file in component would yield the same dot bytes. */
mulib_strbuf_free(&dot);
mulib_metapp_graph_destroy(component);
mulib_metapp_graph_destroy(index);
```

The index is built before selecting an entry file. Discovering reverse includes
requires a complete search boundary: a project tree or a supplied translation-unit
list. Includes reached outside that boundary are indexed, but additional source
files outside the boundary are not discovered automatically.

### Native project trees

`mulib_metapp_dependencies_tree` recursively enumerates a POSIX directory. It
skips hidden entries, symlinks and nonregular files. Source extensions are `.c`,
`.C`, `.cc`, `.cpp`, `.cxx`, and `.c++`; header extensions are `.h`, `.H`, `.hh`,
`.hpp`, `.hxx`, `.inc`, and `.inl`. Other file types are ignored. All source files
are preprocessed in filename order, using the context's configured language
profile. Headers not reached by those sources are then processed in filename
order, skipping headers already reached. This supports header-only components
while letting ordinary headers inherit their translation unit's macros. An
unreached header must be preprocessable with the configured baseline macros.

The native-tree API requires the native include resolver. For generated sources,
custom filesystems, build-specific source lists, or projects containing test files
that deliberately fail preprocessing, use explicit roots:

```c
const char *units[] = { "project/src/a.c", "project/src/b.c" };
mulib_metapp_graph_t *index = NULL;
int rc = mulib_metapp_dependencies(pp, units, 2, NULL, &index, &error);
```

Explicit roots are sorted and deduplicated; headers can be listed as independent
roots when needed. An empty root list produces an empty index. With a custom
resolver, roots are opened through the configured callback with
`including_file == NULL`, `header == root_name`, and `angled == 0`. Include opens
retain their existing callback contract. Successful root and include opens always
receive exactly one release call, including on errors.

### Preprocessing semantics and ownership

Each translation-unit root starts with an independent copy of the original
context's macro definitions, include paths, language options and plugin
registrations. Definitions and include guards from one root cannot leak into
another. The original context's definitions/configuration remain intact. Plugins
and resolver callbacks are borrowed while indexing, including code owned by the
native plugin loader; indexing never adopts or destroys their user state. The
original context is busy during indexing and rejects reentrant mutation.

Only **active, resolved `#include` relationships** become edges. Conditional and
macro-generated includes use normal preprocessing; includes emitted by plugins
are attributed to the physical originating file. `#line` does not rename graph
nodes. Repeated includes produce one edge, including edges to files skipped by
`#pragma once` or an include guard. `__has_include` probes and inactive includes
add no edges. A header active in multiple translation-unit configurations can
have the union of their outgoing edges in the index.

Indexing uses the full preprocessor and its normal limits. Missing active headers,
unsupported directives, unguarded infinite includes, or preprocessing failures in
any indexed root fail the entire build of the index. No partial graph is published.
Files and callbacks should provide consistent contents during indexing. Mutable
plugin/resolver user state is shared and not reset between roots; reproducibility
across separate builds requires reproducible callbacks and inputs. Component
queries on an existing index perform no preprocessing or filesystem access, so
their entry independence is unconditional.

Graphs and extracted components own all their storage, outlive the preprocessor,
and can be destroyed independently. `graph_count` / `graph_file` enumerate
bytewise-sorted file identities; `graph_edge_count` / `graph_edge` enumerate unique
edges in `(from,to)` order. Returned names/edge pointers are borrowed until graph
destruction. `graph_dot` appends Graphviz DOT only on success and escapes filename
quotes/backslashes/newlines. Its output has no selected-entry decoration.

On POSIX, native identities are absolute slash paths normalized lexically;
relative roots and selection filenames are resolved against the working directory
captured when indexing began, not against the project root. Symlink/hardlink
identities are not collapsed. Custom-resolver identities are used exactly as
returned. On other systems the explicit-root API uses lexical slash paths without
POSIX working-directory expansion; native tree discovery returns UNSUPPORTED.

Every graph-producing API requires `*out == NULL` and leaves it NULL on failure.
Selecting an absent file returns `MULIB_METAPP_NOTFOUND`. Dependency options with
zero fields select 4,096 files, 32,768 unique edges, and 100,000 enumerated tree
entries. Root-list slots and discovered source/header candidates also count
against the file limit; tree nesting is limited to 128. Normal preprocessing
source/token/arena/output limits apply independently to each translation unit.
Graph storage is sparse, using MuLIB DSU for connected components; node lookup
and edge insertion are intended for bounded projects rather than unbounded
repository-wide indexing.

## Errors and limits

Results distinguish INVALID, MEMORY, LIMIT, SYNTAX, IO, PLUGIN, UNSUPPORTED,
BUSY and NOTFOUND. Diagnostic records copy the first failure's code, file, line, column and
message; file/message buffers truncate to 255 bytes. Successful processing zeros
the record. Early invalid-argument/BUSY returns leave it unchanged. `#warning`
calls the optional warning callback with code zero, or is ignored if absent.

Zero option limits select these defaults:

| Resource | Default |
|---|---:|
| Each source/plugin object or plugin replacement | 4 MiB |
| Produced output per call | 16 MiB |
| Token insertions/work | 1,000,000 |
| Lexer/expansion/expression steps | 10,000,000 |
| Scratch arena allocation accounting | 128 MiB |
| Persistent user macros | 4,096 |
| Include/generated-source depth | 64 |
| Argument/expression expansion depth | 128 |

Additional fixed bounds are 128 conditional levels, 127 formal parameters
(including the variadic slot), 128 argument slots, 4,096 ELF sections, 65,536 ELF
symbols, and 100,000 relocations. Configured source depth may not exceed 256 and
expansion depth may not exceed 512. Arena accounting includes an alignment charge
per allocation; growable token vectors, retained definitions, output builders and
native images are separate allocations. `arena_bytes` also bounds each native
image. Limits bound work/storage categories, not total process memory or plugin
execution time. Allocation sizes and capacity growth are overflow checked.

Contexts require external synchronization. Process/file/link/registration/config
operations are not reentrant; destruction while busy is ignored. Callbacks must
return normally and must not retain borrowed event data. Independent contexts
have independent state.

## Examples and verification

```sh
cc -std=c11 -Iinclude examples/modules/metapp.c -o /tmp/opencode/metapp
/tmp/opencode/metapp
/tmp/opencode/metapp --cpp -Iinclude '-DVALUE=42' input.cpp
/tmp/opencode/metapp --deps=project project/include/common.h > graph.dot

cc -std=c11 -O2 -fPIC -fno-builtin -fno-stack-protector \
  -fno-asynchronous-unwind-tables -fno-unwind-tables -Iinclude \
  -c examples/plugins/metapp-count.c -o /tmp/opencode/metapp-count.o
/tmp/opencode/metapp --plugin=/tmp/opencode/metapp-count.o input.c

cc -std=c11 -Iinclude tests/test_metapp.c -o /tmp/opencode/test-metapp
/tmp/opencode/test-metapp
python3 tools/check_metapp.py --preprocessor /tmp/opencode/test-metapp --compiler cc
```

The native example adds `#metapp_emit`, `#metapp_define`, `#pragma metapp_note`,
and `__PLUGIN_COUNT__`. Tests cover macro rescanning, optional groups, lexical
rules, conditionals, resolver release, diagnostics, callback mutation, limits,
allocation-failure cleanup, real linked-plugin execution, malformed ELF records,
entry-independent connected dependency graphs, native project discovery,
translation-unit macro isolation, and token-stream comparisons against GCC/Clang. CMake discovers the unit suite
and adds the compiler-reference/native-plugin check when Python and GCC/Clang
are available.
