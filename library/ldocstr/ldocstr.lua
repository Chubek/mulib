-- DocstrIL constructors and filters. Native operations are supplied by MuLIB.
local M = { elements = {}, directives = {}, macros = {}, null = {} }
local function children(value)
  if value == nil then return {} end
  if type(value) == 'string' then return { M.Text(value) } end
  if value.t then return { value } end
  return value
end
function M.Text(text) return { t = 'Text', text = tostring(text) } end
M.Str = M.Text
function M.Element(name, content, attrs)
  return { t = name, attrs = attrs or {}, children = children(content) }
end
local constructors = {
  Para = 'paragraph', Strong = 'strong', Emph = 'emphasis',
  Underline = 'underline', Strikeout = 'strike', Code = 'code',
  BlockQuote = 'blockquote', Span = 'span', Div = 'div',
  Superscript = 'superscript', Subscript = 'subscript',
  Item = 'item', Table = 'table', Row = 'row', Cell = 'cell',
  TOC = 'toc', Index = 'index', LineBreak = 'break', HorizontalRule = 'rule'
}
M.aliases = { Text = 'Str', docstril = 'Doc' }
for public, name in pairs(constructors) do
  M.aliases[name] = public
  M[public] = function(content, attrs) return M.Element(name, content, attrs) end
end
function M.Doc(content) return M.Element('docstril', content, { version = '1' }) end
function M.Header(level, content, attrs)
  attrs = attrs or {}; attrs.level = tostring(level)
  return M.Element('heading', content, attrs)
end
M.aliases.heading = 'Header'
function M.Link(content, href, attrs)
  attrs = attrs or {}; attrs.href = href
  return M.Element('link', content, attrs)
end
M.aliases.link = 'Link'
function M.Image(alt, src, attrs)
  attrs = attrs or {}; attrs.src = src; attrs.alt = alt
  return M.Element('image', alt, attrs)
end
M.aliases.image = 'Image'
function M.CodeBlock(text, language, attrs)
  attrs = attrs or {}; attrs.language = language or ''
  if language and language ~= '' then attrs.class = 'language-' .. language end
  return M.Element('code-block', { M.Text(text) }, attrs)
end
M.aliases['code-block'] = 'CodeBlock'
function M.BulletList(items) return M.Element('list', items, { ordered = 'false' }) end
function M.OrderedList(items) return M.Element('list', items, { ordered = 'true' }) end
function M.element(name, fn) assert(type(fn) == 'function'); M.elements[name] = fn end
function M.directive(name, fn) assert(type(fn) == 'function'); M.directives[name] = fn end
function M.macro(name, fn) assert(type(fn) == 'function'); M.macros[name] = fn end
function M.parse(source, format) return _ldocstr_parse(source, format or 'docstrings') end
function M.xml(doc) return _ldocstr_output(doc, 'xml') end
function M.html(doc) return _ldocstr_output(doc, 'html') end
function M.render(doc, format) return _ldocstr_output(doc, format or 'xml') end
function M.pandoc(doc) return _ldocstr_output(doc, 'pandoc') end
function M.tokens(doc) return _ldocstr_output(doc, 'tokens') end
function M.from_tokens(tokens) return _ldocstr_from_tokens(tokens) end
function M.copy(value)
  if type(value) ~= 'table' then return value end
  local result = {}
  for key, item in pairs(value) do result[key] = M.copy(item) end
  return result
end
M.dom = M.copy
function M.events(doc)
  local events, current, depth = {}, nil, 0
  for _, token in ipairs(M.tokens(doc)) do
    if token.kind == 'open' then
      current = { kind = 'enter', name = token.name, attrs = {}, depth = depth }
      events[#events + 1] = current; depth = depth + 1
    elseif token.kind == 'attribute' then current.attrs[token.name] = token.value
    elseif token.kind == 'text' then
      events[#events + 1] = { kind = 'text', text = token.value, depth = depth }
      current = nil
    elseif token.kind == 'close' then
      depth = depth - 1
      events[#events + 1] = { kind = 'leave', name = token.name, depth = depth }
      current = nil
    end
  end
  return events
end
function M.from_events(events)
  local tokens = {}
  for _, event in ipairs(events) do
    if event.kind == 'enter' then
      tokens[#tokens + 1] = { kind = 'open', name = event.name }
      local keys = {}; for key in pairs(event.attrs or {}) do keys[#keys + 1] = key end
      table.sort(keys)
      for _, key in ipairs(keys) do
        tokens[#tokens + 1] = { kind = 'attribute', name = key, value = event.attrs[key] }
      end
    elseif event.kind == 'text' then tokens[#tokens + 1] = { kind = 'text', value = event.text }
    elseif event.kind == 'leave' then tokens[#tokens + 1] = { kind = 'close', name = event.name }
    else error('invalid DocstrIL event kind') end
  end
  return M.from_tokens(tokens)
end
function M.walk(doc, handlers)
  local function visit(node)
    local result = {}
    for _, child in ipairs(node.children or {}) do
      local replacement = visit(child)
      if replacement ~= false then
        if replacement.t then result[#result + 1] = replacement
        else for _, item in ipairs(replacement) do result[#result + 1] = item end end
      end
    end
    if node.t ~= 'Text' then node.children = result end
    local alias = M.aliases[node.t]
    if node.t == 'list' then alias = node.attrs.ordered == 'true' and 'OrderedList' or 'BulletList' end
    local fn = handlers[node.t] or handlers[alias or ''] or handlers.Element
    if fn then
      local replacement = fn(node)
      if replacement ~= nil then return replacement end
    end
    return node
  end
  return visit(doc)
end
-- The three filter contracts deliberately share one lossless IR.
function M.apply_filter(doc, filter, mode)
  if mode == 'walk' then return M.walk(doc, filter) end
  if mode == 'tokens' then return M.from_tokens(filter(M.tokens(doc))) end
  if mode == 'events' then return M.from_events(filter(M.events(doc))) end
  local result = filter(doc)
  if result == nil then return doc end
  return result
end
function M.stylesheet(doc, source, format)
  return _ldocstr_style(doc, source, format or 'css')
end
-- Works both as an automatically installed module and through a Mune host.
return M
