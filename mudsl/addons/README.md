# Mu editor addons

Editor support for **MuLL** (`*.mull`), the **MuDSL C++20/D toolkit**, and
**MuLIB C headers**. Commands below run from the MuDSL directory.

| Integration | Included |
| --- | --- |
| VS Code / VSCodium | MuLL highlighting, brackets, comments, snippets, bundled LSP client, manual command |
| Vim / Neovim | MuLL filetype, highlighting, comments, indentation, basic omni-completion, manual command, UltiSnips snippets; Neovim LSP setup |
| Sublime Text | TextMate syntax, comment settings, snippets, configuration example for the LSP package |
| JetBrains IDEs | Importable TextMate bundle and live templates; configuration instructions for LSP4IJ |
| Other LSP editors | Standalone Python stdio server |

MuDSL and MuLIB retain their ordinary C, C++, and D filetypes. Snippets include
feature composition, parser combinators, embedded MuLL hosts, implementation
switches, and vector lifetime. MuLL snippets cover declarations, functions,
loops, imports, packages, and MuLIB wrappers.

## Shared language server

Requires **Python 3.9+**, with no Python packages to install.

```sh
sh addons/lsp/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
mull-lsp --root /absolute/path/to/mulib
```

The last command is a stdio server: connect it through an editor's LSP client.
For source-tree use, the command is `python3 /path/to/mudsl/addons/lsp/server.py`.
`--root` accepts a MuLIB checkout or standalone MuDSL directory. Installed
servers carry the native API inventory; local manual/header navigation needs
the checkout. Use `--package-path /absolute/package/root` repeatedly for custom
packages, in runtime discovery order. The MuDSL standard-library root follows
these roots. Paths with spaces are supported.

### Parser-backed diagnostics

Build the optional checker with CMake 3.24+ and a C++20 compiler:

```sh
cmake -S addons/lsp -B addons/lsp/build -DCMAKE_BUILD_TYPE=Release
cmake --build addons/lsp/build --parallel 2
ctest --test-dir addons/lsp/build --output-on-failure
sh addons/lsp/install.sh --checker addons/lsp/build/mull-check
```

Alternatively pass `--checker /absolute/path/to/mull-check` to the server, or
put `mull-check` on `PATH`. The helper uses MuLL's actual lexer/parser from
`MuDSL.hpp`; it **only parses**, including when source contains imports or native
calls. Without the helper the server reports lexical errors and unbalanced
delimiters. It does not report runtime failures (undefined names, mutation rules,
types, numeric evaluation, native ownership, or package execution errors).

### Language features

- MuLL keyword, local-binding, `G:` and imported standard-library completions.
  Direct `val Name <- G:LoadLib("package")` and
  `mulib Name <- G:MulibWrap("library")` bindings are recognized.
- MuLIB wrapper completions use exact C function names from
  `mull/bridge/Mulib/Inventory.json`, plus bridge helpers. Short aliases, native
  constants/types, arbitrary records, and indirect import expressions are not
  inferred.
- Hover documentation, current-document declarations/parameters in the outline,
  local definitions with block shadowing, and navigation from a `LoadLib` string
  to `Package.mull`. Symbol analysis is tolerant of incomplete source and uses
  lexical scopes; it does not perform runtime/data-flow analysis.
- Supplemental C/C++/D completion and hover for MuDSL's core toolkit symbols,
  MuLIB function names and includes; native function definitions open the header.
  Use clangd or your normal C/C++/D tooling for full native-language semantics.
- Full/incremental synchronization, UTF-16 positions, versioned diagnostics,
  4 MiB documents, and shutdown/exit. Source is never evaluated by the server.

Initialization options for clients that support them:

```json
{
  "repositoryRoot": "/absolute/path/to/mulib",
  "packagePaths": ["/absolute/path/to/my/packages"]
}
```

## VS Code / VSCodium

Requires Node.js 20+ and npm to package, Python 3.9+ to run the server.

```sh
sh addons/vscode/install.sh --package-only
code --install-extension addons/vscode/mu-language-tools.vsix
# Or build and install together:
sh addons/vscode/install.sh --code codium
```

Set `mu.repositoryRoot` to your checkout and optionally `mu.pythonPath`,
`mu.checkerPath`, and `mu.packagePaths`. `mu.nativeApi` controls the supplemental
C/C++/D client. **Mu: Open Documentation** opens local manuals;
**Mu: Restart Language Server** restarts the client. Configuration changes also
restart it. Highlighting and snippets work without Python; LSP startup errors
appear in the Mu Language Tools output channel.

For development, run `npm ci` and `npm run prepare-assets` in `addons/vscode`,
then launch VS Code with `--extensionDevelopmentPath=/absolute/path/to/addons/vscode`.
The VSIX bundles the server and inventory; it does not require this source tree
for language completion. The checker remains a separately configured executable.

## Vim / Neovim

```sh
sh addons/vim/install.sh                  # ~/.vim/pack/mu/start/mu
sh addons/vim/install.sh --neovim         # Neovim's XDG data directory
# Custom package location:
sh addons/vim/install.sh --dest /path/to/pack/mu/start/mu
```

Enable `filetype plugin indent on` and `syntax enable` in Vim. Use `Ctrl-X Ctrl-O`
for basic MuLL completion. Snippets use the optional **UltiSnips** plugin.
Set `g:mu_root` to your checkout and run `:MuDocs mull`, `:MuDocs mudsl`, or
`:MuDocs mulib`.

With Neovim 0.8+ and the shared server installed, add to `init.lua`:

```lua
require('mu').setup({
  cmd = { 'mull-lsp' },
  root = '/absolute/path/to/mulib',
  native_api = true, -- also attach to C/C++/D; default is MuLL only
  on_attach = function(_, buffer)
    local opts = { buffer = buffer }
    vim.keymap.set('n', 'gd', vim.lsp.buf.definition, opts)
    vim.keymap.set('n', 'K', vim.lsp.buf.hover, opts)
    vim.bo[buffer].omnifunc = 'v:lua.vim.lsp.omnifunc'
  end,
})
```

Existing completion-plugin capabilities can be passed as `capabilities`.
Classic Vim users can register `mull-lsp` with their preferred LSP client;
use filetype `mull` (and optionally `c`, `cpp`, `d`).

## Sublime Text

```sh
sh addons/sublime/install.sh
# Or choose a Packages directory explicitly:
sh addons/sublime/install.sh --dest /path/to/Packages/Mu
```

The installer selects Sublime Text's platform-specific user package directory.
For Sublime Text 3 or portable installations, pass `--dest`.
Install **LSP** through Package Control, install the shared server, then merge
`sublime/LSP.example.sublime-settings` into **Preferences → Package Settings →
LSP → Settings**, replacing the repository path. An absolute server path is useful
when GUI applications do not inherit your shell's `PATH`.

## JetBrains (CLion, IntelliJ IDEA, etc.)

Close the IDE and select its actual configuration directory:

```sh
sh addons/jetbrains/install.sh --dest "$HOME/.config/JetBrains/CLion2026.2"
```

Enable the **TextMate Bundles** plugin. Under **Settings → Editor → TextMate
Bundles**, add `<IDE-config>/textmate/MuLL.tmbundle`. The installer also copies
`templates/Mu.xml`; restart and find the **Mu** group under **Editor → Live
Templates**. Abbreviations are language-prefixed, such as `mull-loadlib`,
`cpp-mudsl`, `d-mullembed`, and `c-cpp-mulibimpl`. Adjust template applicability
there if desired. The bundle and templates can also be imported/copied manually
from `addons/jetbrains`.

For language intelligence, install **LSP4IJ** and create a user-defined stdio
server with command `/absolute/path/to/mull-lsp --root /absolute/path/to/mulib`.
Map `*.mull` to language ID `mull`; optional native mappings use `c`, `cpp`, and
`d`. This integration uses TextMate/live-template assets and an external LSP
client, rather than a compiled IntelliJ plugin.

## Maintenance and verification

Edit `shared/mull.tmLanguage.json` and `shared/snippets.json`, then regenerate:

```sh
python3 addons/generate.py
python3 addons/generate.py --check
python3 addons/tests/test_addons.py
MULL_CHECKER="$PWD/addons/lsp/build/mull-check" python3 addons/tests/test_addons.py
```

The suite exercises real stdio protocol traffic, UTF-16 incremental edits,
shadowing, package/header navigation, completion edits, install paths, generated
assets, and the checker when configured. It also smoke-tests Vim when available.
The server's hand-maintained host documentation is in `lsp/api.py`; native names
come from the checked-in bridge inventory and are copied at packaging time.
Tests and packaging do not install anything into your live editor configuration.

Install scripts are POSIX shell wrappers around `install.py`; on Windows, invoke
`python addons/install.py <editor> ...` directly (VSIX packaging additionally
needs npm on PATH). Remove the installed package directory or uninstall the VSIX
to uninstall; LSP files are `<prefix>/bin/mull-lsp` and `<prefix>/share/mu-lsp`.
