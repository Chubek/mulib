if exists('b:current_syntax')
  finish
endif
syntax case match
syntax keyword mullStorage val var ref mulib fn
syntax keyword mullControl if else while return break
syntax keyword mullBoolean true false nil
syntax keyword mullWordOperator and or not
syntax keyword mullGlobal G
syntax match mullNumber /\<\d\+\%(\.\d*\)\?\%([eE][+-]\?\d\+\)\?/
syntax match mullOperator /<-\|==\|!=\|<=\|>=\|[+*\/%<>!\-]/
syntax match mullFunction /\<[A-Za-z_][A-Za-z_0-9]*\ze\s*(/
syntax region mullString start=/"/ skip=/\\\\\|\\"/ end=/"/ contains=mullBadEscape,mullEscape,mullLastError
syntax match mullBadEscape /\\./ contained
syntax match mullEscape /\\[nrt0\\"]/ contained
syntax match mullLastError /\$E?/ contained
syntax keyword mullTodo TODO FIXME XXX contained
syntax match mullComment /#.*$/ contains=mullTodo,@Spell
syntax match mullComment +//.*$+ contains=mullTodo,@Spell
highlight default link mullStorage StorageClass
highlight default link mullControl Conditional
highlight default link mullBoolean Boolean
highlight default link mullWordOperator Operator
highlight default link mullGlobal Special
highlight default link mullNumber Number
highlight default link mullOperator Operator
highlight default link mullFunction Function
highlight default link mullString String
highlight default link mullBadEscape Error
highlight default link mullEscape SpecialChar
highlight default link mullLastError Special
highlight default link mullComment Comment
highlight default link mullTodo Todo
let b:current_syntax = 'mull'
