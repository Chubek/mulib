local g={}
function g.argb(a,r,g,b) return math.floor(a)*16777216+math.floor(r)*65536+math.floor(g)*256+math.floor(b) end
function g.intersect(a,b) local x=math.max(a.x,b.x);local y=math.max(a.y,b.y);return {x=x,y=y,w=math.max(0,math.min(a.x+a.w,b.x+b.w)-x),h=math.max(0,math.min(a.y+a.h,b.y+b.h)-y)} end
return g
