local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}
local statuses = {OK=true, ZERO_RESULTS=true, OVER_DAILY_LIMIT=true, OVER_QUERY_LIMIT=true,
  REQUEST_DENIED=true, INVALID_REQUEST=true, UNKNOWN_ERROR=true, NOT_FOUND=true,
  MAX_WAYPOINTS_EXCEEDED=true, MAX_ROUTE_LENGTH_EXCEEDED=true}
function M.new(api_key, options)
  s.token(api_key, 'Google Maps API key')
  local opt=s.object(options, 'Google Maps options', true)
  local cli=c.new(s.base(opt.base_url, 'https://maps.googleapis.com'))
  local places=c.new(s.base(opt.places_base_url, 'https://places.googleapis.com'))
  local api={raw=cli, places_raw=places}
  local function legacy(path, query)
    local data, response=cli:get(path, query)
    assert(type(data) == 'table' and statuses[data.status], 'invalid Google Maps response status')
    if data.status ~= 'OK' and data.status ~= 'ZERO_RESULTS' then
      error('Google Maps: '..data.status, 2)
    end
    return data, response
  end
  -- Geocoding API and legacy Directions API require service enablement.
  function api.geocode(address)
    return legacy('/maps/api/geocode/json', {address=s.text(address, 'address'), key=api_key})
  end
  function api.directions(origin, destination, params)
    local q=s.extras({origin=s.text(origin, 'origin'), destination=s.text(destination, 'destination'),
      key=api_key}, params, 'Google Directions options')
    return legacy('/maps/api/directions/json', q)
  end
  function api.search_places(text_query, field_mask, extra)
    s.token(field_mask, 'Places API New field mask')
    local payload=s.extras({textQuery=s.text(text_query, 'Places text query')}, extra,
      'Places Text Search options')
    return places:json('POST', '/v1/places:searchText', {json=payload,
      headers={['X-Goog-Api-Key']=api_key, ['X-Goog-FieldMask']=field_mask}})
  end
  return api
end
return M
