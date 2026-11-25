if exists('b:current_syntax')
  finish
endif

syntax case match
syntax iskeyword 48-57,65-90,95,97-122,45
syntax match on1xfileValue /\<\%(executable\|complex\|library\|local\|git\|git-remote\|commit\|branch\|tag\|format\|plain\|latex\|troff\|shell\|off\|standard\|strict\|c11\|mir-c11\|mir-vm\|mir-archon\|default\|debug\|release\|on\)\>/
 syntax match on1xfileDirective /^\s*\zs\%(on1xfile\|kind\|name\|directory\|target\|dependency\|profile\|command\|define\|kernel-path\|kernel\|scrutiny\|scrutiny-work\|backend\|optimization-profile\|optimization\|source\|library\|resource\|interface\)\>/
syntax match on1xfileNumber /\<[0-9]\+\>/
syntax match on1xfileComment /#.*/
syntax match on1xfileBadEscape /\\./ contained
syntax match on1xfileEscape /\\\%(["\\\/bfnrt]\|u[0-9A-Fa-f]\{4}\)/ contained
syntax region on1xfileString start=/"/ skip=/\\./ end=/"/ oneline contains=on1xfileBadEscape,on1xfileEscape

highlight default link on1xfileDirective Keyword
highlight default link on1xfileValue Constant
highlight default link on1xfileNumber Number
highlight default link on1xfileComment Comment
highlight default link on1xfileString String
highlight default link on1xfileBadEscape Error
highlight default link on1xfileEscape SpecialChar

let b:current_syntax = 'on1xfile'
