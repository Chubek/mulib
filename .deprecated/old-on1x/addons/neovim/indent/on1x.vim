if exists('b:did_indent')
  finish
endif
let b:did_indent = 1

setlocal autoindent
setlocal indentexpr=On1xIndent()
setlocal indentkeys=0},0),0],o,O,!^F
let b:undo_indent = 'setlocal autoindent< indentexpr< indentkeys<'

function! s:Group(line, column) abort
  return synIDattr(synID(a:line, a:column, 1), 'name')
endfunction

function! s:InMultiline(line) abort
  for l:id in synstack(a:line, 1)
    if synIDattr(l:id, 'name') =~# '^on1x\%(LongCommand\|CommandEnd\|LongString\|BlockComment\|CommentDelimiter\|Docstring\)$'
      return 1
    endif
  endfor
  return 0
endfunction

function! s:CommentLine(line) abort
  let l:column = match(getline(a:line), '\S') + 1
  return l:column > 0 && s:Group(a:line, l:column) =~# '^on1x\%(Comment\|BlockComment\|CommentDelimiter\|Docstring\)$'
endfunction

function! s:LeadingCloses(line) abort
  let l:closes = 0
  let l:text = getline(a:line)
  let l:pos = match(l:text, '\S')
  while l:pos >= 0 && l:text[l:pos] =~# '[]})]' && s:Group(a:line, l:pos + 1) ==# 'on1xDelimiter'
    let l:closes += 1
    let l:pos = match(l:text, '\S', l:pos + 1)
  endwhile
  return l:closes
endfunction

function! On1xIndent() abort
  " Do not guess across multiline literals or comments without the syntax state.
  if get(b:, 'current_syntax', '') !=# 'on1x' || &l:syntax !=# 'on1x' || s:InMultiline(v:lnum)
    return -1
  endif

  let l:prev = prevnonblank(v:lnum - 1)
  " Align against code, not indentation within a comment or multiline literal.
  while l:prev > 0 && (s:CommentLine(l:prev) || s:InMultiline(l:prev))
    if v:lnum - l:prev > 200
      return -1
    endif
    let l:prev = prevnonblank(l:prev - 1)
  endwhile
  if l:prev == 0
    return 0
  endif

  let l:text = getline(l:prev)
  let l:delta = s:LeadingCloses(l:prev) - s:LeadingCloses(v:lnum)
  let l:pos = match(l:text, '[][{}()]')
  while l:pos >= 0
    if s:Group(l:prev, l:pos + 1) ==# 'on1xDelimiter'
      let l:delta += stridx('[{(', l:text[l:pos]) >= 0 ? 1 : -1
    endif
    let l:pos = match(l:text, '[][{}()]', l:pos + 1)
  endwhile
  return max([0, indent(l:prev) + l:delta * shiftwidth()])
endfunction
