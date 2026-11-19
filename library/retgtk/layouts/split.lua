local M = require('retgtk.core')
return function(p)
    local w = M.widget('split', p)
    function w:arrange(r, context)
        local t = math.max(.05, math.min(.95, self.ratio or .5))
        local children = M.children(self, context)
        if children[1] then
            M.paint(children[1], {x=r.x,y=r.y,w=r.w*t,h=r.h}, context)
        end
        if children[2] then
            M.paint(children[2], {x=r.x+r.w*t,y=r.y,w=r.w*(1-t),h=r.h}, context)
        end
    end
    return w
end
