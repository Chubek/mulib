local core = require('muw3lib.core')
local Client = setmetatable({}, {__index=core.Client})
Client.__index = Client
local M = {}

function M.new(base, options)
  return setmetatable(core.new(base, options), Client)
end

function Client:put(path, payload)
  return self:json('PUT', path, {json=payload})
end

function Client:patch(path, payload)
  return self:json('PATCH', path, {json=payload})
end

function Client:delete(path, query)
  return self:json('DELETE', path, {query=query})
end

function Client:head(path, query)
  return self:request('HEAD', path, {query=query})
end

function Client:options(path, query)
  return self:request('OPTIONS', path, {query=query})
end

return M
