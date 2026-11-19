local M = require('retgtk.core')
return function(kit, node)
    local active, count = {}, 0
    local function build(description, depth)
        count = count + 1
        assert(depth <= M.max_depth and count <= M.max_nodes, 'builder limit')
        assert(type(description) == 'table' and not active[description], 'builder cycle')
        assert(type(kit[description.type]) == 'function', 'unknown widget type')
        local children = description.children or {}
        assert(type(children) == 'table', 'invalid builder children')
        active[description] = true
        local w = kit[description.type](description.props or {})
        local ok, err = pcall(function()
            for i=1,#children do
                local child = build(children[i], depth+1)
                local appended, failure = pcall(function() w:append(child) end)
                if not appended then pcall(function() child:destroy() end); error(failure, 0) end
            end
        end)
        active[description] = nil
        if not ok then pcall(function() w:destroy() end); error(err, 0) end
        return w
    end
    return build(node, 1)
end
