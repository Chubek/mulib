# On1x for VS Code

Highlights `.on1x` source and `.on1xi` interfaces, with `#` line-comment
toggling and ordinary bracket/quote pairing. The separate On1xfile language
recognizes `On1xfile`, `On1xfile.m`, and `On1xfile.i` by filename.
See the [CLI and project policy reference](../CLI.md).

Source highlighting follows the compiler
lexer and uses local cues for contextual keywords; it does not validate code
or parse PPX payloads or backtick command contents as another language.

## Install

- **Try from source:** Run `code --extensionDevelopmentPath=/absolute/path/to/mulib/on1x/addons/vscode`
  using the VS Code CLI, then open an `.on1x` file in the new window.
- **Local VSIX:** With `vsce` already installed, run `vsce package` in this
  directory and install the resulting VSIX with **Extensions: Install from VSIX…**
  or `code --install-extension on1x-0.1.0.vsix`. No `npm install` is needed
  in this directory.

The extension is declarative and works in Restricted Mode. Opening a file
does not run a compiler, formatter, Python, or language server.

## Optional language server

VS Code does not wire arbitrary stdio servers through its built-in language
configuration. This extension does not ship an LSP client. If you already use
a separate LSP client extension that supports custom stdio servers and respects
Workspace Trust, opt in **in user settings, for trusted workspaces only**:
select the language ID `on1x`, use the command `python3`, and set its argument
list to `["/absolute/path/to/mulib/on1x/addons/lsp/on1x-lsp.py", "--on1xc", "/absolute/path/to/on1xc"]`.
The adapter currently requires POSIX. Use your own trusted checkout and
compiler paths, not workspace-supplied executables or settings. Highlighting
needs none of this.

## Verify

Run `node on1x/addons/vscode/test-syntax.cjs` from the repository root for
manifest/configuration and regex checks. To verify editor tokenization, open a
source file and run **Developer: Inspect Editor Tokens and Scopes**.
