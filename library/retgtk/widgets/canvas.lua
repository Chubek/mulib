local M=require('retgtk.core');return function(p) local w=M.widget('canvas',p);function w:paint(r,c) if self.renderer then self.renderer(self,r,c) end end;return w end
