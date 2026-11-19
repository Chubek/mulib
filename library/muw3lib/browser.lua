-- Browser capability registry. Unsupported standards are NOT emulated.
local core = require('muw3lib.core')
local M = {}
M.capabilities = {
  dom=false, fetch=true, websocket=false, webstorage=false,
  indexeddb=false, serviceworker=false, canvas=false, webgl=false,
  webrtc=false, webaudio=false, geolocation=false, webcrypto=false,
  file=false, webworkers=false,
}
M.explanations = {
  dom='No document/window DOM in a native Mune Weblet',
  websocket='The Mune lmuw3 binding cannot upgrade HTTP to a duplex socket',
  webstorage='No browser origin-scoped storage or session lifetime',
  indexeddb='No browser IndexedDB transactional engine',
  serviceworker='No browser service worker registration and lifecycle',
  canvas='No browser CanvasRenderingContext2D',
  webgl='No browser GPU/WebGL rendering context',
  webrtc='No peer-connection/media stack or device permissions',
  webaudio='No browser AudioContext or audio processing graph',
  geolocation='No device location permission and position API',
  webcrypto='No browser SubtleCrypto/CryptoKey bindings',
  file='Mune io is not the browser File/Blob and permission API',
  webworkers='No browser JS Worker message/event loop model',
}
function M.available(name) return M.capabilities[name] == true end
function M.require(name)
  assert(type(name) == 'string', 'browser API name must be a string')
  if name == 'fetch' then return M.fetch end
  error(name .. ': ' .. (M.explanations[name] or 'unknown browser API'), 2)
end
local browser_only_options = {
  'cache', 'credentials', 'integrity', 'keepalive', 'mode',
  'referrer', 'referrerPolicy', 'signal', 'window', 'duplex', 'priority',
}
-- Synchronous transport only: returns the native response for any HTTP status.
-- Redirects are not followed, and there is no browser security or lifecycle state.
function M.fetch(url, options)
  if type(options) == 'table' then
    for _, key in ipairs(browser_only_options) do
      assert(options[key] == nil, 'unsupported browser fetch option: ' .. key)
    end
  end
  return core.request(url, options)
end
return M
