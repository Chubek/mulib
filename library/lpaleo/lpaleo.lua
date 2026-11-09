-- Transcription/layout policy lives in Lua; glyph storage and exports are native.
local M={scripts={},null=__mulib_null}
function M.define(glyph) return __mulib_paleo_define(glyph) end
function M.native(name,codepoint,font) return __mulib_paleo_native(name,codepoint,font) end
function M.script(name,profile)
  assert(type(name)=='string' and #name<=63 and type(profile)=='table' and type(profile.mapping)=='table','invalid script profile')
  local direction=profile.direction or 'ltr'
  assert(direction=='ltr' or direction=='rtl' or direction=='ttb' or direction=='boustrophedon','invalid script direction')
  local copy={mapping={},direction=direction,lowercase=profile.lowercase}
  for key,value in pairs(profile.mapping) do
    assert(type(key)=='string' and #key>0 and #key<=64 and type(value)=='string' and #value>0 and #value<=64,'invalid script mapping')
    copy.mapping[key]=value
  end
  M.scripts[name]=copy
end
function M.shape(text,name)
  local profile=assert(M.scripts[name or 'identity'],'unknown paleography script')
  if profile.lowercase then text=string.lower(text) end
  local result,at={},1
  while at<=#text do
    assert(#result<16384,'paleography glyph limit')
    local best,value=nil,nil
    for key,mapped in pairs(profile.mapping) do
      if (not best or #key>#best) and text:sub(at,at+#key-1)==key then best,value=key,mapped end
    end
    if best then result[#result+1]=value; at=at+#best
    else
      local nextbyte=utf8.offset(text,2,at) or (#text+1)
      result[#result+1]=text:sub(at,nextbyte-1);at=nextbyte
    end
  end
  return {glyphs=result,direction=profile.direction}
end
function M.transliterate(text,name)
  return table.concat(M.shape(text,name).glyphs)
end
M.script('identity',{mapping={}})
-- Unicode character mappings, NOT a claim of authentic historical handwriting.
M.script('elder-futhark',{lowercase=true,mapping={
  f='ᚠ',u='ᚢ',th='ᚦ',a='ᚨ',r='ᚱ',k='ᚲ',g='ᚷ',w='ᚹ',h='ᚺ',n='ᚾ',
  i='ᛁ',j='ᛃ',ei='ᛇ',p='ᛈ',z='ᛉ',s='ᛊ',t='ᛏ',b='ᛒ',e='ᛖ',m='ᛗ',l='ᛚ',ng='ᛜ',d='ᛞ',o='ᛟ'
}})
M.script('greek',{lowercase=true,mapping={
  a='α',b='β',g='γ',d='δ',e='ε',z='ζ',th='θ',i='ι',k='κ',l='λ',m='μ',n='ν',
  x='ξ',o='ο',p='π',r='ρ',s='σ',t='τ',u='υ',ph='φ',ch='χ',ps='ψ',w='ω'
}})
return M
