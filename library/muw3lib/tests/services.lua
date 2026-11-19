-- Offline service contracts for Lua 5.4 and MuW3's Mune --weblet.
package.path = 'library/?.lua;library/?/init.lua;' .. package.path
local calls, pending = {}, nil
local null = {}
package.loaded.lmuw3 = {
  null=null, request_redirect_control=true,
  request=function(spec)
    assert(spec.follow_redirects == false, 'credentials must not follow redirects')
    calls[#calls+1]=spec
    local expected=pending or {}
    pending=nil
    local r={status=expected.status or 200, body=expected.body or '{}',
      url=spec.url, headers=expected.headers or {}, trailers={}}
    function r:json() return expected.json or {value=true} end
    return r
  end
}
local lib=require('muw3lib')
local function checked(method, url, fn, ...)
  local count=#calls
  local result, response=fn(...)
  assert(#calls == count+1, 'service should send exactly one request')
  local request=calls[#calls]
  assert(request.method == method, request.method..' != '..method)
  assert(request.url == url, request.url..' != '..url)
  assert(response and response.url == url, 'missing response metadata')
  return request, result, response
end
local function bad(message, fn, ...)
  local count=#calls
  local ok, err=pcall(fn, ...)
  assert(not ok and type(err) == 'string' and err:find(message, 1, true), tostring(err))
  assert(#calls == count, 'invalid input must not reach the transport')
end
local function remote_error(message, fixture, fn, ...)
  local count=#calls
  pending=fixture
  local ok, err=pcall(fn, ...)
  assert(not ok and type(err) == 'string' and err:find(message, 1, true), tostring(err))
  assert(not err:find('dummy-secret', 1, true), 'error exposed server text or credentials')
  assert(#calls == count+1, 'errors must not be retried or redirected')
end
local function header(request, name)
  local found
  for _,h in ipairs(request.headers) do
    if h[1]:lower() == name:lower() then
      assert(not found, 'repeated '..name)
      found=h[2]
    end
  end
  return found
end

local github=lib.github.new('dummy-secret')
local gh='https://api.github.com/repos/owner/repo'
local r=checked('GET', gh..'/issues/9007199254740999', github.issue, 'owner', 'repo', '9007199254740999')
assert(header(r, 'Authorization') == 'Bearer dummy-secret' and header(r, 'X-GitHub-Api-Version'))
checked('GET', gh..'/pulls/12', github.pull, 'owner', 'repo', 12)
checked('GET', gh..'/contents', github.contents, 'owner', 'repo')
r=checked('GET', gh..'/contents/dir/.hidden/a%20b%23.txt', github.contents,
  'owner', 'repo', 'dir/.hidden/a b#.txt', 'feature/main')
assert(r.query.ref == 'feature/main')
r=checked('POST', gh..'/issues', github.create_issue, 'owner', 'repo', {title='Offline fixture'})
assert(r.json.title == 'Offline fixture' and r.form == nil)
bad('invalid GitHub owner', github.repository, '..', 'repo')
bad('invalid GitHub repository', github.repository, 'owner', 'x/y')
bad('invalid issue number', github.issue, 'owner', 'repo', 0.5)
bad('invalid pull request number', github.pull, 'owner', 'repo', '0')
for _,path in ipairs({'/private', 'foo//bar', 'foo/', '../a', 'x/./y', 'x/%2e%2e/y', 'x\\y', 'x\0y'}) do
  bad('invalid repository path', github.contents, 'owner', 'repo', path)
end
bad('overrides a fixed field', github.search_repos, 'language:Lua', {q='unrelated'})
bad('requires HTTPS', lib.github.new, 'dummy-secret', {base_url='http://example.invalid'})
r=checked('GET', gh, lib.github.new().repository, 'owner', 'repo')
assert(header(r, 'Authorization') == nil)
remote_error('HTTP 307', {status=307, body='dummy-secret', headers={{'Location','https://other.invalid/'}}},
  github.create_issue, 'owner', 'repo', {title='Offline fixture'})

local gitlab=lib.gitlab.new('dummy-secret', {base_url='https://git.example.invalid/git/'})
local gl='https://git.example.invalid/git/api/v4/projects/'
r=checked('GET', gl..'owner%2Fteam%2Frepo/issues/13', gitlab.issue, 'owner/team/repo', 13)
assert(header(r, 'Private-Token') == 'dummy-secret' and not header(r, 'Authorization'))
checked('GET', gl..'12/merge_requests/8', gitlab.merge_request, 12, '8')
checked('GET', gl..'12/pipelines/6', gitlab.pipeline, '12', 6)
r=checked('GET', 'https://gitlab.com/api/v4/projects/12',
  lib.gitlab.new('fake-oauth', {oauth=true}).project, '12')
assert(header(r, 'Authorization') == 'Bearer fake-oauth' and not header(r, 'Private-Token'))
bad('invalid GitLab project path', gitlab.project, 'owner/%2E%2E')
bad('invalid GitLab project ID', gitlab.project, '0')
bad('invalid GitLab issue IID', gitlab.issue, 12, '42?admin=true')

local openai=lib.openai.new('dummy-secret', {organization='org-fixture', project='proj-fixture'})
r=checked('GET', 'https://api.openai.com/v1/models/ft%3Agpt-4.1%3Afixture', openai.model, 'ft:gpt-4.1:fixture')
assert(header(r, 'Authorization') == 'Bearer dummy-secret')
assert(header(r, 'OpenAI-Organization') == 'org-fixture' and header(r, 'OpenAI-Project') == 'proj-fixture')
checked('GET', 'https://api.openai.com/v1/responses/resp_fixture', openai.response, 'resp_fixture')
r=checked('POST', 'https://api.openai.com/v1/responses', openai.responses,
  {model='offline-fixture', input='hello', stream=false})
assert(r.json.stream == false and r.json.input == 'hello')
bad('do not stream', openai.responses, {stream=true})
bad('do not stream', openai.chat_completions, {stream='true'})
bad('must be an object', openai.embeddings, null)
bad('invalid OpenAI model ID', openai.model, 'x%2fy')
bad('invalid OpenAI API key', lib.openai.new, 'line1\r\nline2')

local maps=lib.googlemaps.new('dummy-secret')
pending={json={status='ZERO_RESULTS'}}
r=checked('GET', 'https://maps.googleapis.com/maps/api/geocode/json', maps.geocode, 'City, Region')
assert(r.query.address == 'City, Region' and r.query.key == 'dummy-secret' and not header(r, 'X-Goog-Api-Key'))
pending={json={status='OK'}}
r=checked('GET', 'https://maps.googleapis.com/maps/api/directions/json', maps.directions,
  'A', 'B', {mode='walking'})
assert(r.query.origin == 'A' and r.query.destination == 'B' and r.query.mode == 'walking')
r=checked('POST', 'https://places.googleapis.com/v1/places:searchText', maps.search_places,
  'coffee', 'places.displayName,nextPageToken', {pageToken='some/token=='})
assert(r.query == nil and header(r, 'X-Goog-Api-Key') == 'dummy-secret')
assert(header(r, 'X-Goog-FieldMask') == 'places.displayName,nextPageToken'
  and r.json.textQuery == 'coffee' and r.json.pageToken == 'some/token==')
bad('overrides a fixed field', maps.directions, 'A', 'B', {key='override'})
bad('overrides a fixed field', maps.search_places, 'coffee', 'places.displayName', {textQuery='tea'})
bad('invalid Places API New field mask', maps.search_places, 'coffee', 'a\r\nb')
bad('requires HTTPS', lib.googlemaps.new, 'dummy-secret', {places_base_url='http://example.invalid'})
remote_error('Google Maps: REQUEST_DENIED', {json={status='REQUEST_DENIED', error_message='dummy-secret'}},
  maps.geocode, 'City')
remote_error('invalid Google Maps response status', {json={status='dummy-secret'}}, maps.geocode, 'City')

local stripe=lib.stripe.new('dummy-secret', {api_version='2024-06-20'})
r=checked('POST', 'https://api.stripe.com/v1/payment_intents', stripe.create_payment_intent,
  {amount=200, currency='usd', ['metadata[invoice]']='offline'}, 'idempotent-fixture')
assert(r.json == nil and r.form.amount == '200' and r.form['metadata[invoice]'] == 'offline')
assert(header(r, 'Stripe-Version') == '2024-06-20' and header(r, 'Idempotency-Key') == 'idempotent-fixture')
checked('GET', 'https://api.stripe.com/v1/checkout/sessions/cs_fixture', stripe.checkout_session, 'cs_fixture')
r=checked('GET', 'https://api.stripe.com/v1/subscriptions/sub_fixture', stripe.subscription, 'sub_fixture')
assert(not header(r, 'Idempotency-Key'))
bad('invalid payment intent ID', stripe.payment_intent, '../xxx')
bad('exceeds 255 bytes', stripe.create_customer, {}, string.rep('x', 256))
bad('Stripe form fields required', stripe.create_customer, false)

local slack=lib.slack.new('dummy-secret')
pending={json={ok=true}}
r=checked('GET', 'https://slack.com/api/conversations.history', slack.history,
  'C123', 3, {cursor='opaque/token=='})
assert(r.query.channel == 'C123' and r.query.limit == '3' and r.query.cursor == 'opaque/token==' and r.json == nil)
pending={json={ok=true}}
r=checked('POST', 'https://slack.com/api/chat.postMessage', slack.post_message,
  'C123', 'hello', {unfurl_links=false})
assert(r.json.channel == 'C123' and r.json.text == 'hello' and r.json.unfurl_links == false)
assert(header(r, 'Authorization') == 'Bearer dummy-secret')
pending={json={ok=true}}
r=checked('POST', 'https://slack.com/api/chat.postMessage', slack.post_message,
  '#test-channel', nil, {markdown_text='**hello**'})
assert(r.json.text == nil and r.json.markdown_text == '**hello**')
bad('invalid Slack method', slack.call, '../auth.test', {})
bad('invalid Slack method', slack.call, 'auth..test', {})
bad('must use the constructor', slack.call, 'auth.test', {token='override'})
bad('overrides a fixed field', slack.post_message, 'C123', 'hello', {channel='C456'})
remote_error('Slack reported an API error', {json={ok=false, error='dummy-secret'}}, slack.auth_test)
remote_error('invalid Slack response', {json={ok='true'}}, slack.auth_test)

local sid='AC'..string.rep('0', 32)
local twilio=lib.twilio.new(sid, 'twilio-test')
local tw='https://api.twilio.com/2010-04-01/Accounts/'..sid
local msg='SM'..string.rep('1', 32)
r=checked('GET', tw..'/Messages/'..msg..'.json', twilio.message, msg)
assert(header(r, 'Authorization') == 'Basic QUMwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDp0d2lsaW8tdGVzdA==')
assert(r.query == nil)
r=checked('POST', tw..'/Messages.json', twilio.send_message, nil, '+15555550111', 'hello',
  {MessagingServiceSid='MG'..string.rep('2', 32)})
assert(r.form.To == '+15555550111' and r.form.Body == 'hello' and r.form.From == nil)
local media='https://voice.example.invalid/offline.jpg'
r=checked('POST', tw..'/Messages.json', twilio.send_message, '+15555550112', '+15555550111', nil, {MediaUrl=media})
assert(r.form.Body == nil and r.form.MediaUrl == media)
local webhook='https://voice.example.invalid/twiml'
r=checked('POST', tw..'/Calls.json', twilio.call, '+15555550112', '+15555550111', webhook)
assert(r.form.Url == webhook and r.form.From == '+15555550112')
checked('GET', tw..'/Calls.json', twilio.calls)
local call='CA'..string.rep('3', 32)
checked('GET', tw..'/Calls/'..call..'.json', twilio.get_call, call)
bad('invalid Twilio account SID', lib.twilio.new, 'ACdemo', 'dummy-secret')
bad('invalid Twilio message SID', twilio.message, sid)
bad('overrides a fixed field', twilio.send_message, 'A', 'B', 'hello', {To='C'})

local wiki=lib.wikipedia.new({language='simple', user_agent='muw3lib-test (contact: user@example.invalid)'})
local wk='https://simple.wikipedia.org'
r=checked('GET', wk..'/w/api.php', wiki.search, 'HTTP & Lua', 5)
assert(r.query.srsearch == 'HTTP & Lua' and r.query.srlimit == '5')
assert(header(r, 'User-Agent') == 'muw3lib-test (contact: user@example.invalid)')
checked('GET', wk..'/api/rest_v1/page/summary/Caf%C3%A9%20au%20lait%2FHistory', wiki.summary, 'Café au lait/History')
bad('invalid Wikipedia language code', lib.wikipedia.new, {language='en.example.invalid'})
bad('invalid Wikipedia search limit', wiki.search, 'HTTP', 501)
bad('invalid Wikipedia page title', wiki.summary, '..')
remote_error('MediaWiki reported an API error', {json={error={info='dummy-secret'}}}, wiki.page, 'Test')

local weather=lib.openweather.new('dummy-secret', {units='imperial'})
pending={json={cod='200'}}
r=checked('GET', 'https://api.openweathermap.org/data/2.5/forecast', weather.forecast_coordinates, 0, 180)
assert(r.query.lat == '0' and r.query.lon == '180' and r.query.appid == 'dummy-secret' and r.query.units == 'imperial')
pending={json={cod=200}}
r=checked('GET', 'https://api.openweathermap.org/data/2.5/weather', weather.current, 'Helsinki', 'FI')
assert(r.query.q == 'Helsinki,FI')
bad('invalid latitude', weather.coordinates, math.huge, 10)
bad('invalid longitude', weather.forecast_coordinates, 0, 181)
bad('invalid country code', weather.current, 'Helsinki', 'FI,extra')
bad('invalid OpenWeather units', lib.openweather.new, 'dummy-secret', {units='kelvins'})
remote_error('OpenWeather reported an error', {json={cod='401', message='dummy-secret'}}, weather.current, 'City')

local oc=lib.opencode.new()
local origin='http://127.0.0.1:4096'
r=checked('GET', origin..'/api/info', oc.info)
assert(not header(r, 'Authorization'))
assert(oc.raw == nil and oc.shell == nil and oc.command == nil and oc.pty == nil)
bad('requires HTTPS or a loopback IP', lib.opencode.new, {token='dummy-secret', base_url='http://127.0.0.1.example.invalid'})
bad('requires HTTPS or a loopback IP', lib.opencode.new, {base_url='http://localhost:4096'})
bad('requires HTTPS or a loopback IP', lib.opencode.new, {base_url='http://127.0.0.1@elsewhere.invalid'})
bad('choose OpenCode token or Authorization header', lib.opencode.new,
  {token='dummy-secret', headers={{'aUtHoRiZaTiOn','Basic fixture'}}})
local alternate=lib.opencode.new({base_url='http://[::1]:4096', headers={Authorization='Basic fixture'}})
r=checked('GET', 'http://[::1]:4096/api/info', alternate.info)
assert(header(r, 'Authorization') == 'Basic fixture')
bad('session writes are not enabled', oc.create_session)
bad('session writes are not enabled', oc.delete_session, 'ses_fixture')
bad('session writes are not enabled', oc.prompt, 'ses_fixture', {text='hello'})
oc=lib.opencode.new({base_url='https://oc.example.invalid', token='dummy-secret', allow_session_writes=true})
origin='https://oc.example.invalid'
r=checked('GET', origin..'/api/agent/build%20fixture', oc.agent, 'build fixture',
  {['location[directory]']='/root/project'})
assert(r.query['location[directory]'] == '/root/project' and header(r, 'Authorization') == 'Bearer dummy-secret')
r=checked('GET', origin..'/api/model', oc.models, {types={'chat', 'decision'}})
assert(#r.query == 2 and r.query[1][1] == 'types' and r.query[1][2] == 'chat'
  and r.query[2][1] == 'types' and r.query[2][2] == 'decision')
r=checked('GET', origin..'/api/session', oc.sessions, {cursor='opaque/+=', archived=false, parentID='null'})
assert(r.query.cursor == 'opaque/+=' and r.query.archived == 'false' and r.query.parentID == 'null')
checked('GET', origin..'/api/session/ses_fixture/message/msg_one', oc.message, 'ses_fixture', 'msg_one')
bad('invalid OpenCode session ID', oc.session, 'legacy')
bad('invalid OpenCode session ID', oc.session, 'ses_a%2Fshell')
bad('invalid OpenCode message ID', oc.message, 'ses_fixture', 'message')
bad('unsupported OpenCode query parameter', oc.sessions, {unknown='no'})
bad('cannot be combined with order', oc.messages, 'ses_fixture', {cursor='x', order='desc'})
bad('invalid OpenCode model types', oc.models, {types={name='chat'}})
bad('unsupported OpenCode session field', oc.create_session, {parentID='ses_fixture'})
bad('unsupported OpenCode session field', oc.create_session, {permissions={}})
r=checked('POST', origin..'/api/session', oc.create_session, {title='Offline fixture'})
assert(r.json.title == 'Offline fixture')
bad('unsupported OpenCode prompt field', oc.prompt, 'ses_fixture', {parts={{type='text', text='V1'}}})
bad('unsupported OpenCode prompt field', oc.prompt, 'ses_fixture', {text='hello', agents={}})
bad('invalid OpenCode prompt text', oc.prompt, 'ses_fixture', {})
bad('cannot resume execution', oc.prompt, 'ses_fixture', {text='hello', resume=true})
bad('only supports queue delivery', oc.prompt, 'ses_fixture', {text='hello', delivery='steer'})
local payload={text='hello', id='msg_fixture'}
r=checked('POST', origin..'/api/session/ses_fixture/prompt', oc.prompt, 'ses_fixture', payload)
assert(r.json.text == 'hello' and r.json.id == 'msg_fixture' and r.json.resume == false and r.json.delivery == 'queue')
assert(payload.resume == nil and payload.delivery == nil)
pending={status=204, body=''}
local req, result, response=checked('DELETE', origin..'/api/session/ses_fixture', oc.delete_session, 'ses_fixture')
assert(req.json == nil and result == nil and response.status == 204)
remote_error('unexpected OpenCode delete status', {status=200}, oc.delete_session, 'ses_fixture')
remote_error('unexpected OpenCode success status', {status=201}, oc.info)

print('muw3lib service tests passed: '..#calls..' HTTP calls intercepted')
