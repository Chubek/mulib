local M = require('retgtk.core')
local box = require('retgtk.layouts.box')
local K = {
    vbox=box.vbox, hbox=box.hbox, grid=require('retgtk.layouts.grid'),
    split=require('retgtk.layouts.split'), stack=require('retgtk.layouts.stack'),
    scroll=require('retgtk.layouts.scroll'),
}
local names = {'button','label','entry','choice','slider','progress','canvas',
               'list','table','notebook','dialog','image'}
for i=1,#names do K[names[i]] = require('retgtk.widgets.'..names[i]) end
K.checkbox=K.choice; K.textview=K.entry; K.toolbar=K.hbox; K.statusbar=K.label
function K.window(p) return M.widget('window', p) end

function K.application(p)
    p = p or {}
    local app = {
        theme=p.theme or require('gui.themes').dark, window=nil, host=p.host,
        frame_count=0, quit_requested=false, max_frames=p.max_frames or 10000,
    }
    function app:set_window(w)
        assert(not w or (M.is_widget(w) and not w.parent),
               'window must be a live root')
        self.window = w
    end
    function app:quit() self.quit_requested = true end
    function app:frame(input)
        assert(not self._framing, 'recursive application frame')
        assert(input == nil or type(input) == 'table', 'frame input must be a table')
        local w = self.window
        if self.quit_requested or not w or not w.visible or w._destroyed then return false end
        self._framing = true
        local ok, err = pcall(function()
            M.paint(w, {x=0,y=0,w=w.width or 800,h=w.height or 600},
                    {theme=self.theme,input=input or {},app=self,root=w})
        end)
        self._framing = false
        if not ok then error(err, 0) end
        self.frame_count = self.frame_count + 1
        return true
    end
    function app:run()
        assert(not self._running and not self._framing, 'recursive application loop')
        -- Preserve the legacy native-host hook. Its scheduling is host-owned.
        if not self.host then
            assert(type(retgtk_run) == 'function',
                   'provide host:next_frame(app), retgtk_run, or call app:frame()')
        else
            assert(type(self.host.next_frame) == 'function', 'missing host:next_frame')
            assert(type(self.max_frames) == 'number' and self.max_frames >= 1 and
                   self.max_frames <= 1000000 and self.max_frames % 1 == 0,
                   'invalid frame limit')
            assert(self.host.present == nil or type(self.host.present) == 'function')
        end
        self._running = true
        local count, reason, legacy = 0, 'closed', nil
        local ok, err = pcall(function()
            if not self.host then
                local function pack(...) return {n=select('#',...),...} end
                legacy = pack(retgtk_run(self))
                return
            end
            while not self.quit_requested and count < self.max_frames do
                -- The host must block/wait as needed: nil is EOF, not "try again".
                local input = self.host:next_frame(self)
                if input == nil then
                    if self.quit_requested then reason = 'quit' end
                    return
                end
                assert(type(input) == 'table', 'host frame must be a table or nil')
                if self.quit_requested then break end
                local painted = self:frame(input)
                count = count + 1
                if painted and self.host.present then self.host:present(self) end
            end
            reason = self.quit_requested and 'quit' or 'limit'
        end)
        self._running = false
        if not ok then error(err, 0) end
        if not self.host then return (table.unpack or unpack)(legacy, 1, legacy.n) end
        return count, reason
    end
    return app
end
K.models = {
    list=require('retgtk.models.list'), table=require('retgtk.models.table'),
    tree=require('retgtk.models.tree'), proxy=require('retgtk.models.proxy'),
}
K.build = function(node) return require('retgtk.templates.builder')(K, node) end
K.bind = require('retgtk.templates.binding')
return K
