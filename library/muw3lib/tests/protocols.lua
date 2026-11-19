-- Offline protocol contracts; no native network calls on Lua or Mune.
package.path = 'library/?.lua;library/?/init.lua;' .. package.path

local calls = {}
local responder
local null, array_mt = {}, {}
local function array(t) return setmetatable(t or {}, array_mt) end
package.loaded.lmuw3 = {
  null=null,
  array=array,
  request_redirect_control=true,
  request=function(request)
    assert(request.follow_redirects == false, 'protocol requests must not follow redirects')
    calls[#calls + 1] = request
    return responder(request)
  end,
}

local function reply(status, body, parsed, content_type)
  local r = {status=status, body=body, headers={}, trailers={}}
  if content_type then r.headers[1] = {'Content-Type', content_type} end
  function r:json()
    assert(parsed ~= nil, 'mock invalid JSON')
    return parsed
  end
  function r:header(name)
    for _, h in ipairs(self.headers) do
      if h[1]:lower() == name:lower() then return h[2] end
    end
  end
  return r
end

local function reject(fn, fragment)
  local ok, message = pcall(fn)
  assert(not ok, 'expected rejection')
  if fragment then
    assert(tostring(message):find(fragment, 1, true), 'wrong error: ' .. tostring(message))
  end
  return tostring(message)
end

local function no_request(fn, fragment)
  local count = #calls
  reject(fn, fragment)
  assert(#calls == count, 'invalid protocol input reached the transport')
end

local function header(request, name)
  for _, h in ipairs(request.headers) do
    if h[1]:lower() == name:lower() then return h[2] end
  end
end

local rest = require('muw3lib.protocols.rest')
local graphql = require('muw3lib.protocols.graphql')
local jsonrpc = require('muw3lib.protocols.jsonrpc')
local soap = require('muw3lib.protocols.soap')
local sse = require('muw3lib.protocols.sse')

local client = rest.new('https://example.invalid')
responder = function(r)
  if r.method == 'HEAD' or r.method == 'OPTIONS' then return reply(204, '') end
  return reply(200, '{}', {verb=r.method})
end
local value, response = client:get('/read')
assert(value.verb == 'GET' and response.status == 200)
assert(client:put('/upsert', {value=false}).verb == 'PUT')
assert(calls[#calls].json.value == false)
assert(client:patch('/edit', {value=null}).verb == 'PATCH')
assert(calls[#calls].json.value == null)
assert(client:delete('/remove', {revision=42}).verb == 'DELETE')
assert(calls[#calls].query.revision == '42')
assert(client:head('/read').status == 204 and calls[#calls].method == 'HEAD')
assert(client:options('/read').status == 204 and calls[#calls].method == 'OPTIONS')

for _, endpoint in ipairs({
  'https://', 'ftp://example.invalid/graphql', 'https://user@example.invalid/graphql',
  'https://example.invalid/graphql#fragment', 'https://example.invalid/bad path',
  'https://example.invalid\\bad', '//example.invalid',
}) do
  no_request(function() graphql.new(endpoint) end)
end

local gql = graphql.new('https://example.invalid?tenant=test')
responder = function(r)
  assert(r.url == 'https://example.invalid/?tenant=test')
  assert(r.method == 'POST' and r.json.query == '{ field }')
  assert(r.json.variables == nil and r.json.operationName == nil)
  assert(header(r, 'Accept') == 'application/graphql-response+json, application/json')
  return reply(200, '{}', {data={field=false}})
end
local data, envelope, raw = gql.execute('{ field }')
assert(data.field == false and envelope.data == data and raw.status == 200)

responder = function(r)
  assert(r.json.variables.required == null and r.json.operationName == 'Test_1')
  return reply(200, '{}', {data={field=null}, errors=array({{message='UNTRUSTED'}})})
end
local errmsg = reject(function() gql.execute('query Test_1 { field }', {required=null}, 'Test_1') end,
  'GraphQL remote error')
assert(not errmsg:find('UNTRUSTED', 1, true))
data, envelope = gql.execute('query Test_1 { field }', {required=null}, 'Test_1', {allow_errors=true})
assert(data.field == null and envelope.errors[1].message == 'UNTRUSTED')

responder = function() return reply(200, '{}', {errors=array({{message='request failed'}})}) end
data, envelope = gql.execute('{ field }', nil, nil, {allow_errors=true})
assert(data == nil and envelope.errors[1].message == 'request failed')
responder = function() return reply(200, '{}', {data=null}) end
assert(gql.execute('{ field }') == null)

no_request(function() gql.execute('  ') end, 'query required')
no_request(function() gql.execute('{ field }', array()) end, 'variables must be an object')
no_request(function() gql.execute('{ field }', null) end, 'variables must be an object')
no_request(function() gql.execute('{ field }', setmetatable({}, {__index={p=true}})) end,
  'variables must be an object')
no_request(function() gql.execute('{ field }', nil, 'No Spaces') end, 'operation name')
no_request(function() gql.execute('{ field }', nil, nil, {allow_errors='yes'}) end, 'must be boolean')
for _, invalid in ipairs({
  null, array(), {}, false, {data=true}, {data=array()}, {data={}, extensions=array()},
  {data={}, errors=null}, {data={}, errors={}}, {data={}, errors=array()},
  {data={}, errors={false}}, {errors={{message='valid first'}, {message=42}}},
  {errors={[2]={message='sparse'}}},
}) do
  responder = function() return reply(200, '{}', invalid) end
  reject(function() gql.execute('{ field }', nil, nil, {allow_errors=true}) end, 'invalid GraphQL')
end
responder = function() return reply(204, '') end
reject(function() gql.execute('{ field }') end, 'invalid GraphQL')
responder = function() return reply(200, 'not JSON') end
reject(function() gql.execute('{ field }') end, 'invalid JSON response')
responder = function()
  return reply(400, '{}', {errors=array({{message='syntax error'}})}, 'application/graphql-response+json')
end
reject(function() gql.execute('{ field }') end, 'HTTP 400')
data, envelope, raw = gql.execute('{ field }', nil, nil, {allow_errors=true})
assert(data == nil and envelope.errors[1].message == 'syntax error' and raw.status == 400)
responder = function() return reply(400, '{}', {errors={{message='not trusted'}}}, 'application/json') end
reject(function() gql.execute('{ field }', nil, nil, {allow_errors=true}) end, 'HTTP 400')
responder = function()
  return reply(400, '{}', {data={}, errors={{message='invalid'}}}, 'application/graphql-response+json')
end
reject(function() gql.execute('{ field }', nil, nil, {allow_errors=true}) end, 'invalid GraphQL response')
responder = function() return reply(200, '{}', {data={}}, 'text/plain') end
reject(function() gql.execute('{ field }') end, 'media type')
responder = function()
  return reply(302, '{}', {errors={{message='redirect'}}}, 'application/graphql-response+json')
end
reject(function() gql.execute('{ field }', nil, nil, {allow_errors=true}) end, 'HTTP 302')

local rpc = jsonrpc.new('https://example.invalid/rpc')
responder = function(r)
  assert(r.method == 'POST' and r.json.method == 'example' and r.json.jsonrpc == '2.0')
  assert(r.json.params == nil and header(r, 'Content-Type') == 'application/json')
  return reply(200, '{}', {jsonrpc='2.0', id=r.json.id, result=false})
end
value, envelope, raw = rpc.call('example')
assert(value == false and envelope.result == false and raw.status == 200)

responder = function(r)
  assert(getmetatable(r.json.params) == array_mt)
  return reply(200, '{}', {jsonrpc='2.0', id=r.json.id, result=null})
end
assert(rpc.call('list', array()) == null)
local outer_id
responder = function(r)
  if r.json.method == 'outer' then
    outer_id = r.json.id
    assert(rpc.call('inner') == 'inner')
  else
    assert(r.json.id == outer_id + 1)
  end
  return reply(200, '{}', {jsonrpc='2.0', id=r.json.id, result=r.json.method})
end
assert(rpc.call('outer') == 'outer')
no_request(function() rpc.call('') end, 'method')
no_request(function() rpc.notify('rpc.private') end, 'reserved')
no_request(function() rpc.call('bad', false) end, 'params')
no_request(function() rpc.call('bad', null) end, 'params')
no_request(function() rpc.call('bad', {[2]=42}) end, 'params')
no_request(function() rpc.call('bad', array({key='value'})) end, 'params')

local function single(transform)
  responder = function(r)
    return reply(200, '{}', transform({jsonrpc='2.0', id=r.json.id}))
  end
  return function() return rpc.call('example', nil, {allow_errors=true}) end
end
reject(single(function(r) r.result = 1; r.id = tostring(r.id); return r end), 'ID mismatch')
reject(single(function(r) r.result = 1; r.id = null; return r end), 'ID mismatch')
reject(single(function(r) r.result = 1; r.id = r.id - 1; return r end), 'ID mismatch')
reject(single(function(r) r.result = 1; r.jsonrpc = '1.0'; return r end), 'version')
reject(single(function(r) return {r} end), 'shape')
reject(single(function(r) return r end), 'exactly one')
reject(single(function(r) r.result = null; r.error = null; return r end), 'exactly one')
reject(single(function(r) r.error = null; return r end), 'invalid JSON-RPC error')
reject(single(function(r) r.error = {code=1.5, message='text'}; return r end), 'invalid JSON-RPC error')
reject(single(function(r) r.error = {code=1, message=42}; return r end), 'invalid JSON-RPC error')

local remote = single(function(r) r.error = {code=-32602, message='UNTRUSTED', data=null}; return r end)
value, envelope = remote()
assert(value == nil and envelope.error.code == -32602 and envelope.error.data == null)
errmsg = reject(function() rpc.call('example') end, 'JSON-RPC remote error')
assert(not errmsg:find('UNTRUSTED', 1, true))

responder = function(r)
  assert(r.json.id == nil and r.json.params == nil)
  return reply(204, '')
end
assert(rpc.notify('signal').status == 204)
responder = function() return reply(200, ' ') end
reject(function() rpc.notify('signal') end, 'must not receive a response body')
responder = function() return reply(200, '') end
reject(function() rpc.call('signal') end, 'invalid JSON-RPC response')

local batch = {{method='first'}, {method='observe', notify=true}, {method='last', params={7}}}
responder = function(r)
  local a, b, c = r.json[1], r.json[2], r.json[3]
  assert(a.method == 'first' and b.id == nil and c.params[1] == 7 and c.id == a.id + 1)
  return reply(200, '[]', array({
    {jsonrpc='2.0', id=c.id, error={code=-32602, message='bad params'}},
    {jsonrpc='2.0', id=a.id, result=null},
  }))
end
local ordered
ordered, raw = rpc.batch(batch)
assert(#ordered == 2 and ordered[1].result == null and ordered[2].error.code == -32602 and raw.status == 200)
assert(batch[1].id == nil and batch[3].id == nil)

responder = function(r)
  assert(#r.json == 1 and r.json[1].id == nil)
  return reply(204, '')
end
ordered, raw = rpc.batch({{method='observe', notify=true}})
assert(#ordered == 0 and raw.status == 204)
no_request(function() rpc.batch(array()) end, '1-1024')
no_request(function() rpc.batch({[2]={method='bad'}}) end, '1-1024')
no_request(function() rpc.batch({{method='first'}, {method='last', notify=1}}) end, 'flag')
local too_many = {}
for i = 1, 1025 do too_many[i] = {method='a'} end
no_request(function() rpc.batch(too_many) end, '1-1024')
responder = function() return reply(200, '{}', {}) end
reject(function() rpc.batch(batch) end, 'count or shape')
responder = function(r)
  local first = {jsonrpc='2.0', id=r.json[1].id, result=true}
  return reply(200, '[]', array({first, first}))
end
reject(function() rpc.batch(batch) end, 'duplicate response ID')
responder = function(r)
  return reply(200, '[]', array({
    {jsonrpc='2.0', id=r.json[1].id, result=true},
    {jsonrpc='2.0', id=r.json[3].id + 1, result=true},
  }))
end
reject(function() rpc.batch(batch) end, 'unexpected')

local soap11 = soap.new('https://example.invalid/soap')
local soap12 = soap.new('https://example.invalid/soap?x=1', {version='1.2'})
responder = function(r)
  assert(r.url == 'https://example.invalid/soap' and r.method == 'POST')
  assert(header(r, 'SOAPAction') == '"urn:example:test"')
  assert(header(r, 'Content-Type') == 'text/xml; charset=utf-8')
  assert(r.body:find('http://schemas.xmlsoap.org/soap/envelope/', 1, true))
  assert(r.body:find('<soap:Body><Item/></soap:Body>', 1, true))
  return reply(200, '<Envelope/>')
end
assert(soap11.call('"urn:example:test"', '<Item/>') == '<Envelope/>')
responder = function(r)
  assert(r.url == 'https://example.invalid/soap?x=1')
  assert(header(r, 'SOAPAction') == nil)
  assert(header(r, 'Content-Type') == 'application/soap+xml; charset=utf-8; action="urn:example:test"')
  assert(r.body:find('http://www.w3.org/2003/05/soap-envelope', 1, true))
  return reply(500, '<Fault/>')
end
value, raw = soap12.call('urn:example:test', '<Item/>', {allow_error=true})
assert(value == '<Fault/>' and raw.status == 500)
reject(function() soap12.call('urn:example:test', '<Item/>') end, 'HTTP 500')
no_request(function() soap12.call('relative', '<Item/>') end, 'absolute URI')
no_request(function() soap11.call('urn:a\r\nX-Invalid: 1', '<Item/>') end, 'URI reference')
no_request(function() soap11.call('urn:a"extra', '<Item/>') end, 'URI reference')
no_request(function() soap11.call('urn:a%1', '<Item/>') end, 'percent encoding')
no_request(function() soap11.call('', false) end, 'XML body')
no_request(function() soap.new('https://example.invalid', {version=true}) end, 'SOAP version')

local bom = '\239\187\191'
local events, state = sse.parse(bom .. ':comment\r\nid: saved\rretry: 000120\r\r' ..
  'event: update\ndata:  hello\r\ndata:world\r\n\r\nid:\r\nid: bad\0id\r\n' ..
  'retry: -5\nretry: 9007199254740992\nretry: 12345678901234567890\nevent:\ndata\n\n' ..
  'event: dropped\n\n' .. 'data: last\n\n' .. 'data: pending\n')
assert(#events == 3 and events[1].event == 'update' and events[1].data == ' hello\nworld')
assert(events[1].id == 'saved' and events[1].retry == nil)
assert(events[2].event == 'message' and events[2].data == '' and events[2].id == '')
assert(events[3].event == 'message' and events[3].data == 'last' and events[3].id == '')
assert(state.last_event_id == '' and state.retry == 120)
events, state = sse.parse('retry: 0\ndata: ok\r\r')
assert(#events == 1 and events[1].retry == 0 and state.retry == 0)
assert(#sse.parse('data: incomplete') == 0)
assert(#sse.parse('data: incomplete\r\n') == 0)
assert(#sse.parse('data: incomplete\r') == 0)
assert(#sse.parse('data: done\r\r') == 1)
assert(#sse.parse(bom .. bom .. 'data: only one BOM removed\n\n') == 0)
assert(sse.parse('data: é\n\n')[1].data == 'é')
reject(function() sse.parse('data: \255\n\n') end, 'valid UTF-8')
assert(#sse.parse('data\n\n', {max_body_bytes=6, max_events=1}) == 1)
assert(#sse.parse(': comment\n\n', {max_events=0}) == 0)
reject(function() sse.parse('data\n\n', {max_body_bytes=5}) end, 'max_body_bytes')
reject(function() sse.parse('data\n\ndata\n\n', {max_events=1}) end, 'max_events')
reject(function() sse.parse(('a'):rep(1048577)) end, 'max_body_bytes')
no_request(function() sse.fetch('https://example.invalid', nil, {max_events=-1}) end, 'max_events')
no_request(function() sse.fetch('https://example.invalid', nil, {max_body_bytes=1.5}) end, 'max_body_bytes')

responder = function(r)
  assert(r.url == 'https://example.invalid/?topic=test' and r.method == 'GET')
  assert(header(r, 'Accept') == 'text/event-stream' and header(r, 'X-Test') == '1')
  assert(header(r, 'Accept-Encoding') == 'identity')
  return reply(200, 'data: success\n\n', nil, 'Text/Event-Stream ; charset=UTF-8')
end
events, raw = sse.fetch('https://example.invalid?topic=test', {accept='application/json', ['X-Test']='1'})
assert(#events == 1 and events[1].data == 'success' and raw.status == 200)
responder = function() return reply(204, '') end
events, raw, state = sse.fetch('https://example.invalid')
assert(#events == 0 and raw.status == 204 and state.stopped)
responder = function() return reply(200, 'data: wrong type\n\n', nil, 'text/plain') end
reject(function() sse.fetch('https://example.invalid') end, 'text/event-stream')
responder = function() return reply(200, 'data: missing type\n\n') end
reject(function() sse.fetch('https://example.invalid') end, 'text/event-stream')
responder = function()
  local r = reply(200, 'data: ambiguous type\n\n', nil, 'text/event-stream')
  r.headers[2] = {'content-type','text/plain'}
  return r
end
reject(function() sse.fetch('https://example.invalid') end, 'text/event-stream')
responder = function()
  local r = reply(200, 'data: sample\n\n', nil, 'text/event-stream')
  r.headers[2] = {'Content-Encoding',' Identity '}
  return r
end
assert(sse.fetch('https://example.invalid')[1].data == 'sample')
reject(function() sse.fetch('https://example.invalid', nil, {max_body_bytes=1}) end, 'max_body_bytes')
reject(function() sse.fetch('https://example.invalid', nil, {max_events=0}) end, 'max_events')
for _, encoding in ipairs({'gzip', 'identity, gzip', 'identity, identity', ''}) do
  responder = function()
    local r = reply(200, 'data: sample\n\n', nil, 'text/event-stream')
    r.headers[2] = {'Content-Encoding', encoding}
    return r
  end
  reject(function() sse.fetch('https://example.invalid') end, 'Content-Encoding')
end
responder = function()
  local r = reply(200, 'data: sample\n\n', nil, 'text/event-stream')
  r.headers[2] = {'Content-Encoding','identity'}
  r.headers[3] = {'content-encoding','identity'}
  return r
end
reject(function() sse.fetch('https://example.invalid') end, 'Content-Encoding')
responder = function() return reply(201, '', nil, 'text/event-stream') end
reject(function() sse.fetch('https://example.invalid') end, 'HTTP 200 or 204')
responder = function() return reply(302, '') end
reject(function() sse.fetch('https://example.invalid') end, 'HTTP 302')
no_request(function() sse.fetch('https://example.invalid/#fragment') end, 'invalid URL')

local grpc = require('muw3lib.protocols.grpc')
local websocket = require('muw3lib.protocols.websocket')
assert(grpc.supported == false and websocket.supported == false)
no_request(grpc.new, 'unavailable')
no_request(websocket.new, 'unavailable')
print('muw3lib protocol tests passed: ' .. #calls .. ' HTTP calls intercepted')
