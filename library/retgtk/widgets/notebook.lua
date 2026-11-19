local M = require('retgtk.core')
return function(p)
    local w = M.widget('notebook', p)
    w.tabids = {}
    function w:arrange(r, context)
        local children, tabs = M.children(self, context), {}
        for i=1,#children do
            local child = children[i]
            if child.visible and not child._destroyed and child.parent == self then
                tabs[#tabs+1] = {child=child,index=i}
            end
        end
        local n = #tabs
        if n == 0 then self.tabids = {}; return end
        local selected = self.selected or tabs[1].index
        local found = false
        for i=1,n do if tabs[i].index == selected then found = true end end
        if not found then selected = tabs[1].index end
        local tabids = {}
        for i=1,n do
            if self._destroyed or not M.attached(self, context) then break end
            local tab, x = tabs[i], r.x+(i-1)*r.w/n
            -- Key by child identity, so a removed page cannot inherit its ID.
            local id = self.tabids[tab.child] or M.allocate_id()
            tabids[tab.child] = id
            if tab.child.parent == self and tab.child.visible and not tab.child._destroyed then
                local _, _, clicked = M.interact(self,
                    {x=x,y=r.y,w=r.w/n,h=math.min(24,r.h)}, context, id)
                if clicked then selected=tab.index; self:set('selected', selected) end
                imm_rect(x,r.y,r.w/n,math.min(24,r.h),
                         tab.index == selected and context.theme.active or context.theme.normal)
                imm_text(tab.child.title or tostring(tab.index), x+4,r.y+4,context.theme.text)
            end
        end
        self.tabids = tabids
        local child = children[selected]
        if child then M.paint(child, {x=r.x,y=r.y+24,w=r.w,h=r.h-24}, context) end
    end
    return w
end
