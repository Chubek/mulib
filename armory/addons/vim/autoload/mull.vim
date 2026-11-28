function! mull#Complete(findstart, base) abort
  if a:findstart
    let start = col('.') - 1
    let line = getline('.')
    while start > 0 && line[start - 1] =~# '[A-Za-z_0-9]'
      let start -= 1
    endwhile
    return start
  endif
  let words = split('val var ref mulib fn if else while return break and or not true false nil G')
  if strpart(getline('.'), 0, col('.') - 1) =~# '\<G\s*:\s*\w*$'
    let words = split('LoadLib Native GetStdout DieWith Assert Str Type Len MulibWrap')
  else
    for line in getline(1, '$')
      let name = matchstr(line, '\<\%(val\|var\|ref\|mulib\)\s\+\zs[A-Za-z_][A-Za-z_0-9]*')
      if !empty(name)
        call add(words, name)
      endif
    endfor
  endif
  return filter(sort(uniq(sort(words))), 'stridx(v:val, a:base) == 0')
endfunction

function! mull#Indent() abort
  let previous = prevnonblank(v:lnum - 1)
  if previous == 0
    return 0
  endif
  let delta = 0
  let text = getline(previous)
  for index in range(strlen(text))
    let group = synIDattr(synID(previous, index + 1, 1), 'name')
    if group =~# 'String\|Escape\|Comment\|LastError'
      continue
    endif
    if text[index] =~# '[{([]'
      let delta += 1
    elseif text[index] =~# '[})\]]'
      let delta -= 1
    endif
  endfor
  " The previous closing line is already dedented.
  if text =~# '^\s*[})\]]'
    let delta += 1
  endif
  if getline(v:lnum) =~# '^\s*[})\]]'
    let delta -= 1
  endif
  return max([0, indent(previous) + delta * shiftwidth()])
endfunction
