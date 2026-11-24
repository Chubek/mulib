set nomore

function! s:CheckGroup(line, token, name) abort
  let l:column = stridx(getline(a:line), a:token) + 1
  call assert_true(l:column > 0, 'token not present: ' . a:token)
  call assert_equal(a:name, synIDattr(synID(a:line, l:column, 1), 'name'), a:token)
endfunction

try
  help on1x-editing
  call assert_equal(resolve($ON1X_RUNTIME . '/doc/on1x.txt'), resolve(expand('%:p')))
  close

  call writefile(['on1xi 1;', 'fn greet(x: string) -> bool;'], 'stored.on1xi')
  edit stored.on1xi
  call assert_equal('on1x', &filetype)
  call s:CheckGroup(1, 'on1xi', 'on1xDirective')
  call s:CheckGroup(2, 'greet', 'on1xFunction')
  call writefile(['on1xfile 1', 'backend "debug" mir-vm', 'optimization-profile default',
        \ 'optimization "all" off', 'scrutiny-work 8192', 'name "foo#\u0042" # policy'], 'On1xfile')
  edit On1xfile
  call assert_equal('on1xfile', &filetype)
  call assert_equal('# %s', &l:commentstring)
  call s:CheckGroup(1, 'on1xfile', 'on1xfileDirective')
  " The first '1' is part of the directive; the version token is the last.
  let s:version_column = strridx(getline(1), '1') + 1
  call assert_equal('on1xfileNumber', synIDattr(synID(1, s:version_column, 1), 'name'), 'manifest version')
  call s:CheckGroup(2, 'backend', 'on1xfileDirective')
  call s:CheckGroup(2, 'debug', 'on1xfileString')
  call s:CheckGroup(2, 'mir-vm', 'on1xfileValue')
  call s:CheckGroup(3, 'optimization-profile', 'on1xfileDirective')
  call s:CheckGroup(4, 'off', 'on1xfileValue')
  call s:CheckGroup(5, '8192', 'on1xfileNumber')
  call s:CheckGroup(6, '#\u', 'on1xfileString')
  call s:CheckGroup(6, '\u0042', 'on1xfileEscape')
  call s:CheckGroup(6, '# policy', 'on1xfileComment')
  call writefile(['on1xfile 1', 'library "messages.on1x"'], 'On1xfile.m')
  edit On1xfile.m
  call assert_equal('on1xfile', &filetype)
  call s:CheckGroup(2, 'library', 'on1xfileDirective')
  edit On1xfile.i
  call assert_equal('on1xfile', &filetype)
  call assert_equal('on1xfile', b:current_syntax)

  " setfiletype must not override a previously chosen filetype.
  new
  setfiletype text
  doautocmd BufRead claimed.on1x
  call assert_equal('text', &filetype)
  bwipeout

  edit fresh.on1x
  call assert_equal('on1x', &filetype)
  call assert_equal('on1x', b:current_syntax)
  call assert_equal('# %s', &l:commentstring)
  call assert_equal(':#', &l:comments)
  call assert_equal(-1, stridx(&l:formatoptions, 't'))
  setlocal shiftwidth=2 tabstop=8 expandtab
  call setline(1, [
        \ '@scrutiny warn;',
        \ 'macro identity { rule ($value: Expression) => expression { $value }; }',
        \ 'fn main() {',
        \ 'let text = "a\"#b}"; # TODO',
        \ 'let n = identity!(0X2Au16 + 1.25e-2f32 + 2f64);',
        \ 'for i in 1..=3 {',
        \ 'println(text); # (',
        \ '}',
        \ 'taskgroup {',
        \ 'let child = launch `/bin/echo "#" escaped\`tick`;',
        \ 'println(join child);',
        \ 'let other = launch ```/bin/echo',
        \ ' # these bytes are part of the command: } ) ]',
        \ '  ```;',
        \ 'println(join other);',
        \ '}',
        \ 'println(',
        \ 'true',
        \ ');',
        \ '}',
        \ '# async await switch // /*',
        \ 'async await switch // /*',
        \ 'match_count for_each breaker launcher',
        \ 'unsafe import kernel [ dynlib ] mulib.clamp as Clamp;',
        \ 'let dynlib = kernel[dynlib];',
        \ ])
  call s:CheckGroup(1, '@scrutiny', 'on1xDirective')
  call s:CheckGroup(2, 'macro', 'on1xMacroKeyword')
  call s:CheckGroup(2, '$value', 'on1xCapture')
  call s:CheckGroup(3, 'main', 'on1xFunction')
  call s:CheckGroup(4, '\"', 'on1xEscape')
  call s:CheckGroup(4, '#b', 'on1xString')
  call s:CheckGroup(4, '# TODO', 'on1xComment')
  call s:CheckGroup(4, 'TODO', 'on1xTodo')
  call s:CheckGroup(5, 'identity!', 'on1xMacroCall')
  call s:CheckGroup(5, '0X2Au16', 'on1xNumber')
  call s:CheckGroup(5, '1.25e-2f32', 'on1xFloat')
  call s:CheckGroup(5, '2f64', 'on1xFloat')
  call s:CheckGroup(6, '..=', 'on1xOperator')
  call s:CheckGroup(7, '(', 'on1xDelimiter')
  call s:CheckGroup(9, 'taskgroup', 'on1xStatement')
  call s:CheckGroup(10, '`/bin', 'on1xCommand')
  call s:CheckGroup(10, '"#"', 'on1xCommand')
  call s:CheckGroup(13, '#', 'on1xLongCommand')
  call s:CheckGroup(14, '```', 'on1xCommandEnd')
  call s:CheckGroup(15, 'join', 'on1xStatement')
  call s:CheckGroup(18, 'true', 'on1xBoolean')
  call s:CheckGroup(22, 'async', '')
  call s:CheckGroup(22, 'await', '')
  call s:CheckGroup(22, 'switch', '')
  call s:CheckGroup(22, '//', 'on1xOperator')
  call s:CheckGroup(22, '/*', 'on1xOperator')
  for s:word in ['match_count', 'for_each', 'breaker', 'launcher']
    call s:CheckGroup(23, s:word, '')
  endfor

  call s:CheckGroup(24, 'kernel', 'on1xKernelImport')
  call s:CheckGroup(24, 'dynlib', 'on1xKernelModifier')
  call s:CheckGroup(25, 'dynlib', '')

  " Match the file's code indentation but preserve raw command bytes.
  normal! gg=G
  call assert_equal([0, 0, 0, 2, 2, 2, 4, 2, 2, 4, 4, 4, 1, 2, 4, 2, 2, 4, 2, 0],
        \ map(range(1, 20), {_, line -> indent(line)}))
  call assert_equal(' # these bytes are part of the command: } ) ]', getline(13))
  call assert_equal('  ```;', getline(14))

  if has('nvim-0.10')
    lua assert(#vim.lsp.get_clients() == 0, 'LSP started by default')
    lua require('on1x').setup({ cmd = { 'not-an-editor-test-command' } })
    call assert_equal(2, exists(':On1xLspStart'))
    lua assert(#vim.lsp.get_clients() == 0, 'setup started LSP')
    lua local id, err = require('on1x').start({ cmd = 'python3 ...' }); assert(not id and err:find('argv list'))
    lua local id, err = require('on1x').start({ cmd = { 'not-an-editor-test-command' } }); assert(not id and err:find('not executable'))
    lua local id, err = require('on1x').start({ server = '/not-an-editor-test-path' }); assert(not id and err:find('cannot read'))
    edit interface.on1xi
    lua local id, err = require('on1x').start(); assert(not id and err:find('On1x source file'))
    buffer fresh.on1x
    new
    lua local id, err = require('on1x').start(); assert(not id and err:find('On1x source file'))
    close
  endif

  syntax off
  call setline(4, '     let text = "kept";')
  4normal! ==
  call assert_equal(5, indent(4), 'indent without syntax should leave existing whitespace alone')
  setlocal filetype=
  call assert_equal(&g:comments, &l:comments)
  call assert_equal(&g:commentstring, &l:commentstring)
  call assert_equal(&g:formatoptions, &l:formatoptions)
  call assert_equal('', &l:indentexpr)
catch
  call add(v:errors, v:throwpoint . ': ' . v:exception)
endtry

if !empty(v:errors)
  call writefile(v:errors, $ON1X_TEST_LOG)
  cquit
endif
qa!
