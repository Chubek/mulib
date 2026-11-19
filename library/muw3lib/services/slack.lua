local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}
function M.new(bot_token, options)
  s.token(bot_token, 'Slack token')
  local opt=s.object(options, 'Slack options', true)
  local cli=c.new(s.base(opt.base_url, 'https://slack.com'), {bearer=bot_token})
  local api={raw=cli}
  local function checked(verb, method, spec)
    local r, response=cli:json(verb, '/api/'..method, spec)
    assert(type(r) == 'table' and type(r.ok) == 'boolean', 'invalid Slack response')
    if not r.ok then error('Slack reported an API error', 2) end
    return r, response
  end
  -- Only for caller-selected Slack methods that accept POST with a JSON body.
  function api.call(method, payload)
    s.id(method, 'Slack method', '^[A-Za-z][A-Za-z0-9.]*$')
    assert(method:sub(-1) ~= '.' and not method:find('..', 1, true), 'invalid Slack method')
    local body=s.object(payload, 'Slack payload', true)
    assert(body.token == nil, 'Slack token must use the constructor')
    return checked('POST', method, {json=body})
  end
  function api.auth_test() return api.call('auth.test') end
  function api.post_message(channel, text, extra)
    if text ~= nil then s.text(text, 'Slack message text') end
    local payload=s.extras({channel=s.text(channel, 'Slack channel'), text=text},
      extra, 'Slack message options', {text=true})
    assert(text ~= nil or payload.blocks ~= nil or payload.attachments ~= nil
      or payload.markdown_text ~= nil, 'Slack message content required')
    return api.call('chat.postMessage', payload)
  end
  function api.history(channel, limit, extra)
    if limit ~= nil then limit=s.positive_id(limit, 'Slack history limit') end
    local query=s.extras({channel=s.id(channel, 'Slack channel', '^[A-Za-z0-9]+$'), limit=limit},
      extra, 'Slack history options', {limit=true, token=true})
    return checked('GET', 'conversations.history', {query=query})
  end
  return api
end
return M
