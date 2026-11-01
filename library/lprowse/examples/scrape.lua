local p = require('lprowse')
-- The example C host has already connected and started a browser session.
p.go('https://example.com').wait('h1')
local row = p.scrape {
  title = 'h1',
  description = 'p',
  destinations = {selector='a[href]', attribute='href', all=true}
}
print(p.json(row))
