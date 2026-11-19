local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}
local function repo(owner, name)
  return '/repos/'..s.id(owner, 'GitHub owner', '^[A-Za-z0-9][A-Za-z0-9_%-]*$')
    ..'/'..s.id(name, 'GitHub repository', '^[A-Za-z0-9_.%-]+$')
end
function M.new(token, options)
  local opt = s.object(options, 'GitHub options', true)
  if token ~= nil then s.token(token, 'GitHub token') end
  local cli = c.new(s.base(opt.base_url, 'https://api.github.com'), {
    bearer=token, headers={Accept='application/vnd.github+json',
      ['X-GitHub-Api-Version']='2022-11-28', ['User-Agent']='muw3lib/0.1'}})
  local api = {raw=cli}
  function api.repository(owner, name) return cli:get(repo(owner, name)) end
  function api.issues(owner, name, query) return cli:get(repo(owner, name)..'/issues', query) end
  function api.issue(owner, name, id)
    return cli:get(repo(owner, name)..'/issues/'..s.positive_id(id, 'issue number'))
  end
  function api.create_issue(owner, name, payload)
    return cli:post(repo(owner, name)..'/issues', s.object(payload, 'GitHub issue payload'))
  end
  function api.pulls(owner, name, query) return cli:get(repo(owner, name)..'/pulls', query) end
  function api.pull(owner, name, number)
    return cli:get(repo(owner, name)..'/pulls/'..s.positive_id(number, 'pull request number'))
  end
  function api.create_pull(owner, name, payload)
    return cli:post(repo(owner, name)..'/pulls', s.object(payload, 'GitHub pull request payload'))
  end
  function api.contents(owner, name, path, ref)
    local endpoint = repo(owner, name)..'/contents'
    if path ~= nil and path ~= '' then endpoint = endpoint..'/'..s.path(path, 'repository path') end
    return cli:get(endpoint, ref ~= nil and {ref=s.text(ref, 'repository ref')} or nil)
  end
  function api.commits(owner, name, query) return cli:get(repo(owner,name)..'/commits', query) end
  function api.search_repos(query, extra)
    return cli:get('/search/repositories',
      s.extras({q=s.text(query, 'search query')}, extra, 'GitHub search options'))
  end
  return api
end
return M
