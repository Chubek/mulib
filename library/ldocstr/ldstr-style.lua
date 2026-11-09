-- Lua stylesheets are ordinary document filters. CSS implements the cascade.
local M = ldocstr
local S = {}
local function keys(value)
  local result = {}; for key in pairs(value) do result[#result + 1] = key end
  table.sort(result); return result
end
function S.css(rules)
  local result = {}
  for _, selector in ipairs(keys(rules)) do
    assert(not selector:find('[{}]'), 'invalid stylesheet selector')
    result[#result + 1] = selector .. ' {'
    for _, property in ipairs(keys(rules[selector])) do
      local value = tostring(rules[selector][property])
      assert(property:match('^[%w_-]+$') and not value:find('[;{}]'), 'invalid stylesheet declaration')
      result[#result + 1] = property .. ':' .. value .. ';'
    end
    result[#result + 1] = '}'
  end
  return table.concat(result, '\n')
end
function S.apply(doc, rules) return M.stylesheet(doc, S.css(rules), 'css') end
function S.filter(rules)
  local css = S.css(rules)
  return function(doc)
    -- Defer until the host has regenerated heading/term indices. This also
    -- lets selectors style newly generated TOC links, not discarded snapshots.
    doc.children[#doc.children + 1] = M.Element('stylesheet', css, {format = 'css'})
    return doc
  end
end
function S.set(node, declarations)
  local css = S.css({ ['*'] = declarations })
  local wrapper = M.Doc({ node })
  return M.stylesheet(wrapper, css, 'css').children[1]
end
M.style = S
return S
