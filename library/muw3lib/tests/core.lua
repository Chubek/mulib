-- Offline HTTP contract tests: no native request is made.
package.path = 'library/?.lua;library/?/init.lua;' .. package.path
local last, reply, calls
calls = 0
local host = {request_redirect_control=true}
function host.request(spec)
  calls = calls + 1
  assert(spec.follow_redirects == false)
  last = spec
  return reply
end
package.loaded.lmuw3 = host
local core = require('muw3lib.core')
local browser = require('muw3lib.browser')

local function fails(message, f)
  local ok, err = pcall(f)
  assert(not ok and type(err) == 'string' and err:find(message, 1, true), tostring(err))
  return err
end

assert(core.encode(false) == 'false')
assert(core.encode('A /?é') == 'A%20%2F%3F%C3%A9')
assert(core.encode_path('foo/a b/%') == 'foo/a%20b/%25')
assert(core.basic('user', 'p:') == 'Basic dXNlcjpwOg==')
fails('invalid Basic auth', function() core.basic('u:ser', 'p') end)
local origin, path = core.split_url('https://[::1]:8443?x=/a://b')
assert(origin == 'https://[::1]:8443' and path == '/?x=/a://b')
origin, path = core.split_url('HTTPS://Example.test/a%2Fb')
assert(origin == 'HTTPS://Example.test' and path == '/a%2Fb')
origin, path = core.split_url('https://example.test')
assert(origin == 'https://example.test' and path == '/')
for _, url in ipairs({
  'ftp://example.test/', 'https:///tmp', 'https://user:pw@example.test/',
  'https://example.test:0/', 'https://example.test:65536/', 'https://example.test:/',
  'https://example.test/white space', 'https://example.test/odd%2', 'https://example.test/#fragment',
  'https://example.test//other.test',
  'https://example.test\\evil.test/', 'https://example.test/%2E.', 'https://example.test/raw\0',
}) do assert(not pcall(core.split_url, url)) end
fails('base URL cannot contain a query', function() core.new('https://example.test?x=y') end)

local default_headers = {{'Accept', 'application/json'}, {'X-Mode', 'old'}, {'X-Mode', 'also-old'}}
local cli = core.new('https://example.test/prefix///', {
  headers=default_headers, bearer='constructor-token', max_body_bytes=8,
})
local request_headers = {{'x-mode', 'one'}, {'X-Mode', 'two'}, {'authorization', 'custom'}}
reply = {status=200, body='{}'}
local decoded = {}
function reply:json() assert(self == reply); return decoded end
local data, response = cli:json('POST', '/route?link=https://example.test&', {
  headers=request_headers, bearer='request-token', query={enabled=false, count=2},
  form={{'item', true}, {'item', 0}},
})
assert(data == decoded and response == reply and calls == 1)
assert(last.url == 'https://example.test/prefix/route?link=https://example.test' and last.method == 'POST')
assert(last.query.enabled == 'false' and last.query.count == '2')
assert(last.form[1][1] == 'item' and last.form[1][2] == 'true')
assert(last.form[2][1] == 'item' and last.form[2][2] == '0')
assert(#last.headers == 4 and last.headers[1][1] == 'Accept')
assert(last.headers[2][2] == 'one' and last.headers[3][2] == 'two')
assert(last.headers[4][1] == 'Authorization' and last.headers[4][2] == 'Bearer request-token')
assert(#default_headers == 3 and default_headers[2][2] == 'old' and #request_headers == 3)
last.headers[1][2] = 'mutated'

assert(cli:request('GET', '/test?', {query={{'q', 1}, {'q', 2}}}) == reply)
assert(last.url == 'https://example.test/prefix/test' and last.query[2][2] == '2')
assert(last.headers[1][2] == 'application/json' and last.headers[4][2] == 'Bearer constructor-token')
cli:request('POST', '/blank', {query={}, body='a\0b'})
assert(last.query == nil and last.body == 'a\0b')
cli:request('POST', '/false-json', {json=false})
assert(last.json == false)

local before = calls
for _, p in ipairs({'https://other.test', '//other.test', '/a#fragment', '/a/../b', '/a/%2e%2E'}) do
  assert(not pcall(cli.request, cli, 'GET', p))
end
fails('invalid HTTP method', function() cli:request('GET\n', '/') end)
fails('choose body', function() cli:request('POST', '/', {json=false, body=''}) end)
fails('body must be a string', function() cli:request('POST', '/', {body=false}) end)
fails('invalid header name', function() cli:request('GET', '/', {headers={['bad name']='x'}}) end)
fails('invalid header value', function() cli:request('GET', '/', {headers={X='secret\r\nleak'}}) end)
fails('invalid header value', function() cli:request('GET', '/', {headers={X=123}}) end)
fails('ambiguous header', function() cli:request('GET', '/', {headers={Accept='a', accept='b'}}) end)
fails('managed by the HTTP host', function() cli:request('GET', '/', {headers={hOsT='x'}}) end)
fails('choose bearer or basic', function() cli:request('GET', '/', {bearer='x', basic='y'}) end)
fails('must be dense', function() cli:request('GET', '/', {query={[2]={'key','value'}}}) end)
fails('cannot mix', function() cli:request('GET', '/', {query={{'k','v'}, key='value'}}) end)
fails('cannot contain NUL', function() cli:request('GET', '/', {query={key='v\0'}}) end)
fails('finite numbers', function() cli:request('GET', '/', {query={key=math.huge}}) end)
local fields = {}
for i = 1, 257 do fields[i] = {'k', 'v'} end
fails('field limit exceeded', function() cli:request('GET', '/', {query=fields}) end)
fails('non-negative integer', function() cli:request('GET', '/', {max_body_bytes=0.5}) end)
fails('allow_error must be boolean', function() cli:request('GET', '/', {allow_error=1}) end)
fails('do not follow redirects', function() cli:request('GET', '/', {follow_redirects=true}) end)
fails('do not follow redirects', function() browser.fetch('https://example.test', {redirect='follow'}) end)
fails('unsupported browser fetch option: credentials', function()
  browser.fetch('https://example.test', {credentials='omit'})
end)
host.request_redirect_control = nil
fails('requires lmuw3 request redirect control', function() cli:request('GET', '/') end)
fails('requires lmuw3 request redirect control', function() browser.fetch('https://example.test') end)
host.request_redirect_control = true
assert(calls == before)

reply = {status=403, body='SECRET'}
local err = fails('HTTP 403', function() cli:request('GET', '/secret-path?token=secret-query') end)
assert(not err:find('SECRET', 1, true) and not err:find('secret-', 1, true))
assert(cli:request('GET', '/', {allow_error=true}) == reply)
assert(browser.fetch('https://example.test?x=y#fragment', {query={a=false}}) == reply)
assert(last.url == 'https://example.test?x=y' and last.query.a == 'false' and last.method == 'GET')
fails('HTTP 403', function() browser.fetch('https://example.test', {allow_error=false}) end)

reply = {status=302, body=''}
assert(browser.require('fetch')('https://example.test') == reply)
fails('HTTP 302', function() cli:request('GET', '/') end)
reply = {status=200, body='123456789'}
fails('exceeds max_body_bytes', function() cli:request('GET', '/') end)
assert(cli:request('GET', '/', {max_body_bytes=9}) == reply)
fails('exceeds max_body_bytes', function() browser.fetch('https://example.test', {max_body_bytes=0}) end)
reply = {status=204, body=''}
data, response = cli:json('GET', '/', {max_body_bytes=0})
assert(data == nil and response == reply)
reply = {status=200, body='invalid'}
function reply:json() error('SECRET') end
err = fails('invalid JSON response', function() cli:json('GET', '/') end)
assert(not err:find('SECRET', 1, true))
reply = {status='200', body=''}
fails('invalid MuW3 response', function() cli:request('GET', '/') end)
print('muw3lib core tests passed: ' .. calls .. ' requests intercepted')
