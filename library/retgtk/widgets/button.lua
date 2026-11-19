local M=require('retgtk.core');return function(p) local w=M.widget('button',p);function w:paint(r,c) if M.control(self,r,c) then self:emit('clicked') end end;return w end
