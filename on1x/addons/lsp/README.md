# On1x stdio language server

Configure your editor to launch:

```sh
python3 /path/to/on1x/addons/lsp/on1x-lsp.py --on1xc /path/to/on1xc
```

The `--on1xc` argument is optional; the default is `on1xc` on `PATH`. Use a
trusted build or installation of the current On1x compiler. An explicit
path is recommended, especially for a development build. A bare name is
looked up on absolute `PATH` entries only, ignoring the server's working
directory and candidates under recognized workspace roots or source
directories (including symlink targets). Do not put untrusted locations
on the launcher's `PATH` or point `--on1xc` at an untrusted executable.
If using generated `import foreign stdffi.*` recipes outside the compiler's
default location, point `--foreignlib-path` at the directory containing the
`.ffi` recipes (for example `/absolute/build/stdffi/mulib/recipes`).
Repeat it for multiple trusted recipe directories. The adapter does not
inherit `MULIB_FOREIGNLIB_PATH` or run a recipe generator.
The server uses only the Python 3.8+ standard library and POSIX
process/resource APIs. It has been tested on Linux, not on Windows.

## Features and limits

- **Diagnostics on save only.** The adapter runs
  `on1xc --deps --diagnostics json --color never FILE`, not `--check`.
  This reports real scanner, parser and module-loading errors that the
   compiler encounters without expanding PPX, including missing or ambiguous
   operator bindings. Operator diagnostics include the compiler's remediation
   when available; they do not invent a repair. The server does not type check,
  expand macros, elaborate native extensions or diagnose their generated
   declarations. It resolves portable `import foreign stdffi.*` imports
   and reports loader findings such as missing recipes or exports. Native
   `import kernel` declarations (including `unsafe` and `[dynlib]`) are
   separate: when the loader stops because this mode cannot admit KernelBind
   imports, the adapter reports that limitation as information rather than
   treating the request as invalid syntax. An empty diagnostics list does
   not mean a program builds.
- Diagnostics require **`ON1X_WITH_SYREN=ON`** in `on1xc`. A compiler
  without JSON diagnostics can still format; save inspection logs the
  compiler's failure rather than parsing text into made-up diagnostics.
  The server maps the compiler's byte offsets to point diagnostics;
  there is no source span in the compiler report. Only locations in the
  saved entry are published. Diagnostics about dependencies and other
  files are logged (up to three reports), not placed on unrelated or
  possibly unsaved buffers.
- Save a local `.on1x` document with UTF-8 text. The saved bytes must
  match the open buffer except for CRLF/CR/LF normalization. A missing
  final newline, different text, or a detectable change while the
  compiler runs causes the inspection to be discarded. Edits and closing
  the buffer clear previously published findings. There is no check on
  opening, typing, or saving only a dependency of the buffer.
- **Whole-document formatting.** The adapter runs `on1xc fmt --isolated
  --target plain` on a private copy of the current buffer, even if that content
  hasn't been saved yet. It returns one replacement edit or no edits
  if unchanged and never writes your source file. The formatter rejects
  invalid syntax. It uses compiler defaults (not workspace `.on1xfmt`
  or the client's tab settings) and can normalize line endings. Source
   preservation directives such as `# on1xfmt: off` still apply.
   This formats locally declared operators. Unlike save diagnostics, it does
   not load imported or pervasive operator declarations.
- **Plain On1xfile support.** `On1xfile`, `On1xfile.m`, and `On1xfile.i`
  use the current `on1xc aux format` driver for formatting and save-time
  diagnostics. The adapter passes a private copy with its original basename
  and maps any reported line span back to the editor. This checks the plain
  directives and policy syntax, including backend and optimization profiles,
  not the project graph, optimizer inventory, actual targets or build results.
  It never runs actions or renders QMR templates. Save inspection skips
  QMR manifests and formatting rejects them.
- **Revision-bound quick fixes.** Bounded compiler-reported repair edits with
  status `validated` are exposed after save as quick fixes tied to the open
  document version. Repairs reported only as `candidate` are visible as
  disabled suggestions with an explicit unverified reason. The adapter never
  upgrades a candidate, applies an edit itself, or keeps actions after the
   document changes.
- **Operator hover and go-to-definition.** Hover shows the visible operator's
  checked signature, when available, and declared fixity. Go-to-definition
  follows that particular declaration, including through selective imports.
  Both require a saved source file matching the open buffer; they return no
  result if the source does not check or requires PPX or extension execution.
  No editor-side operator index is maintained.
- The server advertises full-text synchronization and UTF-16 positions
  by default; it can negotiate UTF-8 or UTF-32. It accepts local
  `file:` URIs (including `localhost`), not remote or untitled buffers.
   No general-purpose hover or go-to-definition, completion, symbols,
   references, rename, range formatting or semantic index is implemented.

## What runs and what is read

For the current compiler, `--deps` disables native declaration
extension execution (including source `@enable Protocols`) across the
loaded graph and returns before ordinary PPX rewriting. The adapter never
uses `--allow-source-ppx`, selects PPX processors, invokes unrestricted `--check`,
runs a project's build scripts, or forwards compiler arguments from the
workspace. `fmt --isolated` parses a single copied buffer without loading imports or
running PPX. `aux format` invokes the trusted project's plain-manifest
formatter bundled with the compiler, not a project command or template.
Operator navigation calls `on1xc --operator-info --source-only`, which
checks the saved source graph without running PPX, native declaration
extensions, or project actions. These commands do not use a shell.
The subprocess gets a private working directory and a
minimal environment, without inherited `ON1X_*` or `MULIB_FOREIGNLIB_PATH`
search paths. Update the adapter and compiler together if these CLI
guarantees change.

Save inspection is **not a filesystem sandbox**. It resolves the
entry URI to its real path. The standard compiler may read its prelude,
follow symlinks, load absolute or relative source imports (also outside
the workspace), and read literal `On1xfile` package identity and paths
as part of module discovery. This is passive discovery; it does not
run actions or apply project build profiles from those manifests.
The adapter does not snapshot dependencies or prevent concurrent
filesystem changes. Do not enable it on an untrusted workspace if those
reads are unacceptable; use an OS-level sandbox if filesystem
confinement is required.

The adapter caps frames at 8 KiB of headers and 16 MiB of body, with up
to 64 open documents, 2 MiB per buffer and 16 MiB of retained text.
Requests are processed serially; one compiler call is allowed at a time
with a five-second timeout, capped CPU time, 1 GiB address space and 4 MiB
per stdout/stderr stream. The address-space limit can prevent ASan
compiler builds from starting. Budget failures are logged for saving or
returned as formatting errors.

## Tests

Run from the repository root:

```sh
python3 on1x/addons/lsp/test_lsp.py -v
python3 on1x/addons/lsp/test_lsp.py --on1xc build/on1x/on1xc -v
python3 on1x/addons/lsp/test_lsp.py --on1xc build/on1x/on1xc \
    --foreignlib-path build/on1x/stdffi/mulib/recipes -v
```

The first command runs protocol and subprocess-boundary tests and skips
the real compiler tests. Add `--no-syren` with a compiler built without
Syren to test real formatting and diagnostics-unavailable behavior; the
JSON/PPX import smoke then skips. The real syntax smoke checks an LF
buffer over a CRLF file, an astral character before the diagnostic, and
formatting against a real compiler rather than the protocol fixture.
