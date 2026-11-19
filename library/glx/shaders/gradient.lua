return function(x,y,a,b) local t=math.max(0,math.min(1,x));return a[1]+(b[1]-a[1])*t,a[2]+(b[2]-a[2])*t,a[3]+(b[3]-a[3])*t,a[4]+(b[4]-a[4])*t end
