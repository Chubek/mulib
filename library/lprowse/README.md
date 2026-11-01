# lprowse

An extensible Lua automation DSL hosted by MuLIB Mune and Prowse control.

```lua
local p = require('lprowse')
p.connect('webdriver', 'http://127.0.0.1:4444').start({browserName='firefox'})
p.go('https://example.com').wait('h1')
print(p.json(p.scrape{heading='h1', links={selector='a', attribute='href', all=true}}))
p.quit()
```

Use dot calls. Browser selection comes from CDP, W3C WebDriver or Firefox
Marionette; WebKit/Safari use their W3C drivers. The native host owns connections
and exposes browser operations; ordinary Lua functions/tables implement custom
scrapers, workflows and bounded breadth-first crawlers.

* [Full API, build instructions and REPL](../../manual/mulib-prowse-control.md)
* [Protocol endpoints and transport adapters](../../manual/mulib-prowse-protocol.md)
* [`examples/scrape.lua`](examples/scrape.lua)
* [`examples/crawl.lua`](examples/crawl.lua)
* [`examples/local-smoke.lua`](examples/local-smoke.lua), a local data-URL workflow

The host embeds `lprowse.lua` using `include/include-mulib-lprowse.h`. Regenerate
with `python3 tools/embed_lprowse.py` and verify with `--check`. No Lua installation
is required by the default C/Mune application.
