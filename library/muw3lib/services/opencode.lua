-- Bounded OpenCode V2 HTTP client. Routes, fields and success statuses are
-- from standards/opencode-v2/openapi.json, not the OpenCode V1 API.
local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}

local function session_id(id) return s.id(id, 'OpenCode session ID', '^ses') end
local function message_id(id) return s.id(id, 'OpenCode message ID', '^msg_') end
local function session(id) return '/api/session/'..c.encode(session_id(id)) end
local function order(value)
  assert(value == 'asc' or value == 'desc', 'invalid OpenCode order')
  return value
end
local function archived(value)
  assert(type(value) == 'boolean' or value == 'true' or value == 'false', 'invalid archived value')
  return tostring(value)
end
local function limit(value) return s.positive_id(value, 'OpenCode limit') end
local function query_text(value) return s.text(value, 'OpenCode query value') end
local function parent_id(value) return value == 'null' and value or session_id(value) end
local message_types = {['agent-switched']=true, ['model-switched']=true, ['location-switched']=true,
  user=true, synthetic=true, system=true, skill=true, shell=true, assistant=true, compaction=true, idle=true}
local function message_type(value)
  assert(type(value) == 'string' and message_types[value], 'invalid OpenCode message type')
  return value
end
local session_fields = {order=order, limit=limit, archived=archived, parentID=parent_id, search=query_text,
  directory=query_text, project=query_text, subpath=query_text, cursor=query_text}
local message_fields = {order=order, limit=limit, cursor=query_text, type=message_type}
local location_fields = {['location[directory]']=query_text}
local project_fields = {archived=archived}
local function checked_query(query, fields)
  local out=s.object(query, 'OpenCode query', true)
  for k,v in pairs(out) do
    assert(fields[k], 'unsupported OpenCode query parameter')
    out[k]=fields[k](v)
  end
  return next(out) and out or nil
end
local function has_authorization(headers)
  if type(headers) ~= 'table' then return false end
  for key,value in next,headers do
    local name=key
    if type(key) == 'number' and type(value) == 'table' then name=rawget(value, 1) end
    if type(name) == 'string' and name:lower() == 'authorization' then return true end
  end
  return false
end

function M.new(options)
  local opt=s.object(options, 'OpenCode options', true)
  assert(opt.allow_session_writes == nil or type(opt.allow_session_writes) == 'boolean',
    'invalid OpenCode allow_session_writes option')
  if opt.token ~= nil then
    s.token(opt.token, 'OpenCode token')
    assert(not has_authorization(opt.headers), 'choose OpenCode token or Authorization header')
  end
  local cli=c.new(s.base(opt.base_url, 'http://127.0.0.1:4096', true),
                  {bearer=opt.token, headers=opt.headers})
  local api={} -- No arbitrary method/path escape hatch on the V2 convenience client.
  local function json(method, path, spec)
    local data, response=cli:json(method, path, spec)
    assert(response.status == 200, 'unexpected OpenCode success status')
    assert(type(data) == 'table' and data ~= c.muw3.null, 'invalid OpenCode response')
    return data, response
  end
  local function get(path, query)
    return json('GET', path, {query=query})
  end
  local function writable()
    assert(opt.allow_session_writes == true, 'OpenCode session writes are not enabled')
  end
  local function at_location(path, query)
    return get(path, checked_query(query, location_fields))
  end
  function api.info() return get('/api/info') end
  function api.location(query) return at_location('/api/location', query) end
  function api.agents(query) return at_location('/api/agent', query) end
  function api.agent(id, query)
    return at_location('/api/agent/'..c.encode(s.id(id, 'OpenCode agent ID')), query)
  end
  function api.providers(query) return at_location('/api/provider', query) end
  function api.provider(id, query)
    return at_location('/api/provider/'..c.encode(s.id(id, 'OpenCode provider ID')), query)
  end
  function api.models(query)
    local q=s.object(query, 'OpenCode query', true)
    local t=q.types
    q.types=nil
    local location=checked_query(q, location_fields)
    if t == nil then return get('/api/model', location) end
    local pairs_out={}
    if location then pairs_out[1]={'location[directory]', location['location[directory]']} end
    if type(t) == 'string' then t={t} end
    assert(type(t) == 'table', 'invalid OpenCode model types')
    local n=rawlen(t)
    assert(n >= 1 and n <= 64, 'invalid OpenCode model types')
    for k in next,t do
      assert(type(k) == 'number' and k >= 1 and k <= n and k % 1 == 0, 'invalid OpenCode model types')
    end
    for i=1,n do pairs_out[#pairs_out+1]={'types', query_text(rawget(t, i))} end
    return get('/api/model', pairs_out)
  end
  function api.default_model(query) return at_location('/api/model/default', query) end
  function api.projects(query) return get('/api/project', checked_query(query, project_fields)) end
  function api.sessions(query) return get('/api/session', checked_query(query, session_fields)) end
  function api.session(id) return get(session(id)) end
  function api.messages(id, query)
    local q=checked_query(query, message_fields)
    assert(not q or not (q.cursor and q.order), 'OpenCode message cursor cannot be combined with order')
    return get(session(id)..'/message', q)
  end
  function api.message(id, msg)
    return get(session(id)..'/message/'..c.encode(message_id(msg)))
  end
  function api.create_session(payload)
    writable()
    local body=s.object(payload, 'OpenCode session payload', true)
    for k in pairs(body) do assert(k == 'title' or k == 'id', 'unsupported OpenCode session field') end
    if body.title ~= nil then s.text(body.title, 'OpenCode session title') end
    if body.id ~= nil then session_id(body.id) end
    return json('POST', '/api/session', {json=body})
  end
  function api.delete_session(id)
    writable()
    local response=cli:request('DELETE', session(id))
    assert(response.status == 204, 'unexpected OpenCode delete status')
    return nil, response
  end
  function api.prompt(id, payload)
    writable()
    local body=s.object(payload, 'OpenCode prompt payload')
    for k in pairs(body) do
      assert(k == 'text' or k == 'id' or k == 'resume' or k == 'delivery',
        'unsupported OpenCode prompt field')
    end
    s.text(body.text, 'OpenCode prompt text')
    if body.id ~= nil then message_id(body.id) end
    assert(body.resume == nil or body.resume == false, 'OpenCode prompt cannot resume execution')
    assert(body.delivery == nil or body.delivery == 'queue', 'OpenCode prompt only supports queue delivery')
    -- Queue only: this request neither wakes the session nor forces steering.
    body.resume, body.delivery=false, 'queue'
    return json('POST', session(id)..'/prompt', {json=body})
  end
  return api
end
return M
