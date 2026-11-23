if exists('b:current_syntax')
  finish
endif

" Lexical rules: grammar/On1x.g and src/lexer_fallback.cpp.
syntax case match
syntax iskeyword 48-57,65-90,95,97-122

syntax keyword on1xDeclaration fn nextgroup=on1xFunction skipwhite skipnl
syntax match on1xFunction /\<[A-Za-z_][A-Za-z0-9_]*\>\ze\s*(/ contained
syntax keyword on1xDeclaration extern let var forall
syntax keyword on1xConditional if else
syntax keyword on1xRepeat while
syntax keyword on1xStatement return
syntax keyword on1xImport import nextgroup=on1xKernelImport skipwhite skipnl
syntax match on1xKernelImport /\<kernel\ze\s*\[\s*dynlib\s*\]/ contained nextgroup=on1xKernelLinkage skipwhite
syntax match on1xKernelImport /\<kernel\ze\s\+\%(\%(as\>\)\@![A-Za-z_]\|[".]\)/ contained
syntax region on1xKernelLinkage matchgroup=on1xDelimiter start=/\[/ end=/\]/ oneline contained contains=on1xKernelModifier
syntax keyword on1xKernelModifier dynlib contained
syntax keyword on1xMacroKeyword macro rule let_syntax letrec_syntax quote splice lift
syntax keyword on1xBoolean true false
syntax keyword on1xType i64 u8 u16 u32 u64 f32 f64 bool string void json json_view sexpr buffer xfiber

" Contextual parser words: highlight common forms, not arbitrary uses as identifiers.
syntax match on1xImport /^\s*\zsfrom\ze\s\+[.A-Za-z_]/
syntax match on1xImport /\<unsafe\ze\s\+import\>/
syntax match on1xDeclaration /^\s*\zs\%(record\|enum\)\ze\s\+[A-Za-z_]/
syntax match on1xType /\<\%(code\|slice\|array\|ifiber\|channel\|sender\|receiver\)\ze\s*</
syntax match on1xType /\<\%(record\|sum\)\ze\s*{/
syntax match on1xType /\<array\ze\s*\[\|\<\%(record\|variant\)\ze\s*</
syntax match on1xConditional /\<match\>\ze\s*\%([A-Za-z_0-9"(!-]\|<-\)/
syntax match on1xConditional /\<with\ze\s*{/
syntax match on1xRepeat /\<loop\ze\s*{\|\<for\>\ze\s*\%((\|[A-Za-z_]\)/
syntax match on1xStatement /\<\%(break\|continue\)\>\ze\s*\%([A-Za-z_]\|;\)/
syntax match on1xStatement /\<defer\ze\s\+[A-Za-z_(]\|\<unsafe\ze\s*{/
syntax match on1xStatement /\<\%(taskgroup\|poll\)\ze\s*{\|\<taskgroup\ze\s\+within\>/
syntax match on1xStatement /\<launch\>\ze\s*\%({\|`\|capture\s*`\|exec\s*(\)/
syntax match on1xStatement /\<\%(join\|split\|close\|cancel\)\ze\s\+[A-Za-z_]/
syntax match on1xMacroKeyword /\<\%(block\|expression\|syntax_block\)\ze\s*{/
syntax match on1xMacroCall /\<[A-Za-z_][A-Za-z0-9_]*\ze\s*!\s*(/
syntax match on1xCapture /\$[A-Za-z_][A-Za-z0-9_]*/
syntax match on1xDirective /@\s*[A-Za-z_][A-Za-z0-9_]*/
syntax match on1xDirective /^\s*\zson1xi\ze\s\+[0-9]/

syntax match on1xNumber /\<[0-9]\+\%(u8\|u16\|u32\|u64\)\?\>/
syntax match on1xNumber /\<0[xX][0-9a-fA-F]\+\%(u8\|u16\|u32\|u64\)\?\>/
syntax match on1xFloat /\<[0-9]\+\.[0-9]\+\%([eE][+-]\?[0-9]\+\)\?\%(f32\|f64\)\?\>/
syntax match on1xFloat /\<[0-9]\+[eE][+-]\?[0-9]\+\%(f32\|f64\)\?\>/
syntax match on1xFloat /\<[0-9]\+f\%(32\|64\)\>/
syntax match on1xDelimiter /[][{}();:,.]/
syntax match on1xOperator /\%(&&\|::\|=>\|->\|<-\|\.\.\.\|\.\.=\|\.\.\|==\|!=\|<=\|>=\|||\|[+*/%<>=!|?-]\)/

syntax match on1xTodo /\<\%(TODO\|FIXME\|XXX\)\>/ contained
syntax match on1xComment /#.*/ contains=on1xTodo
syntax match on1xBadEscape /\\./ contained
syntax match on1xEscape /\\["\\nrt0]/ contained
syntax region on1xString start=/"/ skip=/\\./ end=/"/ oneline contains=on1xBadEscape,on1xEscape
syntax region on1xCommand start=/`\%(``\)\@!/ skip=/\\./ end=/`/ oneline
syntax region on1xLongCommand matchgroup=on1xCommandStart start=/```/ matchgroup=on1xCommandEnd end=/```/

" A triple-backtick token can start arbitrarily far above the visible window.
syntax sync fromstart

highlight default link on1xDeclaration Keyword
highlight default link on1xFunction Function
highlight default link on1xConditional Conditional
highlight default link on1xRepeat Repeat
highlight default link on1xStatement Statement
highlight default link on1xImport Include
highlight default link on1xKernelImport Include
highlight default link on1xKernelModifier Keyword
highlight default link on1xMacroKeyword Keyword
highlight default link on1xMacroCall Macro
highlight default link on1xCapture Identifier
highlight default link on1xDirective PreProc
highlight default link on1xType Type
highlight default link on1xBoolean Boolean
highlight default link on1xNumber Number
highlight default link on1xFloat Float
highlight default link on1xOperator Operator
highlight default link on1xDelimiter Delimiter
highlight default link on1xComment Comment
highlight default link on1xTodo Todo
highlight default link on1xString String
highlight default link on1xEscape SpecialChar
highlight default link on1xBadEscape Error
highlight default link on1xCommand String
highlight default link on1xLongCommand String
highlight default link on1xCommandStart String
highlight default link on1xCommandEnd String

let b:current_syntax = 'on1x'
