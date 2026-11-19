local ui=require('immgtk.core')
function ui.image(pixels,cols,rows,x,y,w,h)
  ui._index(cols,256);ui._index(rows,256)
  local count=cols*rows
  assert(type(pixels)=='table' and #pixels==count and count<=4096,'image pixel count')
  local colors={}
  for i=1,count do
    local c=ui._number(pixels[i])
    assert(c>=0 and c<=0xffffffff and c==math.floor(c),'invalid image color')
    colors[i]=c
  end
  x,y,w,h=ui._box(x,y,w,h)
  for j=1,rows do for i=1,cols do
    imm_rect(x+(i-1)*w/cols,y+(j-1)*h/rows,w/cols,h/rows,colors[(j-1)*cols+i])
  end end
end
return ui
