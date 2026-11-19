local core = require('muw3lib.core')
local common = require('muw3lib.protocols.common')
local M = {}

local function validate(result)
  assert(common.object(result) and (result.data ~= nil or result.errors ~= nil)
    and (result.data == nil or result.data == core.muw3.null or common.object(result.data))
    and (result.extensions == nil or common.object(result.extensions)), 'invalid GraphQL response')
  if result.errors ~= nil then
    local count = common.array_size(result.errors)
    assert(count and count > 0, 'invalid GraphQL errors')
    for i = 1, count do
      local item = result.errors[i]
      assert(common.object(item) and type(item.message) == 'string', 'invalid GraphQL error')
    end
  end
end

function M.new(endpoint, options)
  local client, path = common.endpoint(endpoint, options)
  local obj = {raw=client}
  function obj.execute(query, variables, operation_name, opts)
    assert(type(query) == 'string' and query:find('%S'), 'GraphQL query required')
    assert(variables == nil or common.object(variables), 'GraphQL variables must be an object')
    assert(operation_name == nil or (type(operation_name) == 'string' and
      operation_name:match('^[_A-Za-z][_A-Za-z0-9]*$')), 'invalid GraphQL operation name')
    local allow_errors = common.flag(opts, 'allow_errors')
    local response = client:request('POST', path, {
      json={query=query, variables=variables, operationName=operation_name},
      headers={Accept='application/graphql-response+json, application/json',
               ['Content-Type']='application/json'},
      allow_error=allow_errors,
    })
    local success = response.status >= 200 and response.status < 300
    local mime, duplicate = common.media_type(response)
    if not success then
      if response.status < 400 or mime ~= 'application/graphql-response+json' then
        error('HTTP ' .. response.status, 2)
      end
    else
      assert(not duplicate and (mime == nil or mime == 'application/graphql-response+json' or
        mime == 'application/json'), 'invalid GraphQL response media type')
    end
    local result
    if response.body ~= '' then
      local ok
      ok, result = pcall(response.json, response)
      assert(ok, 'invalid JSON response')
    end
    validate(result)
    if not success then
      assert(result.errors and (result.data == nil or result.data == core.muw3.null),
        'invalid GraphQL response for non-2xx HTTP status')
    end
    if result.errors and not allow_errors then
      error('GraphQL remote error (use allow_errors to inspect the envelope)', 2)
    end
    return result.data, result, response
  end
  return obj
end

return M
