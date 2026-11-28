if exists('g:loaded_mu_tools')
  finish
endif
let g:loaded_mu_tools = 1

function! s:Docs(kind) abort
  let root = get(g:, 'mu_root', '')
  if empty(root)
    let header = findfile('Armory.hpp', expand('%:p:h') . ';')
    let root = empty(header) ? getcwd() : fnamemodify(header, ':p:h')
  endif
  let armory = filereadable(root . '/Armory.hpp') ? root : root . '/armory'
  let mulib = armory ==# root ? fnamemodify(root, ':h') : root
  let kind = empty(a:kind) ? 'mull' : a:kind
  let files = {'mull': armory . '/mull/README.md', 'armory': armory . '/manual/README.md', 'mulib': mulib . '/manual/README.md'}
  if !has_key(files, kind) || !filereadable(files[kind])
    echoerr 'MuDocs: use mull, armory or mulib, and set g:mu_root to your checkout'
    return
  endif
  execute 'edit' fnameescape(files[kind])
endfunction

command! -nargs=? MuDocs call <SID>Docs(<q-args>)
