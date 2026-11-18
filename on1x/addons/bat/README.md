# On1x for bat

From the MuLIB repository root, copy the shared Sublime syntax into bat's
custom syntax directory and rebuild the cache:

```sh
syntax_dir="$(bat --config-dir)/syntaxes"
mkdir -p "$syntax_dir"
cp on1x/addons/sublime/On1x.sublime-syntax "$syntax_dir/"
bat cache --build
bat --list-languages | grep On1x
bat on1x/examples/hello.on1x
```

Use `batcat` instead of `bat` if that is your distribution's executable name.
Both `.on1x` and `.on1xi` are detected automatically; use `bat -l On1x` for
stdin or other filenames. Remove the copied file and run `bat cache --build`
again to uninstall. No editor, compiler, or language server is needed.

Run `python3 on1x/addons/bat/test_bat.py` to check cache compilation and
highlighting without touching your configuration.
