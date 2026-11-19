local M = require('retgtk.core')
return function(p)
    local w = M.widget('stack', p)
    function w:arrange(r, context)
        local child = M.children(self, context)[self.selected or 1]
        if child then M.paint(child, r, context) end
    end
    return w
end
