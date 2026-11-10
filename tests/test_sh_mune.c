#define MULIB_IMPL_MUNE
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_PARALLEL_SH
#define MULIB_IMPL_SHEBANG_SH
#include "../include/mulib-parallel-sh.h"
#include "../include/mulib-shebang-sh.h"
#include <assert.h>

static void lua(mulib_shebang_sh_t *p,const char *source){
  int rc=mulib_shebang_sh_lua(p,source,strlen(source));
  if(rc)fprintf(stderr,"%s\n",p->error);
  assert(!rc);
}
int main(void){
  mulib_parallel_sh_t p;mulib_shebang_sh_t s;char error[256],*out;FILE *f=tmpfile();
  const char *source="channel('q'); send('q', 'hello'); assert(recv('q') == 'hello'); "
    "assert(recv('q') == nil); task('printf shell'); lua_task('print(\"lua\")'); assert(run())";
  assert(f);assert(!mulib_parallel_sh_init(&p,2));p.keep_order=1;p.stdout_file=f;
  assert(!mulib_parallel_sh_lua(&p,source,strlen(source),error,sizeof error));
  assert(p.ran&&p.completed==2&&!p.last_status);
  {char buf[64]={0};rewind(f);assert(fread(buf,1,sizeof buf-1,f)==9);assert(!strcmp(buf,"shelllua\n"));}
  mulib_parallel_sh_free(&p);fclose(f);
  assert(!mulib_parallel_sh_init(&p,1));
  source="task('exit 4'); assert(not run())";
  assert(!mulib_parallel_sh_lua(&p,source,strlen(source),error,sizeof error));
  assert(p.last_status==MULIB_PARALLEL_SH_FAILED&&p.failed==1);
  mulib_parallel_sh_free(&p);
  {char *args[]={"parallel","--lua-code","task('exit 5'); run()"};
   assert(mulib_parallel_sh_main(3,args)==1);}
  assert(!mulib_parallel_sh_init(&p,1));
  assert(mulib_parallel_sh_lua_file(&p,"/nonexistent/mulib-script",error,sizeof error)==MULIB_PARALLEL_SH_SYSTEM&&error[0]);
  source="task('bad' .. string.char(0))";
  assert(mulib_parallel_sh_lua(&p,source,strlen(source),error,sizeof error)==MULIB_PARALLEL_SH_FAILED&&!p.count);
  mulib_parallel_sh_free(&p);

  assert(!mulib_shebang_sh_init(&s));
  lua(&s,"define('X', 'hello'); add_filter('upper'); function macro(n) if n == 'Y' then return 'world' end end; "
         "function filter(line, n) return line .. ':' .. tostring(n) end");
  assert(!mulib_shebang_sh_transform(&s,"${X} {{Y}}",2,&out));
  assert(!strcmp(out,"HELLO WORLD:2"));free(out);
  lua(&s,"function filter(line, n) return nil end");
  assert(!mulib_shebang_sh_transform(&s,"${X}",1,&out)&&!strcmp(out,"HELLO"));free(out);
  lua(&s,"function filter() error('callback failed') end");
  assert(mulib_shebang_sh_transform(&s,"x",1,&out)==MULIB_SHEBANG_SH_LUA&&!out&&s.error[0]);
  lua(&s,"function filter() return string.char(0) end");
  assert(mulib_shebang_sh_transform(&s,"x",1,&out)==MULIB_SHEBANG_SH_INVALID&&!out);
  lua(&s,"function filter() return string.rep('x', 100) end");s.max_expansion=4;
  assert(mulib_shebang_sh_transform(&s,"x",1,&out)==MULIB_SHEBANG_SH_LIMIT&&!out);
  lua(&s,"function filter() return 42 end");
  assert(mulib_shebang_sh_transform(&s,"x",1,&out)==MULIB_SHEBANG_SH_LUA&&!out);
  lua(&s,"filter = nil; function macro() return string.char(0) end");
  assert(mulib_shebang_sh_transform(&s,"${Z}",1,&out)==MULIB_SHEBANG_SH_INVALID&&!out);
  mulib_shebang_sh_free(&s);return 0;
}
