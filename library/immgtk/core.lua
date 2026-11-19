-- Compatible core from MuLIB; extensions use no implicit globals.
local ui={normal=0xff343b48,hover=0xff46556a,active=0xff2465a8,text=0xffeeeeee,background=0xff202630,accent=0xff56b4e9}
function ui._number(v) assert(type(v)=='number' and v==v and v>-math.huge and v<math.huge,'finite number required'); return v end
function ui._rect(x,y,w,h) ui._number(x);ui._number(y);ui._number(w);ui._number(h);assert(math.abs(x)<=1000000 and math.abs(y)<=1000000 and w>=0 and h>=0 and w<=1000000 and h<=1000000,'invalid widget geometry');return x,y,w,h end
function ui._box(x,y,w,h) if x==nil then x,y,w,h=ui.next() end;ui._rect(x,y,w,h);assert(w>0 and h>0,'positive widget size required');return x,y,w,h end
function ui._string(t) assert(type(t)=='string','text must be a string');return t end
function ui._clamp(v,lo,hi) return math.max(lo,math.min(hi,v)) end
function ui._range(v,lo,hi) ui._number(v);ui._number(lo);ui._number(hi);ui._number(hi-lo);assert(hi>lo and hi-lo>0,'increasing, representable range required') end
function ui._color(hot,held,selected) return held and ui.active or (hot and ui.hover or (selected and ui.accent or ui.normal)) end
function ui._caption(t,x,y) imm_text(ui._string(t),x+4,y+4,ui.text) end
function ui._items(a) assert(type(a)=='table' and #a>0 and #a<=256,'1..256 items required');for i=1,#a do ui._string(a[i]) end end
function ui._index(v,n) ui._number(v);assert(v==math.floor(v) and v>=1 and v<=n,'selection out of range') end
function ui.layout(x,y,w,row,gap) row=row or 24;gap=gap or 4;ui._box(x,y,w,row);ui._number(gap);assert(gap>=0 and gap<=1000000,'invalid layout gap');ui.lx=x;ui.ly=y;ui.lw=w;ui.lr=row;ui.lg=gap end
function ui.next() assert(ui.lx~=nil,'call ui.layout first');local x,y,w,h=ui._rect(ui.lx,ui.ly,ui.lw,ui.lr);ui.ly=y+h+ui.lg;return x,y,w,h end
return ui
