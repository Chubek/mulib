autocmd BufRead,BufNewFile *.on1x,*.on1xi if &l:filetype ==# '' | setfiletype on1x | endif
" These exact names take precedence over other languages' .m and .i suffixes.
autocmd BufRead,BufNewFile On1xfile,On1xfile.m,On1xfile.i setlocal filetype=on1xfile
