return function(a, property, b, target, both)
    local busy, disconnected = false, false
    local function update(src, srcprop, dst, dstprop)
        if busy or disconnected or src._destroyed or dst._destroyed then return end
        busy = true
        local ok, err = pcall(function() dst:set(dstprop, src[srcprop]) end)
        busy = false
        if not ok then error(err, 0) end
    end
    local forward = a:on('changed', function(_, name)
        if name == property then update(a, property, b, target) end
    end)
    local reverse
    if both then
        reverse = b:on('changed', function(_, name)
            if name == target then update(b, target, a, property) end
        end)
    end
    local function disconnect()
        if disconnected then return end
        disconnected = true
        a:off('changed', forward)
        if reverse then b:off('changed', reverse) end
        a:off('destroy', disconnect)
        b:off('destroy', disconnect)
    end
    a:on('destroy', disconnect)
    b:on('destroy', disconnect)
    local ok, err = pcall(function() update(a, property, b, target) end)
    if not ok then disconnect(); error(err, 0) end
    return disconnect
end
