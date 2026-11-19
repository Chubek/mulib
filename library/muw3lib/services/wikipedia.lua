local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}
function M.new(options)
  local opt=s.object(options, 'Wikipedia options', true)
  local language=opt.language
  if language == nil then language='en' end
  assert(type(language) == 'string' and #language <= 63 and language:match('^[a-z][a-z0-9%-]*$')
    and language:sub(-1) ~= '-', 'invalid Wikipedia language code')
  local base=s.base(opt.base_url, 'https://'..language..'.wikipedia.org')
  local agent=opt.user_agent
  if agent == nil then agent='muw3lib/0.1 (educational client)' end
  local cli=c.new(base, {headers={['User-Agent']=s.header(agent, 'Wikipedia User-Agent')}})
  local api={raw=cli}
  local function action(query)
    local r, response=cli:get('/w/api.php', query)
    assert(type(r) == 'table' and r ~= c.muw3.null, 'invalid MediaWiki response')
    if r.error ~= nil then error('MediaWiki reported an API error', 2) end
    return r, response
  end
  function api.search(query, limit)
    if limit == nil then limit=10 end
    local n=tonumber(s.positive_id(limit, 'Wikipedia search limit'))
    s.valid_range(n, 1, 500, 'Wikipedia search limit')
    return action({action='query', list='search', srsearch=s.text(query, 'Wikipedia search term'),
      srlimit=n, format='json'})
  end
  function api.page(title)
    return action({action='query', prop='extracts', exintro='1',
      explaintext='1', titles=s.text(title, 'Wikipedia page title'), format='json'})
  end
  function api.summary(title)
    return cli:get('/api/rest_v1/page/summary/'..s.path(title, 'Wikipedia page title', nil, true))
  end
  return api
end
return M
