local M=require('retgtk.core');return function(p) local w=M.widget('label',p);function w:paint(r,c) M.caption(self,r,c) end;return w end
