local M = require('retgtk.core')
return function(p)
    local w = M.widget('grid', p)
    function w:arrange(r, context)
        local cols = self.columns or 1
        assert(type(cols) == 'number' and cols >= 1 and cols <= M.max_nodes and cols % 1 == 0)
        local children = M.children(self, context)
        local rows = math.ceil(#children/cols)
        if rows == 0 then return end
        for i=1,#children do
            local x, y = (i-1)%cols, math.floor((i-1)/cols)
            M.paint(children[i], {x=r.x+x*r.w/cols,y=r.y+y*r.h/rows,
                                 w=r.w/cols,h=r.h/rows}, context)
        end
    end
    return w
end
