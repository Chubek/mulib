-- lprowse: MuLIB browser automation DSL. The host supplies __mulib_lprowse.
local native = assert(__mulib_lprowse, 'lprowse requires a MuLIB Prowse host')
local M = {version = '1.0'}
M.null = setmetatable({}, {__tostring = function() return 'null' end})
local array_mt = {}
function M.array(t) return setmetatable(t or {}, array_mt) end
local function quote(s)
  return '"' .. s:gsub('[%z\1-\31\\"]', function(c)
    if c == '"' then return '\\"' end
    if c == '\\' then return '\\\\' end
    return string.format('\\u%04x', string.byte(c))
  end) .. '"'
end
function M.json(value)
  local seen = {}
  local function encode(v, depth)
    assert(depth <= 32, 'JSON nesting limit')
    if v == M.null or v == nil then return 'null' end
    local kind = type(v)
    if kind == 'string' then return quote(v) end
    if kind == 'boolean' then return tostring(v) end
    if kind == 'number' then
      assert(v == v and v ~= math.huge and v ~= -math.huge, 'non-finite JSON number')
      return string.format('%.17g', v)
    end
    assert(kind == 'table' and not seen[v], 'JSON requires acyclic data tables')
    seen[v] = true
    local count, maximum, numeric = 0, 0, true
    for k in pairs(v) do
      count = count + 1
      if type(k) ~= 'number' or k < 1 or k % 1 ~= 0 then numeric = false
      elseif k > maximum then maximum = k end
    end
    local array = getmetatable(v) == array_mt or (count > 0 and numeric)
    local out = {}
    if array then
      assert(numeric and count == maximum, 'JSON arrays must be dense')
      for i = 1, maximum do out[i] = encode(v[i], depth + 1) end
    else
      local keys = {}
      for k in pairs(v) do assert(type(k) == 'string', 'JSON object keys must be strings'); keys[#keys + 1] = k end
      table.sort(keys)
      for i, k in ipairs(keys) do out[i] = quote(k) .. ':' .. encode(v[k], depth + 1) end
    end
    seen[v] = nil
    return (array and '[' or '{') .. table.concat(out, ',') .. (array and ']' or '}')
  end
  return encode(value, 0)
end
local function call(op, args)
  local value, message, code = native(op, M.json(args or {}))
  if message ~= nil then error(op .. ': ' .. message .. ' (' .. tostring(code) .. ')', 3) end
  return value
end
function M.connect(protocol, endpoint) call('connect', {protocol=protocol, endpoint=endpoint}); return M end
function M.start(capabilities) call('start', {capabilities=capabilities or {}}); return M end
function M.attach(session) call('attach', {session=session}); return M end
function M.quit() call('quit'); return M end
function M.disconnect() call('disconnect'); return M end
function M.status() return call('status') end
function M.go(url) call('navigate', {url=url}); return M end
M.navigate = M.go
function M.eval(expression) return call('evaluate', {expression=expression}) end
function M.click(selector) call('click', {selector=selector}); return M end
function M.type(selector, text) call('type', {selector=selector, text=text}); return M end
function M.fill(selector, text)
  M.eval('(function(){const e=document.querySelector(' .. quote(selector) .. ');if(!e)throw new Error("element not found");e.focus();e.value=' .. quote(text) .. ';e.dispatchEvent(new Event("input",{bubbles:true}));e.dispatchEvent(new Event("change",{bubbles:true}));return true})()')
  return M
end
function M.text(selector, all) return call('query', {selector=selector, property='text', all=not not all}) end
function M.html(selector, all) return call('query', {selector=selector or 'html', property='html', all=not not all}) end
function M.attr(selector, attribute, all) return call('query', {selector=selector, property='attribute', attribute=attribute, all=not not all}) end
function M.links(selector) return call('query', {selector=selector or 'a[href]', property='href', all=true}) end
function M.title() return M.eval('document.title') end
function M.url() return M.eval('location.href') end
function M.back() M.eval('(history.back(),true)'); return M end
function M.forward() M.eval('(history.forward(),true)'); return M end
function M.reload() M.eval('(location.reload(),true)'); return M end
function M.screenshot() return call('screenshot') end -- base64 PNG
function M.sleep(milliseconds) call('sleep', {milliseconds=milliseconds}); return M end
function M.wait(selector, milliseconds) call('wait', {selector=selector, milliseconds=milliseconds or 5000}); return M end
function M.command(method, params) return call('command', {method=method, params=params or {}}) end
function M.http(method, path, body) return call('http', {method=method, path=path, body=body}) end
function M.event() return call('event') end
function M.resolve(url, base) return call('resolve', {url=url, base=base}) end
function M.scrape(fields)
  local row = {}
  for name, field in pairs(fields) do
    if type(field) == 'function' then row[name] = field(M)
    elseif type(field) == 'string' then row[name] = M.text(field)
    elseif field.attribute then row[name] = M.attr(field.selector, field.attribute, field.all)
    elseif field.html then row[name] = M.html(field.selector, field.all)
    else row[name] = M.text(field.selector, field.all) end
  end
  return row
end
function M.retry(attempts, delay, fn)
  assert(math.type(attempts) == 'integer' and attempts > 0 and attempts <= 1000, 'invalid retry count')
  local last
  for i = 1, attempts do
    local ok, value = pcall(fn, M, i)
    if ok then return value end
    last = value
    if i < attempts then M.sleep(delay or 100) end
  end
  error(last, 2)
end
function M.crawl(seed, options)
  local o = options or {}
  local maximum, depth_limit, queue_limit = o.max_pages or 100, o.max_depth or 3, o.max_queue or 1000
  assert(math.type(maximum)=='integer' and maximum>0 and maximum<=100000, 'invalid page limit')
  assert(math.type(depth_limit)=='integer' and depth_limit>=0 and depth_limit<=1000, 'invalid depth limit')
  assert(math.type(queue_limit)=='integer' and queue_limit>0 and queue_limit<=100000, 'invalid queue limit')
  seed = M.resolve(seed, seed):gsub('#.*$', '')
  local origin = seed:match('^(https?://[^/]+)')
  assert(origin, 'crawl seed must be an absolute HTTP(S) URL')
  local queue, seen, rows, head, visited = {{url=seed, depth=0}}, {[seed]=true}, M.array(), 1, 0
  while head <= #queue and visited < maximum do
    local page = queue[head]; head = head + 1; visited = visited + 1
    local ok, result = pcall(function()
      M.go(page.url)
      if o.wait then M.wait(o.wait, o.timeout or 5000) end
      local row = o.fields and M.scrape(o.fields) or {url=M.url(), title=M.title()}
      if o.visit then o.visit(row, page, M) end
      if page.depth < depth_limit then
        for _, url in ipairs(M.links(o.links)) do
          if type(url)=='string' then
            url = url:gsub('#.*$', '')
            local next_origin = url:match('^(https?://[^/]+)')
            if next_origin and (o.same_origin == false or next_origin == origin) and not seen[url]
                and #queue < queue_limit and (not o.allow or o.allow(url, page.depth + 1)) then
              seen[url] = true; queue[#queue+1] = {url=url, depth=page.depth+1}
            end
          end
        end
      end
      return row
    end)
    if not ok then
      if not o.continue_on_error then error(result, 2) end
      result = {url=page.url, error=tostring(result)}
    end
    if o.collect ~= false then rows[#rows+1] = result end
    if o.delay and head <= #queue then M.sleep(o.delay) end
  end
  return rows, {visited=visited, queued=#queue, remaining=#queue-head+1}
end
return M
