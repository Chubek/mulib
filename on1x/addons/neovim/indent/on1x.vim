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

function! s:InCommand(line) abort
  return s:Group(a:line, 1) =~# '^on1x\%(LongCommand\|CommandEnd\)$'
endfunction

function! s:LeadingCloses(text) abort
  return strlen(substitute(matchstr(a:text, '^\s*\zs\%([]})]\s*\)\+'), '\s', '', 'g'))
endfunction

function! On1xIndent() abort
  " Do not guess across command literal bodies without the syntax state.
  if get(b:, 'current_syntax', '') !=# 'on1x' || &l:syntax !=# 'on1x' || s:InCommand(v:lnum)
    return -1
  endif

  let l:prev = prevnonblank(v:lnum - 1)
  " Align against code, not indentation within a comment or multiline command.
  while l:prev > 0 && (getline(l:prev) =~# '^\s*#' || s:InCommand(l:prev))
    if v:lnum - l:prev > 200
      return -1
    endif
    let l:prev = prevnonblank(l:prev - 1)
  endwhile
  if l:prev == 0
    return 0
  endif

  let l:text = getline(l:prev)
  let l:delta = s:LeadingCloses(l:text) - s:LeadingCloses(getline(v:lnum))
  let l:pos = match(l:text, '[][{}()]')
  while l:pos >= 0
    if s:Group(l:prev, l:pos + 1) ==# 'on1xDelimiter'
      let l:delta += stridx('[{(', l:text[l:pos]) >= 0 ? 1 : -1
    endif
    let l:pos = match(l:text, '[][{}()]', l:pos + 1)
  endwhile
  return max([0, indent(l:prev) + l:delta * shiftwidth()])
endfunction
