local M = require('retgtk.core')
return function(p)
    local w = M.widget('table', p)
    function w:paint(r, context)
        local model = self.model
        if not model then return end
        local cols, count = model:columns(), model:count()
        assert(type(cols) == 'number' and cols >= 0 and cols <= 1024 and cols % 1 == 0,
               'invalid model column count')
        assert(type(count) == 'number' and count >= 0 and count <= 9007199254740991 and
               count % 1 == 0, 'invalid model row count')
        if cols == 0 then return end
        local row, first = self.row_height or 24, self.first or 1
        assert(type(row) == 'number' and row >= 1 and row < math.huge, 'invalid row height')
        assert(type(first) == 'number' and first >= 1 and first % 1 == 0, 'invalid first row')
        local rows = math.min(math.floor(r.h/row), math.floor(M.max_nodes/cols))
        for i=first,math.min(count,first+rows-1) do
            for j=1,cols do
                if self._destroyed or not M.attached(self, context) then return end
                local x,y = r.x+(j-1)*r.w/cols, r.y+(i-first)*row
                imm_rect(x,y,r.w/cols-1,row-1,context.theme.normal)
                imm_text(tostring(model:get(i,j) or ''), x+4,y+4,context.theme.text)
            end
        end
    end
    return w
end
