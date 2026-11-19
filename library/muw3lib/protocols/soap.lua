local common = require('muw3lib.protocols.common')
local M = {}
local namespaces = {
  ['1.1']='http://schemas.xmlsoap.org/soap/envelope/',
  ['1.2']='http://www.w3.org/2003/05/soap-envelope',
}
local suffix = '</soap:Body></soap:Envelope>'

local function version_or_default(version)
  if version == nil then version = '1.1' end
  assert(version == '1.1' or version == '1.2', 'SOAP version must be 1.1 or 1.2')
  return version
end

function M.envelope(inner, version)
  assert(type(inner) == 'string', 'SOAP XML body required')
  local namespace = namespaces[version_or_default(version)]
  local prefix = '<?xml version="1.0" encoding="UTF-8"?>' ..
    '<soap:Envelope xmlns:soap="' .. namespace .. '"><soap:Body>'
  return prefix .. inner .. suffix
end

local function action_value(action)
  if action == nil then return '' end
  assert(type(action) == 'string', 'SOAP action must be a URI reference')
  -- Existing callers may already supply a quoted SOAPAction header value.
  if #action >= 2 and action:sub(1, 1) == '"' and action:sub(-1) == '"' then
    action = action:sub(2, -2)
  end
  assert(not action:find('[^A-Za-z0-9%-._~:/?#%[%]@!$&\'()*+,;=%%]'),
    'SOAP action must be a URI reference')
  assert(not action:gsub('%%%x%x', ''):find('%%'), 'invalid percent encoding in SOAP action')
  return action
end

function M.new(endpoint, options)
  assert(options == nil or common.object(options), 'SOAP options must be a table')
  local version = version_or_default(options and options.version)
  local client, path = common.endpoint(endpoint, options)
  local obj = {raw=client}
  function obj.call(soap_action, body_xml, opts)
    local action = action_value(soap_action)
    local allow_error = common.flag(opts, 'allow_error')
    local headers
    if version == '1.1' then
      headers = {['Content-Type']='text/xml; charset=utf-8', SOAPAction='"' .. action .. '"'}
    else
      local content_type = 'application/soap+xml; charset=utf-8'
      if action ~= '' then
        assert(action:match('^[A-Za-z][A-Za-z0-9+%.%-]*:'), 'SOAP 1.2 action must be an absolute URI')
        content_type = content_type .. '; action="' .. action .. '"'
      end
      headers = {['Content-Type']=content_type}
    end
    local response = client:request('POST', path, {body=M.envelope(body_xml, version),
      headers=headers, allow_error=allow_error})
    -- Caller-supplied inner XML and the raw reply are not parsed or validated.
    return response.body, response
  end
  return obj
end

return M
