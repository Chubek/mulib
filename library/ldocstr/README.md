# ldocstr

The automatically installed Lua library for
[`include/mulib-docstring.h`](../../include/mulib-docstring.h).
It runs in MuLIB's Mune Lua implementation, without a third-party Lua runtime.

- [`ldocstr.lua`](ldocstr.lua): DocstrIL constructors, source parsing, all output
  adapters, token/event reconstruction, and postorder/document/stream filters.
- [`ldstr-style.lua`](ldstr-style.lua): Lua stylesheet builders and filters using
  the same native CSS cascade as CSS/JSON/YAML stylesheets.
- [`examples/keyboard.lua`](examples/keyboard.lua): an inline element, directive
  and function-like macro extension.
- [`examples/document.lua`](examples/document.lua): documentation authored in Lua.
- [`examples/filter.lua`](examples/filter.lua): a Pandoc-like walk filter.
- [`examples/style.lua`](examples/style.lua): a stylesheet implemented as a filter.

No `require` is needed in compiler extension/filter chunks. Lua-authored documents
can use `require('ldocstr')` or `require('ldstr-style')` explicitly. Constructors
are plain Lua; native parsing/rendering requires a Docstrings host. Loading these
files in an unrelated system Lua process does not install native MuLIB bindings.

See the [language/API manual](../../manual/mulib-docstring.md). Changes to the two
default modules require `python3 tools/embed_ldocstr.py`; the generated C include
is checked in so applications do not need the Lua files at runtime.
