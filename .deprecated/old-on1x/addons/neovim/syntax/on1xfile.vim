if exists('b:current_syntax')
  finish
endif

syntax case match
syntax iskeyword 48-57,65-90,95,97-122,45
" The directive heads come from tools/on1x-project.py and src/discovery.cpp.
syntax match on1xfileValue /\<\%(executable\|complex\|library\|local\|git\|git-remote\|commit\|branch\|tag\|format\|plain\|latex\|troff\|shell\|off\|standard\|strict\|c11\|mir-c11\|mir-vm\|mir-archon\|default\|debug\|release\|on\|opt-in\|always\|override\|preserve\|string\|integer\|boolean\|true\|false\|cephyr\|sandas\|yellowstone\|host\)\>/
syntax match on1xfileDirective /^\s*\zs\%(on1xfile\|kind\|name\|directory\|vm\|scrutiny\|scrutiny-work\|optimization-profile\|optimization\|backend\|target\|dependency\|profile\|profile-arg\|command\|define\|dotenv\|kernel\|kernel-path\|source\|library\|resource\|interface\|compiler\|assembler\|linker\|assembler-arg\|linker-arg\|proxy-http\|proxy-https\|proxy-any\|proxy-none\)\ze\%([ \t\r#]\|\\[ \t\r]*$\|$\)/
syntax match on1xfileNumber /\k\@<!-\=\d\+\>/
syntax match on1xfileContinuation /\\\ze[ \t\r]*$/
syntax match on1xfileComment /#.*/
syntax match on1xfileBadEscape /\\./ contained
syntax match on1xfileEscape /\\\%(["\\\/bfnrt]\|u[0-9A-Fa-f]\{4}\)/ contained
syntax region on1xfileString start=/"/ skip=/\\./ end=/"/ oneline contains=on1xfileBadEscape,on1xfileEscape

highlight default link on1xfileDirective Keyword
highlight default link on1xfileValue Constant
highlight default link on1xfileNumber Number
highlight default link on1xfileContinuation Special
highlight default link on1xfileComment Comment
highlight default link on1xfileString String
highlight default link on1xfileBadEscape Error
highlight default link on1xfileEscape SpecialChar

let b:current_syntax = 'on1xfile'
