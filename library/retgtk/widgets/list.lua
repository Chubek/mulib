local M = require('retgtk.core')
return function(p)
    local w = M.widget('list', p)
    w.rowids = {}
    function w:paint(r, context)
        local model, row, first = self.model, self.row_height or 24, self.first or 1
        assert(type(row) == 'number' and row >= 1 and row < math.huge, 'invalid row height')
        assert(type(first) == 'number' and first >= 1 and first % 1 == 0, 'invalid first row')
        local count = model and model:count() or 0
        assert(type(count) == 'number' and count >= 0 and count <= 9007199254740991 and
               count % 1 == 0, 'invalid model count')
        local last = math.min(count, first+math.min(M.max_nodes,math.floor(r.h/row))-1)
        local rowids = {}
        for i=first,last do
            if self._destroyed or not M.attached(self, context) then break end
            local id = self.rowids[i] or M.allocate_id()
            rowids[i] = id
            local y = r.y+(i-first)*row
            local hot, held, click = M.interact(self, {x=r.x,y=y,w=r.w,h=row}, context, id)
            imm_rect(r.x,y,r.w,row,held and context.theme.active or
                     self.selected == i and context.theme.accent or
                     hot and context.theme.hover or context.theme.normal)
            imm_text(tostring(model:get(i)), r.x+4,y+4,context.theme.text)
            if click then self:set('selected', i) end
        end
        self.rowids = rowids -- retain IDs only for the visible viewport
    end
    return w
end
