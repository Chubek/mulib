local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.plot_map(v,lo,hi,logarithmic) if hi<=lo then return nil end;if logarithmic then if v<=0 or lo<=0 then return nil end;return math.log(v/lo)/math.log(hi/lo) end;return (v-lo)/(hi-lo) end
return a
