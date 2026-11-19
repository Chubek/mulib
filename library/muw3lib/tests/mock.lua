-- Offline contract tests; run with Lua 5.4 or with MuW3's Mune --weblet.
package.path = 'library/?.lua;library/?/init.lua;' .. package.path
local calls = {}
local array_mt = {}
package.loaded['lmuw3'] = {
  request_redirect_control = true,
  null = {},
  array = function(values) return setmetatable(values or {}, array_mt) end,
  request = function(spec)
    assert(spec.follow_redirects == false, 'adapter must disable redirects before sending')
    calls[#calls+1] = spec
    local reply = {status=200, body='{}', headers={}, trailers={}, url=spec.url}
    function reply:json()
      if spec.url:find('/api/auth.test', 1, true) or
         spec.url:find('/api/chat.postMessage', 1, true) then return {ok=true} end
      if spec.url:find('rpc', 1, true) then
        return {jsonrpc='2.0', id=spec.json.id, result={answer=42}}
      end
      if spec.url:find('graphql', 1, true) then return {data={hello='world'}} end
      if spec.url:find('maps.googleapis.com', 1, true) then return {status='OK', value=true} end
      return {value=true, id='session_1'}
    end
    function reply:header(name) return nil end
    return reply
  end,
  form = function(t) return 'encoded-query' end,
  json_encode = function(t) return '{}' end,
  json_decode = function(t) return {} end,
}
local lib = require('muw3lib')
assert(lib.core.encode('A /?') == 'A%20%2F%3F')
assert(lib.core.encode_path('dir/hello world.lua') == 'dir/hello%20world.lua')
assert(lib.core.base64('user:pass') == 'dXNlcjpwYXNz')
assert(lib.core.base64('a') == 'YQ==')
assert(lib.core.base64('ab') == 'YWI=')
assert(lib.core.xml_escape('<&"\'>') == '&lt;&amp;&quot;&apos;&gt;')
local github = lib.github.new('example-token')
assert(github.repository('Chubek','mulib').value)
assert(calls[#calls].url == 'https://api.github.com/repos/Chubek/mulib')
assert(github.create_issue('a','b',{title='test'}).value)
assert(github.contents('x','y','dir/a b.lua').value)
assert(calls[#calls].url == 'https://api.github.com/repos/x/y/contents/dir/a%20b.lua')
local openai = lib.openai.new('example-key')
assert(openai.responses({model='gpt-4.1', input='hi'}).value)
local maps = lib.googlemaps.new('example-key')
assert(maps.geocode('Berlin').value)
assert(maps.directions('A','B').value)
assert(maps.search_places('pizza', 'places.displayName').value)
assert(calls[#calls].url == 'https://places.googleapis.com/v1/places:searchText')
local stripe = lib.stripe.new('sk_test_example')
assert(stripe.create_payment_intent({amount='100', currency='usd'}, 'req-1').value)
local slack = lib.slack.new('xoxb-fake')
assert(slack.auth_test().ok)
assert(slack.post_message('C123','hello').ok)
local twilio = lib.twilio.new('AC' .. string.rep('a', 32),'example')
assert(twilio.send_message('+15555550100','+15555550101','hello').value)
local gitlab = lib.gitlab.new('test-token')
assert(gitlab.project('a/b').value)
assert(calls[#calls].url:find('a%%2Fb'))
local oc = lib.opencode.new({allow_session_writes=true})
assert(oc.info().value)
assert(oc.prompt('ses_fixture',{text='hello'}).value)
assert(calls[#calls].url == 'http://127.0.0.1:4096/api/session/ses_fixture/prompt')
local wiki = lib.wikipedia.new()
assert(wiki.search('Mune', 3).value)
local ow = lib.openweather.new('test-key')
assert(ow.current('Helsinki','FI').value)
local gql = lib.graphql.new('https://api.example.org/graphql')
assert(gql.execute('{ hello }').hello == 'world')
local rpc = lib.jsonrpc.new('https://api.example.org/rpc')
assert(rpc.call('answer',{}).answer == 42)
local soap = lib.soap.new('https://api.example.org/soap')
local xml = soap.call('urn:test', '<ns:Test/>')
assert(xml == '{}')
local events = lib.sse.parse('event: update\nid: 3\ndata: hello\ndata: world\n\n')
assert(#events == 1 and events[1].id == '3' and events[1].data == 'hello\nworld')
assert(lib.unsupported.reason('websocket'))
assert(lib.browser.available('fetch'))
assert(not lib.browser.available('dom'))
assert(not pcall(lib.websocket.new))
assert(not pcall(lib.grpc.new))
assert(not pcall(lib.browser.require, 'indexeddb'))
assert(#calls >= 19)
print('muw3lib mock tests passed: ' .. #calls .. ' HTTP calls intercepted')
