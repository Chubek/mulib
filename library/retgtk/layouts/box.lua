local M = require('retgtk.core')
local function box(p, vertical)
    local w = M.widget('box', p)
    function w:arrange(r, context)
        local gap, pad = self.spacing or 4, self.padding or 0
        local snapshot, children, weight = M.children(self, context), {}, 0
        for i=1,#snapshot do
            local child = snapshot[i]
            if child.visible and child.parent == self and not child._destroyed then
                children[#children+1] = child
                weight = weight + math.max(0, child.weight or 1)
            end
        end
        local n = #children
        if n == 0 then return end
        local space = math.max(0, (vertical and r.h or r.w)-2*pad-(n-1)*gap)
        local t = 0
        for i=1,n do
            local child = children[i]
            local a = math.floor(t*space)
            t = t + (weight > 0 and math.max(0,child.weight or 1)/weight or 1/n)
            local b = i == n and space or math.floor(t*space)
            local q = {x=r.x+pad,y=r.y+pad,w=r.w-2*pad,h=r.h-2*pad}
            if vertical then q.y=q.y+a+(i-1)*gap; q.h=b-a
            else q.x=q.x+a+(i-1)*gap; q.w=b-a end
            M.paint(child, q, context)
        end
    end
    return w
end
return {vbox=function(p) return box(p,true) end, hbox=function(p) return box(p,false) end}
