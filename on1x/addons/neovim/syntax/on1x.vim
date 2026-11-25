if exists('b:current_syntax')
  finish
endif

" Lexical rules: grammar/On1x.g and src/lexer_fallback.cpp.
syntax case match
syntax iskeyword 48-57,65-90,95,97-122

syntax keyword on1xDeclaration fn proc nextgroup=on1xFunction skipwhite skipnl
syntax match on1xFunction /\<[A-Za-z_][A-Za-z0-9_]*\>\ze\s*[(<]/ contained
syntax keyword on1xDeclaration extern let var forall
syntax keyword on1xConditional if else
syntax keyword on1xRepeat while
syntax keyword on1xStatement return
syntax keyword on1xImport import nextgroup=on1xKernelImport,on1xForeignImport skipwhite skipnl
syntax match on1xForeignImport /\<foreign\ze\s\+stdffi\s*\.\s*[A-Za-z_][A-Za-z0-9_]*\>\%(\s*\.\)\@!/ contained nextgroup=on1xForeignNamespace skipwhite
syntax keyword on1xForeignNamespace stdffi contained nextgroup=on1xForeignDot skipwhite
syntax match on1xForeignDot /\./ contained nextgroup=on1xForeignKernel skipwhite
syntax match on1xForeignKernel /\<[A-Za-z_][A-Za-z0-9_]*\>/ contained nextgroup=on1xImportAlias skipwhite
syntax keyword on1xImportAlias as contained
syntax match on1xKernelImport /\<kernel\ze\s*\[\s*dynlib\s*\]/ contained nextgroup=on1xKernelLinkage skipwhite
syntax match on1xKernelImport /\<kernel\ze\s\+\%(\%(as\>\)\@![A-Za-z_]\)/ contained
syntax region on1xKernelLinkage matchgroup=on1xDelimiter start=/\[/ end=/\]/ oneline contained contains=on1xKernelModifier
syntax keyword on1xKernelModifier dynlib contained
syntax keyword on1xMacroKeyword macro rule let_syntax letrec_syntax quote splice lift
syntax keyword on1xBoolean true false
syntax keyword on1xType i64 u8 u16 u32 u64 f32 f64 bool char string void json json_view sexpr buffer xfiber

" Contextual parser words: highlight common forms, not arbitrary uses as identifiers.
syntax match on1xImport /^\s*\zsfrom\ze\s\+[.A-Za-z_]/
syntax match on1xImport /\<unsafe\ze\s\+import\s\+kernel\>/
syntax match on1xRemovedImport /\<unsafe\s\+import\s\+foreign\>/
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
syntax match on1xNumber /\<0[oO][0-7]\+\%(u8\|u16\|u32\|u64\)\?\>/
syntax match on1xNumber /\<0[bB][01]\+\%(u8\|u16\|u32\|u64\)\?\>/
syntax match on1xFloat /\<[0-9]\+\.[0-9]\+\%([eE][+-]\?[0-9]\+\)\?\%(f32\|f64\)\?\>/
syntax match on1xFloat /\<[0-9]\+[eE][+-]\?[0-9]\+\%(f32\|f64\)\?\>/
syntax match on1xFloat /\<[0-9]\+f\%(32\|64\)\>/
syntax match on1xDelimiter /[][{}();:,.]/
syntax match on1xOperator /\%(&&\|::\|:=\|=>\|->\|<-\|\.\.\.\|\.\.=\|\.\.\|==\|!=\|<=\|>=\|||\|[+*/%<>=!|?~-]\)/

syntax match on1xTodo /\<\%(TODO\|FIXME\|XXX\)\>/ contained
syntax match on1xComment /#\%(\\\)\@!.*/ contains=on1xTodo
syntax region on1xBlockComment matchgroup=on1xCommentDelimiter start=/{-/ end=/-}/ contains=on1xNestedComment,on1xTodo
syntax region on1xBlockComment matchgroup=on1xCommentDelimiter start=/{@\%([A-Za-z_][A-Za-z0-9_]*\_s*(\)\@!/ end=/@}/ contains=on1xNestedComment,on1xTodo
syntax region on1xNestedComment matchgroup=on1xCommentDelimiter start=/{-/ end=/-}/ contained contains=on1xNestedComment,on1xTodo
syntax region on1xNestedComment matchgroup=on1xCommentDelimiter start=/{@/ end=/@}/ contained contains=on1xNestedComment,on1xTodo
syntax region on1xDocstring start=/{[!>]/ end=/}/ contains=on1xTodo
syntax match on1xCharacter /\%(\<b!\)\?#\\\%([uU]+[0-9A-Fa-f]\+\>\|[xX][0-9A-Fa-f]\+\>\|[oO][0-7]\+\>\|\%(space\|newline\|tab\|return\|alarm\|backspace\|escape\|delete\|nul\|null\|pound\)\>\|[A-Za-z0-9_]\>\|[^A-Za-z0-9_ \t\r\n]\)/
syntax match on1xBadEscape /\\./ contained
syntax match on1xEscape /\\["\\nrt$]\|\$\$/ contained
syntax match on1xApostropheEscape /\\'/ contained
syntax match on1xByteEscape /\\\%(["\\nrt0]\|x[0-9A-Fa-f]\{2}\)/ contained
syntax match on1xInterpolationName /\$[A-Za-z_][A-Za-z0-9_]*/ contained
syntax region on1xInterpolation matchgroup=on1xInterpolationDelimiter start=/\${/ end=/}/ contained contains=TOP
syntax region on1xInterpolationBrace transparent matchgroup=on1xDelimiter start=/{\%([-!>]\|@\%([A-Za-z_][A-Za-z0-9_]*\_s*(\)\@!\)\@!/ end=/}/ contained containedin=on1xInterpolation,on1xInterpolationBrace contains=TOP
syntax region on1xString start=/"/ skip=/\\./ end=/"/ oneline contains=on1xBadEscape,on1xEscape,on1xInterpolationName,on1xInterpolation
syntax region on1xLongString start=/"""/ skip=/\\./ end=/"""/ contains=on1xBadEscape,on1xEscape,on1xInterpolationName,on1xInterpolation
syntax region on1xLongString start=/'''/ skip=/\\./ end=/'''/ contains=on1xBadEscape,on1xEscape,on1xApostropheEscape,on1xInterpolationName,on1xInterpolation
syntax region on1xString start=/\<r!"/ end=/"/ oneline
syntax region on1xLongString start=/\<r!"""/ end=/"""/
syntax region on1xLongString start=/\<r!'''/ end=/'''/
syntax region on1xString start=/\<b!"/ skip=/\\./ end=/"/ oneline contains=on1xBadEscape,on1xByteEscape
syntax region on1xLongString start=/\<b!"""/ skip=/\\./ end=/"""/ contains=on1xBadEscape,on1xByteEscape
syntax region on1xLongString start=/\<b!'''/ skip=/\\./ end=/'''/ contains=on1xBadEscape,on1xByteEscape,on1xApostropheEscape
syntax region on1xCommand start=/`\%(``\)\@!/ skip=/\\./ end=/`/ oneline
syntax region on1xLongCommand matchgroup=on1xCommandStart start=/```/ matchgroup=on1xCommandEnd end=/```/

" A multiline literal or comment can start far above the visible window.
syntax sync fromstart

highlight default link on1xDeclaration Keyword
highlight default link on1xFunction Function
highlight default link on1xConditional Conditional
highlight default link on1xRepeat Repeat
highlight default link on1xStatement Statement
highlight default link on1xImport Include
highlight default link on1xForeignImport Include
highlight default link on1xForeignNamespace Include
highlight default link on1xForeignDot Delimiter
highlight default link on1xForeignKernel Identifier
highlight default link on1xImportAlias Include
highlight default link on1xRemovedImport Error
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
highlight default link on1xBlockComment Comment
highlight default link on1xNestedComment Comment
highlight default link on1xCommentDelimiter Comment
highlight default link on1xDocstring Comment
highlight default link on1xTodo Todo
highlight default link on1xCharacter Character
highlight default link on1xString String
highlight default link on1xLongString String
highlight default link on1xEscape SpecialChar
highlight default link on1xApostropheEscape SpecialChar
highlight default link on1xByteEscape SpecialChar
highlight default link on1xInterpolationName Identifier
highlight default link on1xInterpolationDelimiter Special
highlight default link on1xBadEscape Error
highlight default link on1xCommand String
highlight default link on1xLongCommand String
highlight default link on1xCommandStart String
highlight default link on1xCommandEnd String

let b:current_syntax = 'on1x'
