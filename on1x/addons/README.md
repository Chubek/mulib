# On1x addons

The editor addons recognize `*.on1x` source files and `*.on1xi` interface files.
These integrations don't change the compiler or application runtime. Package manifests
(`On1xfile`) use different syntax and aren't covered.

| Integration | Setup |
| --- | --- |
| Vim / Neovim | [Runtime package and LSP setup](neovim/doc/on1x.txt) |
| VS Code / VSCodium | [Extension](vscode/README.md) |
| Sublime Text | [Syntax package](sublime/README.md) |
| `bat` | [Syntax cache installation](bat/README.md) |
| Other LSP clients | [Standalone stdio server](lsp/README.md) |
| ChatGPT web | [Read-only remote MCP knowledge service](chatgpt/README.md) |

The ChatGPT addon retrieves cited canonical documentation/kernel evidence. It
does not compile, execute, or admit bindings; ChatGPT web requires a separately
hosted HTTPS endpoint or a configured Secure MCP Tunnel, not local files.

Syntax highlighting doesn't require `on1xc`, Python, or the language server.
Highlighting is lexical; it doesn't tell you whether a program compiles.
For compiler-backed editing of `*.on1x` files, opt in to the server with a
trusted `on1xc` executable. See the server guide for the supported features
and limits.

To try highlighting after installation, open
[`cancellation_deadlines.on1x`](../examples/cancellation_deadlines.on1x)
or run `bat on1x/examples/cancellation_deadlines.on1x` from the repository root.

With an On1x build configured for tests, run:

```sh
ctest --test-dir build/on1x --output-on-failure -R '^on1x\.addon_'
```

The editor smoke tests use tools already installed on the host. The Neovim
LSP smoke requires Neovim 0.10+ and a compiler built with Syren.
