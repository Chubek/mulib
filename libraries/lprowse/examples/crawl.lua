local p = require('lprowse')
local rows, stats = p.crawl('https://example.com', {
  max_pages=10, max_depth=2, max_queue=100, same_origin=true, delay=100,
  fields={title='h1', description='p'},
  visit=function(row, page) row.url=page.url; print(p.json(row)) end
})
print(p.json(stats))
assert(#rows == stats.visited)
