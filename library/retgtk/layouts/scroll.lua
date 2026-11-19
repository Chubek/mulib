local M = require('retgtk.core')
return function(p)
    local w = M.widget('scroll', p)
    function w:arrange(r, context)
        local h = self.content_height or r.h
        self.offset = math.max(0, math.min(math.max(0,h-r.h), self.offset or 0))
        if M.can_interact(self, context) then
            local x,y = imm_pointer()
            if x >= r.x and x < r.x+r.w and y >= r.y and y < r.y+r.h then
                self.offset = math.max(0, math.min(math.max(0,h-r.h),
                    self.offset-(context.input.scroll or 0)*24))
            end
        end
        local children = M.children(self, context)
        for i=1,#children do
            M.paint(children[i], {x=r.x,y=r.y-self.offset,w=r.w,h=h}, context)
        end
    end
    return w
end
