local ui = require('immgtk.core')
local a = require('immgtk.advanced')
require('immgtk.containers.window')
require('immgtk.controls.drag_float')
require('immgtk.editors.timeline')
function a.demo(state, input)
  return a.frame(input, function()
    a.begin_window('demo', 'Workbench', state.bounds)
    state.gain = a.drag_float('gain', state.gain, 0, 1, ui.next())
    state.time = a.timeline('time', state.time, 0, 60, ui.next())
    a.end_window()
    return state
  end)
end
return a
