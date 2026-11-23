# On1x for Sublime Text

In Sublime Text, choose **Preferences → Browse Packages…**. Create an `On1x`
directory there and copy `On1x.sublime-syntax`, `On1xfile.sublime-syntax`,
and `Comments.tmPreferences` into it. Open an `.on1x` or `.on1xi` file or a
plain `On1xfile`, `On1xfile.m`, or `On1xfile.i`. The syntax menu lists
**On1x** and **On1xfile**. Toggle Comment uses `#`.

This is lexical highlighting, with local approximations for contextual
keywords and opaque strings for both forms of backtick command literal. The
same syntax files are used by [bat](../bat/README.md).

## Optional language server

The syntax package does not run anything. If you already have Sublime's
[LSP package](https://lsp.sublimetext.io/), add this entry to your user
**Preferences: LSP Server Configurations**:

```json
{
  "on1x": {
    "enabled": false,
    "selector": "source.on1x",
    "command": [
      "/absolute/path/to/python3",
      "/absolute/path/to/mulib/on1x/addons/lsp/on1x-lsp.py",
      "--on1xc", "/absolute/path/to/on1xc"
    ]
  }
}
```

Use trusted POSIX executable and checkout paths. Leave the global setting
disabled. Opt in only for a trusted project you control using
**LSP: Enable Language Server in Project**; don't copy LSP launch settings
from projects you have not reviewed.

## Verify

To verify scopes in Sublime Text, also copy `syntax_test_on1x.on1x` into
`Packages/On1x`, open it, and run **Tools → Build** (Automatic build system). The
`../bat/test_bat.py` smoke test checks the shared syntax with bat's syntect engine;
it is not a substitute for running Sublime's own scope assertions.
