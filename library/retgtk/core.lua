local M = {
    next_id = 2251799813685248,
    max_depth = 128,
    max_nodes = 8192,
    max_listeners = 1024,
    max_signal_depth = 64,
}
local W = {}
W.__index = W
local reserved = {
    id=true, kind=true, parent=true, children=true, listeners=true,
    _destroyed=true, _destroying=true, _emitting=true,
}

function M.allocate_id()
    assert(M.next_id >= 2251799813685248 and
           M.next_id < 4503599627370496 and M.next_id % 1 == 0,
           'retgtk ID range exhausted')
    local id = M.next_id
    M.next_id = id + 1
    return id
end

function M.widget(kind, props)
    props = props or {}
    assert(type(kind) == 'string' and type(props) == 'table')
    for k in pairs(props) do
        assert(not reserved[k] and W[k] == nil, 'reserved widget property')
    end
    local w = setmetatable({
        kind=kind, id=M.allocate_id(), children={}, listeners={}, parent=nil,
        visible=true, enabled=true, weight=1, text='', value=0,
    }, W)
    for k,v in pairs(props) do w[k] = v end
    return w
end

local function live(w)
    return getmetatable(w) == W and not w._destroyed and not w._destroying
end
function M.is_widget(w) return live(w) end

function M.children(w, context)
    if context and context._children then return context._children[w] or {} end
    local result = {}
    for i=1,#w.children do result[i] = w.children[i] end
    return result
end

function W:append(child)
    assert(live(self) and live(child) and not child.parent, 'invalid child')
    local p, depth = self, 0
    while p do
        depth = depth + 1
        assert(p ~= child and depth <= M.max_depth, 'cycle or excessive depth')
        p = p.parent
    end
    -- Validate the whole prospective subtree before changing either owner.
    local count = 0
    local function check(node, level)
        count = count + 1
        assert(level <= M.max_depth and count <= M.max_nodes, 'tree limit')
        for i=1,#node.children do check(node.children[i], level+1) end
    end
    check(child, depth+1)
    assert(#self.children < M.max_nodes, 'child limit')
    self.children[#self.children+1] = child
    child.parent = self
    return child
end

function W:remove(child)
    for i=1,#self.children do
        if self.children[i] == child then
            table.remove(self.children, i)
            child.parent = nil
            return true
        end
    end
    return false
end

function W:set_child(child)
    assert(live(self), 'destroyed widget')
    if child and child.parent == self and #self.children == 1 then return child end
    -- append performs cycle/depth validation before existing children are removed.
    if child and child.parent ~= self then self:append(child) end
    local children = M.children(self)
    for i=1,#children do
        if children[i] ~= child then self:remove(children[i]) end
    end
    return child
end

function W:on(name, fn)
    assert(live(self) and type(name) == 'string' and #name > 0 and
           #name <= 128 and type(fn) == 'function')
    local total = 0
    for _,entries in pairs(self.listeners) do total = total + #entries end
    assert(total < M.max_listeners, 'listener limit')
    local listeners = self.listeners[name] or {}
    self.listeners[name] = listeners
    listeners[#listeners+1] = {fn=fn, active=true}
    return fn -- legacy disconnect token
end

function W:off(name, fn)
    local listeners = self.listeners[name]
    if not listeners then return false end
    local found = false
    for i=#listeners,1,-1 do
        if listeners[i].fn == fn then
            listeners[i].active = false
            table.remove(listeners, i)
            found = true
        end
    end
    if #listeners == 0 then self.listeners[name] = nil end
    return found
end

function W:emit(name, ...)
    if self._destroyed then return false end
    local depth = self._emitting or 0
    assert(depth < M.max_signal_depth, 'signal recursion limit')
    local source, snapshot = self.listeners[name] or {}, {}
    for i=1,#source do snapshot[i] = source[i] end
    local args, argc = {...}, select('#', ...)
    local unpack_args = table.unpack or unpack
    self._emitting = depth + 1
    local ok, err = pcall(function()
        local destroy_error, destroy_failed
        for i=1,#snapshot do
            local entry = snapshot[i]
            if self._destroyed then break end
            if entry.active then
                if name == 'destroy' then
                    local ran, failure = pcall(entry.fn, self, unpack_args(args, 1, argc))
                    if not ran and not destroy_failed then
                        destroy_error, destroy_failed = failure, true
                    end
                else entry.fn(self, unpack_args(args, 1, argc)) end
            end
        end
        if destroy_failed then error(destroy_error, 0) end
    end)
    self._emitting = depth
    if not ok then error(err, 0) end
    return true
end

function W:set(name, value)
    assert(live(self) and type(name) == 'string' and
           not reserved[name] and W[name] == nil, 'invalid widget property')
    if self[name] ~= value then
        self[name] = value
        self:emit('changed', name, value)
    end
end

function W:show() assert(live(self)); self.visible = true end
function W:hide() assert(live(self)); self.visible = false end

function W:destroy()
    if self._destroyed or self._destroying then return false end
    self._destroying = true
    if self.parent then self.parent:remove(self) end
    local children = M.children(self)
    -- Always finish teardown even if a destroy listener throws.
    local ok, err = pcall(function() self:emit('destroy') end)
    self._destroyed = true
    for _,listeners in pairs(self.listeners) do
        for i=1,#listeners do listeners[i].active = false end
    end
    self.listeners = {}
    for i=1,#children do
        if children[i].parent == self then
            self:remove(children[i])
            local child_ok, child_err = pcall(function() children[i]:destroy() end)
            if ok and not child_ok then ok, err = false, child_err end
        end
    end
    self._destroying = false
    if not ok then error(err, 0) end
    return true
end

function M.attached(w, context)
    if context and context._parents then
        if context.app and context.app.window ~= context.root then return false end
        local p = w
        for _=1,M.max_depth do
            if not p or not context._children[p] or p.parent ~= context._parents[p] then
                return false
            end
            if p == context._root then return true end
            p = p.parent
        end
        return false
    end
    return true
end

function M.can_interact(w, context)
    local depth = 0
    if not M.attached(w, context) then return false end
    while w do
        depth = depth + 1
        if depth > M.max_depth or not live(w) or not w.visible or not w.enabled then
            return false
        end
        w = w.parent
    end
    return true
end

local function finite(x)
    return type(x) == 'number' and x == x and x > -math.huge and x < math.huge
end

function M.valid_rect(r)
    return type(r) == 'table' and finite(r.x) and finite(r.y) and
           finite(r.w) and finite(r.h) and finite(r.x+r.w) and finite(r.y+r.h)
end

local function intersect(a, b)
    local x, y = math.max(a.x,b.x), math.max(a.y,b.y)
    return {x=x,y=y,w=math.max(0,math.min(a.x+a.w,b.x+b.w)-x),
                    h=math.max(0,math.min(a.y+a.h,b.y+b.h)-y)}
end

function M.prepare(root, context)
    local count = 0
    context._root = root
    context._children, context._parents = {}, {}
    local function visit(w, depth)
        count = count + 1
        assert(live(w) and depth <= M.max_depth and count <= M.max_nodes, 'tree limit')
        assert(not context._children[w], 'duplicate widget or cycle')
        local children = M.children(w)
        context._children[w], context._parents[w] = children, w.parent
        for i=1,#children do
            assert(children[i].parent == w, 'inconsistent tree')
            visit(children[i], depth+1)
        end
    end
    visit(root, 1)
end

function M.paint(w, r, context)
    assert(M.valid_rect(r), 'invalid widget bounds')
    if not context._painting or context._painting == 0 then M.prepare(w, context) end
    if not M.attached(w, context) then return end
    local ancestor, depth = w, 0
    while ancestor do
        depth = depth + 1
        if depth > M.max_depth or not live(ancestor) or not ancestor.visible then return end
        ancestor = ancestor.parent
    end
    if r.w <= 0 or r.h <= 0 then return end
    local old_clip = context._clip
    local clip = old_clip and intersect(r, old_clip) or r
    if clip.w <= 0 or clip.h <= 0 then return end
    local painting = context._painting or 0
    assert(painting < M.max_depth, 'paint recursion limit')
    context._painting = painting + 1
    w.bounds = {x=r.x,y=r.y,w=r.w,h=r.h}
    context._clip = clip
    local pushed = type(imm_clip_push) == 'function' and type(imm_clip_pop) == 'function'
    local did_push = false
    local ok, err = pcall(function()
        if pushed then imm_clip_push(clip.x,clip.y,clip.w,clip.h); did_push = true end
        if w.paint then w:paint(r, context)
        elseif w.kind == 'window' then imm_rect(r.x,r.y,r.w,r.h,context.theme.background) end
        if not live(w) or not w.visible or not M.attached(w, context) then return end
        if w.arrange then w:arrange(r, context)
        else
            local children = M.children(w, context)
            for i=1,#children do M.paint(children[i], r, context) end
        end
    end)
    context._clip = old_clip
    context._painting = painting
    if did_push then
        local pop_ok, pop_err = pcall(imm_clip_pop)
        if ok and not pop_ok then ok, err = false, pop_err end
    end
    if not ok then error(err, 0) end
end

function M.interact(w, r, context, id)
    if not M.can_interact(w, context) then return false,false,false,false end
    assert(M.valid_rect(r), 'invalid interaction bounds')
    if context and context._clip then r = intersect(r, context._clip) end
    if r.w <= 0 or r.h <= 0 then return false,false,false,false end
    return imm_interact(id or w.id, r.x,r.y,r.w,r.h)
end

function M.caption(w, r, context)
    imm_text(tostring(w.text), r.x+4,r.y+4,context.theme.text)
end

function M.control(w, r, context)
    local hot, held, clicked, focus = M.interact(w, r, context)
    local theme = context.theme
    imm_rect(r.x,r.y,r.w,r.h,held and theme.active or hot and theme.hover or theme.normal)
    M.caption(w, r, context)
    return clicked, focus, held
end
return M
