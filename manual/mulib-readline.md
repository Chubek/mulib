# MuLIB Readline

`include/mulib-readline.h` provides a bounded UTF-8 editor, ordered lexical
highlighting, prefix completion, history, declarative directives, and POSIX
terminal/pipe input. It can be embedded without terminal I/O by feeding
`mulib_termkey_event_t` events and consuming text/spans/completions.

## Build and ownership

Define `MULIB_IMPL_READLINE`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_TERMKEY`, and
`MULIB_IMPL_TERMUTILS` in one C11 TU. The optional parser-loader closure additionally
uses `MULIB_IMPL_PARSER`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_PEG`, `MULIB_IMPL_AST`.
The runnable example explicitly enables that complete closure:

```sh
cc -std=c11 -Iinclude examples/modules/readline.c -lm -o readline
./readline
./readline library/librl/lua.mlsyn
```

`create(limits)` returns an owned editor; `destroy` releases it. Inputs to rule,
completion, directive and history functions are copied. `text` returns borrowed
storage invalidated by mutation. `read`, `render` and expanded directives return
malloc-owned strings; release with `free`. Output pointers for `read` start NULL.
Externally serialize access. Enumeration callbacks borrow their strings/spans
and may not reenter or destroy the editor; reentrant mutation returns `BUSY`.

## Editing API

`set(editor,text,length)` atomically replaces the buffer and places the cursor at
the end. `text(editor,&length,&cursor)` exposes a NUL-terminated line and byte
offsets. Lines must be UTF-8 without C0/C1 controls or DEL. Newlines/tabs belong
to the input layer, not stored editor lines.

`edit(editor,&event)` handles text insertion, Left/Right, Home/End, Backspace,
Delete, Up/Down history, and Tab completion. Movement/deletion is by Unicode
scalar, not grapheme cluster. Ctrl-A/E move to ends; Ctrl-K/U/W delete suffix,
prefix or preceding space-delimited word; Ctrl-C returns `INTERRUPT`; Ctrl-D on
empty returns `EOF`, otherwise forward-delete. Enter returns `LINE`. Bracketed
paste accumulates split UTF-8 scalars and changes ASCII controls/newlines/tabs to
spaces, preventing pasted newlines from submitting commands. C1/invalid UTF-8
paste is rejected. Nonediting events are ignored.

`history_add` copies nonempty lines, ignores consecutive duplicates, and evicts
oldest lines to satisfy entry/byte budgets. Moving back down from history restores
the draft. History is in-memory; applications can persist entries separately.

`render(editor,prompt,columns,&output)` returns an ANSI redraw with horizontal
clipping and cursor positioning. Prompts are printable ASCII, width must be at
least 8, and space is reserved to avoid terminal wrapping. Unicode width is an
approximation for combining marks/CJK/emoji; it is not a full Unicode grapheme or
terminal-width database. Colors are fixed style mappings; hosts can render the
`highlight` spans themselves for different themes/frontends.

`read(editor,input_fd,output_fd,prompt,&line)` edits POSIX TTYs in raw mode, enables
bracketed paste and attempts to restore attributes on every return. Failed
restoration is reported as `IO`. It returns `LINE`, `EOF`, `INTERRUPT`, or a negative
error. Pipes use bounded UTF-8 plain lines, accept LF/CRLF and produce no prompts
or ANSI sequences. A final unterminated line is returned. Other controls are
rejected. Terminal I/O is `UNSUPPORTED` on non-POSIX platforms; the editor,
profiles, highlighting, rendering and completion APIs remain portable C11.

## Highlighting and completion

`rule(editor,kind,style,start,end,escape)` appends an ordered rule. The first
matching rule wins. Put longer comment/string delimiters before shorter prefixes.

| Kind / profile spelling | Match |
| --- | --- |
| `WORD` / `word` | Whole identifier, ASCII alphanumeric/underscore or UTF-8 identifier bytes |
| `PREFIX` / `line` | Start delimiter through end of the editor line |
| `SPAN` / `span` | Start/end delimiters with optional printable ASCII escape character |
| `NUMERIC` / `number` | Digit followed by identifier/dot bytes, with identifier boundary |

Styles are `plain`, `keyword`, `string`, `comment`, `number`, `type`, `function`,
`directive`. Rules are lexical aids, with no regex engine, multiline state or
language grammar. Unterminated spans extend to the line end.
`highlight(editor,text,n,callback,user)` visits contiguous styled byte spans.

`completion(editor,text,description)` adds a copied item; duplicate text is a
no-op. `complete` visits matches for the word before the cursor and reports its
replacement start. Whitespace and `()[]{};,+*/=\"'` delimit completion words;
dots/colons remain part of names such as `browser.go`. `accept(editor,index)`
atomically replaces the prefix with the selected zero-based match; Tab accepts
the first match. Enumeration stops when a callback returns nonzero. Hosts can
build menus/dynamic candidate sets with this API.

## Directives

`directive(editor,name,action,expansion,help)` requires exactly one nonempty
action or expansion. Names contain identifier bytes or hyphens and omit `:`.
`dispatch(editor,":name arguments",&action,&arguments,&expansion)` returns
`DIRECTIVE` for a known name, `NOT_FOUND` otherwise. Actions borrow stored editor
data; arguments borrow the input line. Expanded strings are caller-owned and
replace every `$*` with raw argument text. This is textual substitution, not
automatic quoting or evaluation. The application chooses how to use the result.
Readline does not execute scripts or shell commands from profiles.

## Profile formats

The extensions select versioned UTF-8 JSON schemas. Root `version` is `1`;
`language` is descriptive metadata. Unknown metadata is ignored. Definitions
append transactionally: any invalid definition/quota/allocation failure rolls
back all additions from that profile. Existing configuration is retained.
Duplicate directive names fail; duplicate completion text is ignored.

`.mlsyn`:

```json
{"version":1,"language":"Lua","rules":[
  {"kind":"line","style":"comment","start":"--"},
  {"kind":"span","style":"string","start":"\"","end":"\"","escape":"\\"},
  {"kind":"word","style":"keyword","start":"local"},
  {"kind":"number","style":"number"}
]}
```

`.mlac`:

```json
{"version":1,"items":[
  {"text":"browser.go","description":"Navigate to a URL"},
  {"text":"browser.scrape","description":"Extract named fields"}
]}
```

`.mldir`:

```json
{"version":1,"directives":[
  {"name":"say","expansion":"print($*)","help":"Print a Lua expression"},
  {"name":"docs","action":"documentation:lua","help":"Application documentation handler"}
]}
```

`load_memory(editor,kind,json,length,&error_line)` selects a schema directly;
`load(editor,path,&error_line)` infers it from the exact lowercase extension.
Syntax errors report a one-based JSON line; schema/quota errors report line zero.
Profile data is copied into the editor and may be released after loading.

## Bundled language library

[`library/librl`](../library/librl/README.md) ships **45 files for 15 languages**:
C (`c`), C++ (`cpp`), Python, JavaScript, TypeScript, Lua, Rust, Go, Java,
C# (`csharp`), Ruby, PHP, Bash, SQL, JSON. Each has a `.mlsyn`, `.mlac`, `.mldir`.
Profiles include common keywords, types/builtins, string/comment rules, a starter
expansion and an application documentation action. They aid editing without
claiming complete language parsing. Loading a starter does not run that language.

## Limits and errors

Zero limit fields select 65536 line bytes, 256 history entries/1 MiB history,
4096 total rule/completion/directive entries and 1 MiB of copied configuration
strings. Delimiters are at most 256 bytes. Each profile's source is bounded by
`config_bytes`; JSON parse allocations and array bookkeeping are additional.
Strings count terminators toward configuration/history storage budgets.

`INVALID`, `MEMORY`, `LIMIT`, `IO`, `NOT_FOUND`, `BUSY`, and `UNSUPPORTED` are
negative errors. Mutation functions preserve logical content on allocation or
quota failure; history dropping happens only after necessary allocation succeeds.
Rules/completions/directives remain intact if a transactional load fails.

`tests/test_readline.c` verifies UTF-8 movement/deletion/split paste, spans,
completion boundaries, history drafts, directives, rollback and all bundled
profiles. `tools/check_prowse_io.py` additionally checks actual PTY editing,
completion, history, paste, interrupts and terminal restoration through the REPL.
