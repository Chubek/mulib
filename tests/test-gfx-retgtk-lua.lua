-- Run from the repository root with stock Lua 5.4 or LuaJIT. Mock primitives
-- exercise host contracts, not a Mune/native binding certification.
package.path = './library/?.lua;./library/?/init.lua;' .. package.path
local K = require('retgtk')
local M = require('retgtk.core')
local hits, draws, clips, policy = {}, {}, 0, nil
function imm_rect(x,y,w,h,color) draws[#draws+1] = {x,y,w,h,color} end
function imm_text(text,x,y,color) assert(type(text) == 'string') end
function imm_pointer() return 10,10,false end
function imm_interact(id,x,y,w,h)
    hits[#hits+1] = {id=id,x=x,y=y,w=w,h=h}
    if policy then return policy(id,x,y,w,h) end
    return false,false,false,false
end
function imm_clip_push() clips = clips+1 end
function imm_clip_pop() assert(clips > 0); clips = clips-1 end
local function reset() hits={}; draws={}; policy=nil; assert(clips == 0) end
local function fails(fn)
    local ok = pcall(fn)
    assert(not ok, 'expected failure')
    assert(clips == 0, 'leaked native clip')
end
local function application(root)
    local app = K.application()
    app:set_window(root)
    return app
end

-- Emission snapshots: removed listeners are skipped, additions are deferred;
-- duplicate callbacks and recursive emission cannot invalidate an iterator.
do
    local w, seen = M.widget('test'), {}
    local late = function() seen[#seen+1]='late' end
    local second
    w:on('signal', function(self,a,b)
        assert(a == 7 and b == nil)
        seen[#seen+1]='first'
        self:off('signal', second)
        self:on('signal', late)
    end)
    second = w:on('signal', function() seen[#seen+1]='second' end)
    w:emit('signal', 7, nil)
    assert(table.concat(seen, ',') == 'first')
    seen={}
    w:emit('signal', 7, nil)
    assert(table.concat(seen, ',') == 'first,late')
    assert(w:off('signal', late))
    assert(not w:off('signal', late))
    local nested = 0
    w:on('nested', function(self,n)
        nested=nested+1
        if n>0 then self:emit('nested', n-1) end
    end)
    w:emit('nested', 2)
    assert(nested == 3 and w._emitting == 0)
    w:on('bad', function() error('listener error') end)
    fails(function() w:emit('bad') end)
    assert(w._emitting == 0)
    w:on('recursive', function(self) self:emit('recursive') end)
    fails(function() w:emit('recursive') end)
    assert(w._emitting == 0)
    local ran = false
    w:on('destroy-self', function(self) self:destroy() end)
    w:on('destroy-self', function() ran=true end)
    w:emit('destroy-self')
    assert(w._destroyed and not ran and not w:destroy())
end

-- Tree updates validate before mutation and destroy recursively/idempotently.
do
    local root, a, b = K.window(), K.vbox(), K.button()
    root:append(a); a:append(b)
    fails(function() b:append(root) end)
    assert(root.children[1] == a and a.children[1] == b and not root.parent)
    fails(function() a:set_child(root) end)
    assert(a.children[1] == b and b.parent == a)
    a:set_child(b)
    local destroyed = 0
    b:on('destroy', function() destroyed=destroyed+1; error('teardown error') end)
    b:on('destroy', function() destroyed=destroyed+1 end)
    fails(function() root:destroy() end)
    assert(root._destroyed and a._destroyed and b._destroyed and destroyed == 2)
    assert(#root.children == 0 and #a.children == 0 and not b.parent)
    assert(not root:destroy())
    fails(function() b:on('clicked', function() end) end)
    fails(function() M.widget('test', {id=42}) end)
    local nil_error = M.widget('test')
    nil_error:on('destroy', function() error(nil) end)
    fails(function() nil_error:destroy() end)
    assert(nil_error._destroyed)
    local old_limit = M.max_listeners
    M.max_listeners = 2
    local limited = M.widget('test')
    limited:on('a', function() end); limited:on('b', function() end)
    fails(function() limited:on('c', function() end) end)
    M.max_listeners = old_limit
    local next_id = M.next_id
    M.next_id = 4503599627370496
    fails(function() M.widget('test') end)
    M.next_id = next_id
    local node={type='window',children={}}
    node.children[1]=node
    fails(function() K.build(node) end)
    local built=K.build({type='window',children={{type='label',props={text='built'}}}})
    assert(built.children[1].text == 'built')
end

-- Hidden/disabled ancestors suppress every control, including list rows/tabs
-- which previously bypassed the shared interaction helper.
do
    reset()
    local root = K.vbox({enabled=false})
    local button, entry, choice = K.button(), K.entry({text='old'}), K.checkbox()
    local list = K.list({model=K.models.list({'one','two'})})
    local notebook = K.notebook()
    notebook:append(K.label({title='Tab'}))
    root:append(button); root:append(entry); root:append(choice)
    root:append(list); root:append(notebook)
    local clicks = 0
    button:on('clicked', function() clicks=clicks+1 end)
    policy=function() return true,true,true,true end
    local app = application(root)
    assert(app:frame({text='changed',scroll=1}))
    assert(#hits == 0 and clicks == 0 and entry.text == 'old')
    assert(not choice.checked and not list.selected and not notebook.selected)
    root.enabled=true; root.visible=false
    reset(); assert(not app:frame({text='hidden'}))
    assert(#hits == 0 and #draws == 0)
    root.visible=true; button.visible=false; entry.enabled=false
    policy=function() return false,false,false,false end
    assert(app:frame({text='ignored'}))
    for i=1,#hits do assert(hits[i].id ~= button.id and hits[i].id ~= entry.id) end
    assert(entry.text == 'old' and clips == 0)
end

-- Whole-frame child snapshots: remove a sibling without skipping the next;
-- append a new widget only for the following frame.
do
    reset()
    local root, order = K.vbox(), {}
    local added = K.canvas({renderer=function() order[#order+1]='added' end})
    local victim = K.canvas({renderer=function() order[#order+1]='victim' end})
    local last = K.canvas({renderer=function() order[#order+1]='last' end})
    local first = K.canvas({renderer=function()
        order[#order+1]='first'
        if victim.parent then root:remove(victim); root:append(added) end
    end})
    root:append(first); root:append(victim); root:append(last)
    local app = application(root)
    app:frame()
    assert(table.concat(order, ',') == 'first,last')
    order={}; app:frame()
    assert(table.concat(order, ',') == 'first,last,added')
    assert(clips == 0)
    -- Reusing a direct-paint context still starts a new frame snapshot.
    local context={theme=app.theme,input={}}
    M.paint(root,{x=0,y=0,w=800,h=600},context)
    local extra=K.label()
    root:append(extra)
    M.paint(root,{x=0,y=0,w=800,h=600},context)
    assert(extra.bounds and context._painting == 0)
end

-- Removing the currently painting ancestor invalidates all of its descendants.
do
    reset()
    local root, subtree, button = K.window(), K.vbox(), K.button()
    root:append(subtree); subtree:append(button)
    subtree.paint=function(self) root:remove(self) end
    application(root):frame()
    assert(#hits == 0)
    reset()
    root=K.window(); button=K.button()
    local app=application(root)
    root:append(button)
    root.paint=function() app:set_window(K.window()) end
    app:frame()
    assert(#hits == 0)
end

-- Model mutations during list selection keep the borrowed model alive and
-- cannot dispatch more interactions after destruction.
do
    reset()
    local list=K.list({model=K.models.list({'a','b','c'})})
    list:on('changed', function(self,name)
        if name == 'selected' then self.model=nil; self:destroy() end
    end)
    policy=function() return false,false,true,false end
    application(list):frame()
    assert(list._destroyed and #hits == 1)
    local invalid = K.table({model=K.models.table({{'a'}},1),row_height=0})
    fails(function() application(invalid):frame() end)
    local cells = 0
    local bounded = K.table({height=100000,width=100,model={
        count=function() return 1000000 end,
        columns=function() return 1024 end,
        get=function() cells=cells+1; return 'x' end,
    }})
    application(bounded):frame()
    assert(cells == M.max_nodes)
end

-- Bounds clipping also applies to hits when drawing clip hooks are absent.
do
    reset()
    local scroll = K.scroll({content_height=1000,offset=500,width=100,height=100})
    scroll:append(K.button())
    application(scroll):frame()
    assert(#hits == 1 and hits[1].y == 0 and hits[1].h == 100)
    local push,pop=imm_clip_push,imm_clip_pop
    imm_clip_push=nil; imm_clip_pop=nil
    reset(); application(scroll):frame()
    assert(#hits == 1 and hits[1].h == 100)
    imm_clip_push=push; imm_clip_pop=pop
end

-- Error paths always restore clip scopes/frame guards and allow another frame.
do
    reset()
    local canvas=K.canvas({renderer=function() error('paint error') end})
    local root=K.window(); root:append(canvas)
    local app=application(root)
    fails(function() app:frame() end)
    assert(not app._framing)
    canvas.renderer=function() end
    assert(app:frame())
    canvas.renderer=function() app:frame() end
    fails(function() app:frame() end)
    canvas.renderer=function() end
    assert(app:frame())
end

-- Two-way binding unwinds its recursion guard after callback errors and
-- disconnects explicitly or when either endpoint is destroyed.
do
    local a,b=K.entry({text='one'}),K.entry()
    local disconnect=K.bind(a,'text',b,'text',true)
    assert(b.text == 'one')
    b:set('text','two'); assert(a.text == 'two')
    local bad=b:on('changed',function() error('bound listener error') end)
    fails(function() a:set('text','three') end)
    b:off('changed',bad)
    a:set('text','four'); assert(b.text == 'four')
    disconnect(); disconnect()
    a:set('text','five'); assert(b.text == 'four')
    K.bind(a,'text',b,'text',true)
    a:destroy()
    b:set('text','survives')
    assert(a.text == 'five')
end

-- A bounded host loop consumes one explicit input per tick, presents only
-- successful frames, and never polls again after EOF/quit/the configured cap.
do
    reset()
    local host={polls=0,presents=0}
    function host:next_frame()
        self.polls=self.polls+1
        if self.polls <= 3 then return {} end
    end
    function host:present() self.presents=self.presents+1 end
    local app=K.application({host=host,max_frames=10})
    app:set_window(K.window())
    local count,reason=app:run()
    assert(count == 3 and reason == 'closed' and host.polls == 4 and host.presents == 3)
    assert(app.frame_count == 3 and not app._running)
    function host:next_frame() self.polls=self.polls+1; return {} end
    app.max_frames=2
    count,reason=app:run()
    assert(count == 2 and reason == 'limit')
    function host:present(current) self.presents=self.presents+1; current:quit() end
    count,reason=app:run()
    assert(count == 1 and reason == 'quit')
    count,reason=app:run()
    assert(count == 0 and reason == 'quit')
    local failing=K.application({host={next_frame=function() return false end}})
    fails(function() failing:run() end); assert(not failing._running)
    fails(function() failing:set_window({}) end)
    failing.host.next_frame=function(current,application) application:quit() end
    count,reason=failing:run()
    assert(count == 0 and reason == 'quit')
    local legacy=K.application()
    retgtk_run=function(current) assert(current == legacy); return 'legacy',nil,3 end
    local a,b,c=legacy:run()
    assert(a == 'legacy' and b == nil and c == 3); retgtk_run=nil
end
print('gfx RetGTK Lua mutation and host contracts: ok')
