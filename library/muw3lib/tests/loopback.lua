-- Run only through the Python loopback fixture with the native Mune host.
package.path = assert(arg[1]) .. '/?.lua;' .. arg[1] .. '/?/init.lua;' .. package.path
local lib = require('muw3lib')
local w = require('lmuw3')
local base = assert(arg[2])
assert(w.request_redirect_control)

if arg[3] == 'host-disabled' then
  local reply = w.request{url=base .. '/redirect', follow_redirects=true}
  assert(reply.status == 307 and reply.redirects == 0)
  return
end

assert(lib.fetch == lib.browser.fetch)
local cli = lib.rest.new(base .. '/api', {headers={['X-Fixture']='default'}})
local data, reply = cli:json('GET', '/rest', {
  headers={['x-fixture']='override'},
  query={{'name','a & b'},{'name','%/+!'}}
})
assert(data.ok and reply.status == 200)
assert(reply:header('x-answer') == '42')
assert(reply:json().ok)
local body = lib.fetch(base .. '/binary', {method='POST', body='a\0b'})
assert(body.status == 200 and body.body == 'b\0a')
assert(lib.fetch(base .. '/not-found').status == 404)
assert(not pcall(function() cli:get('/not-found') end))

local graphql = lib.graphql.new(base .. '/graphql')
local value = graphql.execute('query Read { answer }', nil, 'Read')
assert(value.answer == 42)
local rpc = lib.jsonrpc.new(base .. '/rpc')
assert(rpc.call('example.read', {name='Mune'}).answer == 42)
local ordered = rpc.batch({
  {method='example.first'},
  {method='example.notice', notify=true},
  {method='example.second'},
})
assert(#ordered == 2 and ordered[1].result == 'example.first'
  and ordered[2].error.code == -32601)
assert(rpc.notify('example.notice').status == 204)
assert(lib.opencode.new({base_url=base}).info().version == '2.0-fixture')
local soap = lib.soap.new(base .. '/soap')
local xml = soap.call('urn:example', '<p:Read xmlns:p="urn:example"/>')
assert(xml:find('urn:reply', 1, true))
local soap12 = lib.soap.new(base .. '/soap12', {version='1.2'})
local _, soap_reply = soap12.call('urn:example', '<p:Read xmlns:p="urn:example"/>')
assert(soap_reply.status == 200)
local events, stream = lib.sse.fetch(base .. '/events')
assert(#events == 2 and events[1].id == '7' and events[2].id == '7')
assert(events[1].data == 'one\ntwo' and events[2].data == 'three' and stream.status == 200)

local flags = {follow_redirects=false, url=base .. '/redirect'}
reply = w.request(flags)
assert(reply.status == 307 and reply.redirects == 0)
assert(w.request{url=base .. '/redirect-same'}.body == 'host policy retained')
assert(not pcall(w.request, {url=base .. '/redirect', follow_redirects=0}))
reply = lib.fetch(base .. '/redirect', {
  method='POST', headers={['X-Fixture-Key']='test-only'}, body='test-only',
  query={key='test-only'},
})
assert(reply.status == 307 and reply.redirects == 0)
local good, message = pcall(function()
  return cli:request('POST','/redirect',{headers={['X-Fixture-Key']='test-only'},
    query={key='test-only'}, body='test-only'})
end)
assert(not good and not tostring(message):find('test-only', 1, true))
print('muw3lib native loopback passed')
