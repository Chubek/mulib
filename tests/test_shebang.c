#define MULIB_SHEBANG_SH_NO_MUNE
#define MULIB_IMPL_SHEBANG_SH
#include "../include/mulib-shebang-sh.h"
#include <assert.h>
#include <string.h>
int main(void){
 mulib_shebang_sh_t p;char *s;FILE *in,*out;char text[256]={0};
 assert(mulib_shebang_sh_init(&p)==0);
 assert(mulib_shebang_sh_define(&p,"NAME","hello")==0);
 assert(mulib_shebang_sh_add_filter(&p,"upper")==0);
 assert(mulib_shebang_sh_transform(&p,"${NAME}, {{NAME}}!",1,&s)==0);
 assert(!strcmp(s,"HELLO, HELLO!"));free(s);
 in=tmpfile();out=tmpfile();assert(in&&out);
 fputs("#!/bin/shebang --filter=upper\n${NAME}\n",in);rewind(in);
 assert(mulib_shebang_sh_process(&p,in,out)==0);
 rewind(out);assert(fgets(text,sizeof text,out)&&!strcmp(text,"HELLO\n"));
 fclose(in);fclose(out);mulib_shebang_sh_free(&p);return 0;
}
