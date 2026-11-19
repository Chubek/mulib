local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}
function M.new(api_key, options)
  s.token(api_key, 'OpenWeather API key')
  local opt=s.object(options, 'OpenWeather options', true)
  local cli=c.new(s.base(opt.base_url, 'https://api.openweathermap.org'))
  local units=opt.units
  if units == nil then units='metric' end
  assert(units == 'standard' or units == 'metric' or units == 'imperial', 'invalid OpenWeather units')
  local api={raw=cli}
  local function request(path, query)
    query.appid, query.units=api_key, units
    local result, response=cli:get(path, query)
    assert(type(result) == 'table' and result ~= c.muw3.null, 'invalid OpenWeather response')
    if result.cod ~= nil and result.cod ~= 200 and result.cod ~= '200' then
      error('OpenWeather reported an error', 2)
    end
    return result, response
  end
  local function city_query(city, country)
    s.text(city, 'city')
    if country == nil then return {q=city} end
    assert(type(country) == 'string' and country:match('^[A-Za-z][A-Za-z]$'), 'invalid country code')
    return {q=city..','..country}
  end
  local function coordinates(lat, lon)
    return {lat=s.valid_range(lat, -90, 90, 'latitude'), lon=s.valid_range(lon, -180, 180, 'longitude')}
  end
  function api.current(city, country)
    return request('/data/2.5/weather', city_query(city, country))
  end
  function api.forecast(city, country)
    return request('/data/2.5/forecast', city_query(city, country))
  end
  function api.coordinates(lat, lon)
    return request('/data/2.5/weather', coordinates(lat, lon))
  end
  function api.forecast_coordinates(lat, lon)
    return request('/data/2.5/forecast', coordinates(lat, lon))
  end
  return api
end
return M
