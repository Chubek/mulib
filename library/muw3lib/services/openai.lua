local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}
function M.new(api_key, options)
  s.token(api_key, 'OpenAI API key')
  local opt=s.object(options, 'OpenAI options', true)
  local headers={}
  if opt.organization ~= nil then
    headers['OpenAI-Organization']=s.token(opt.organization, 'OpenAI organization')
  end
  if opt.project ~= nil then headers['OpenAI-Project']=s.token(opt.project, 'OpenAI project') end
  local cli=c.new(s.base(opt.base_url, 'https://api.openai.com'), {bearer=api_key, headers=headers})
  local api={raw=cli}
  local function post(path, payload)
    local body=s.object(payload, 'OpenAI request payload')
    assert(body.stream == nil or body.stream == false, 'OpenAI service methods do not stream')
    return cli:post(path, body)
  end
  function api.responses(payload) return post('/v1/responses', payload) end
  function api.response(id)
    return cli:get('/v1/responses/'..c.encode(s.id(id, 'OpenAI response ID')))
  end
  function api.embeddings(payload) return post('/v1/embeddings', payload) end
  function api.models() return cli:get('/v1/models') end
  function api.model(id) return cli:get('/v1/models/'..c.encode(s.id(id, 'OpenAI model ID'))) end
  function api.chat_completions(payload) return post('/v1/chat/completions', payload) end
  return api
end
return M
