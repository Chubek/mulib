#define MULIB_PARALLEL_SH_NO_MUNE
#define MULIB_IMPL_PARALLEL_SH
#include "../include/mulib-parallel-sh.h"
#include <assert.h>
#include <string.h>
int main(void){
 mulib_parallel_sh_t p;mulib_parallel_sh_channel_t *c;char *a,*s;
 assert(mulib_parallel_sh_init(&p,2)==0);
 s=mulib_parallel_sh_expand("echo {} {.} {/} {//} {#}","/foo/bar.txt",1);
 assert(s&&strstr(s,"'/foo/bar.txt'")&&strstr(s,"'/foo/bar'")&&strstr(s,"'bar.txt'")&&strstr(s,"'/foo'")&&strstr(s,"1"));free(s);
 assert(mulib_parallel_sh_channel_create(&p,"news",&c)==0);
 assert(mulib_parallel_sh_channel_send(c,"hello")==0);
 assert(mulib_parallel_sh_channel_receive(c,&a)==0&&!strcmp(a,"hello"));free(a);
 assert(mulib_parallel_sh_add(&p,"printf 'one\\n'")==0);
 assert(mulib_parallel_sh_add_input(&p,"printf '%s\\n' {}","two")==0);
 p.keep_order=1;
 assert(mulib_parallel_sh_run(&p)==0&&p.completed==2);
 mulib_parallel_sh_free(&p);return 0;
}
