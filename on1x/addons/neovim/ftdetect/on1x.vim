autocmd BufRead,BufNewFile *.on1x,*.on1xi if &l:filetype ==# '' | setfiletype on1x | endif
