local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}
local function sid(value, label, prefix_pattern)
  s.id(value, label, prefix_pattern..'[A-Fa-f0-9]+$')
  assert(#value == 34, 'invalid '..label)
  return value
end
function M.new(account_sid, auth_token, options)
  sid(account_sid, 'Twilio account SID', '^AC')
  s.token(auth_token, 'Twilio auth token')
  local opt=s.object(options, 'Twilio options', true)
  local cli=c.new(s.base(opt.base_url, 'https://api.twilio.com'), {basic=c.basic(account_sid, auth_token)})
  local prefix='/2010-04-01/Accounts/'..account_sid
  local api={raw=cli}
  function api.messages(query) return cli:get(prefix..'/Messages.json', query) end
  function api.message(id)
    return cli:get(prefix..'/Messages/'..sid(id, 'Twilio message SID', '^[SM]M')..'.json')
  end
  function api.calls(query) return cli:get(prefix..'/Calls.json', query) end
  function api.get_call(id)
    return cli:get(prefix..'/Calls/'..sid(id, 'Twilio call SID', '^CA')..'.json')
  end
  function api.send_message(from, to, body, extra)
    if from ~= nil then s.text(from, 'Twilio sender') end
    if body ~= nil then s.text(body, 'message body') end
    local fields=s.extras({From=from, To=s.text(to, 'Twilio recipient'), Body=body},
      extra, 'Twilio message options', {From=true, Body=true})
    if fields.MessagingServiceSid ~= nil then
      sid(fields.MessagingServiceSid, 'Twilio messaging service SID', '^MG')
    end
    assert(from ~= nil or fields.MessagingServiceSid, 'Twilio sender or messaging service SID required')
    if fields.MediaUrl ~= nil then s.text(fields.MediaUrl, 'Twilio media URL') end
    if fields.ContentSid ~= nil then sid(fields.ContentSid, 'Twilio content SID', '^HX') end
    assert(body ~= nil or fields.MediaUrl or fields.ContentSid, 'Twilio message content required')
    return cli:json('POST', prefix..'/Messages.json', {form=fields})
  end
  function api.call(from, to, twiml_url, extra)
    s.text(twiml_url, 'Twilio TwiML URL')
    assert(twiml_url:match('^https?://[^/%s]+') and not twiml_url:find('[%s%c]'), 'invalid Twilio TwiML URL')
    local fields=s.extras({From=s.text(from, 'Twilio sender'), To=s.text(to, 'Twilio recipient'),
      Url=twiml_url}, extra, 'Twilio call options')
    return cli:json('POST', prefix..'/Calls.json', {form=fields})
  end
  return api
end
return M
