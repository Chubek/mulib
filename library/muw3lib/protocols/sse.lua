-- Complete buffered SSE bodies only; no incremental delivery or reconnects.
local common = require('muw3lib.protocols.common')
local M = {}

local function limit(value, default, name)
  if value == nil then return default end
  assert(type(value) == 'number' and value >= 0 and value <= 9007199254740991 and value % 1 == 0,
    name .. ' must be a non-negative safe integer')
  return value
end

local function limits(options)
  if options == nil then options = {} end
  assert(common.object(options), 'SSE options must be a table')
  return limit(options.max_body_bytes, 1048576, 'max_body_bytes'),
    limit(options.max_events, 4096, 'max_events')
end

local function parse(body, max_bytes, max_events)
  assert(type(body) == 'string', 'SSE body must be a string')
  assert(#body <= max_bytes, 'SSE body exceeds max_body_bytes')
  assert(utf8.len(body), 'SSE body must be valid UTF-8')
  local events, state, data = {}, {}, {}
  local event_count, data_count = 0, 0
  local kind, last_id, retry
  local function emit()
    state.last_event_id = last_id
    if data_count > 0 then
      assert(event_count < max_events, 'SSE event count exceeds max_events')
      event_count = event_count + 1
      events[event_count] = {data=table.concat(data, '\n'),
        event=(kind and kind ~= '' and kind or 'message'), id=last_id, retry=retry}
      data, data_count = {}, 0
    end
    kind, retry = nil, nil
  end

  -- Strip exactly one leading UTF-8 BOM; payload bytes otherwise pass through
  -- unchanged. Unlike a browser decoder, invalid UTF-8 is rejected above.
  local pos = body:sub(1, 3) == '\239\187\191' and 4 or 1
  while pos <= #body do
    local stop = body:find('[\r\n]', pos)
    if not stop then break end
    local line = body:sub(pos, stop - 1)
    pos = stop + 1
    if body:byte(stop) == 13 and body:byte(pos) == 10 then pos = pos + 1 end
    if line == '' then
      emit()
    elseif line:sub(1, 1) ~= ':' then
      local colon = line:find(':', 1, true)
      local key, value = line, ''
      if colon then
        key = line:sub(1, colon - 1)
        local start = colon + 1
        if line:byte(start) == 32 then start = start + 1 end
        value = line:sub(start)
      end
      if key == 'data' then
        data_count = data_count + 1
        data[data_count] = value
      elseif key == 'id' and not value:find('\0', 1, true) then last_id = value
      elseif key == 'event' then kind = value
      elseif key == 'retry' and value:match('^[0-9]+$') then
        -- Ignore values too large for exact arithmetic across Lua/Mune.
        -- Strip leading zeros before tonumber so long padded values work.
        local digits = value:gsub('^0+', '')
        if #digits <= 16 then
          local delay = digits == '' and 0 or tonumber(digits)
          if delay and delay <= 9007199254740991 then
            retry, state.retry = delay, delay
          end
        end
      end
    end
  end
  -- EOF never dispatches a pending event; a real empty line is required.
  return events, state
end

function M.parse(body, options)
  local max_bytes, max_events = limits(options)
  return parse(body, max_bytes, max_events)
end

function M.fetch(url, headers, options)
  local max_bytes, max_events = limits(options)
  local client, path = common.endpoint(url, {headers=headers})
  local response = client:request('GET', path, {
    headers={Accept='text/event-stream', ['Accept-Encoding']='identity'},
    max_body_bytes=max_bytes,
  })
  local encoding, duplicate = common.singleton_header(response, 'content-encoding')
  assert(not duplicate and (encoding == nil or encoding:match('^[ \t]*(.-)[ \t]*$'):lower() == 'identity'),
    'SSE requires identity Content-Encoding')
  if response.status == 204 then return {}, response, {stopped=true} end
  assert(response.status == 200, 'SSE requires HTTP 200 or 204')
  assert(common.media_type(response) == 'text/event-stream',
    'SSE requires a text/event-stream response')
  local events, state = parse(response.body, max_bytes, max_events)
  return events, response, state
end

return M
