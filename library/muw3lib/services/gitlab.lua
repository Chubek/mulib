local c = require('muw3lib.core')
local s = require('muw3lib.services._util')
local M = {}
local function project(id)
  local path
  if type(id) == 'number' or (type(id) == 'string' and id:match('^[0-9]+$')) then
    path = s.positive_id(id, 'GitLab project ID')
  else
    assert(type(id) == 'string' and id:find('/', 1, true), 'invalid GitLab project ID')
    path = s.path(id, 'GitLab project path', '^[A-Za-z0-9_.%-]+$', true)
  end
  return '/api/v4/projects/'..path
end
function M.new(token, options)
  s.token(token, 'GitLab token')
  local opt=s.object(options, 'GitLab options', true)
  assert(opt.oauth == nil or type(opt.oauth) == 'boolean', 'invalid GitLab OAuth option')
  local headers=opt.oauth and {Authorization='Bearer '..token} or {['PRIVATE-TOKEN']=token}
  local cli=c.new(s.base(opt.base_url, 'https://gitlab.com'), {headers=headers})
  local api={raw=cli}
  function api.project(id) return cli:get(project(id)) end
  function api.projects(query) return cli:get('/api/v4/projects', query) end
  function api.issues(id, query) return cli:get(project(id)..'/issues', query) end
  function api.issue(id, iid)
    return cli:get(project(id)..'/issues/'..s.positive_id(iid, 'GitLab issue IID'))
  end
  function api.create_issue(id, payload)
    return cli:post(project(id)..'/issues', s.object(payload, 'GitLab issue payload'))
  end
  function api.merge_requests(id, query) return cli:get(project(id)..'/merge_requests', query) end
  function api.merge_request(id, iid)
    return cli:get(project(id)..'/merge_requests/'..s.positive_id(iid, 'GitLab merge request IID'))
  end
  function api.create_merge_request(id, payload)
    return cli:post(project(id)..'/merge_requests', s.object(payload, 'GitLab merge request payload'))
  end
  function api.pipelines(id, query) return cli:get(project(id)..'/pipelines', query) end
  function api.pipeline(id, pipeline_id)
    return cli:get(project(id)..'/pipelines/'..s.positive_id(pipeline_id, 'GitLab pipeline ID'))
  end
  return api
end
return M
