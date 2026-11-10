#define MULIB_SHEBANG_SH_NO_MUNE
#define MULIB_IMPL_SHEBANG_SH
#include "../include/mulib-shebang-sh.h"
#include <assert.h>

static void transformed(mulib_shebang_sh_t *p,const char *in,const char *expected){
  char *out=NULL;assert(!mulib_shebang_sh_transform(p,in,12,&out));
  assert(out&&!strcmp(out,expected));free(out);
}
static void processed(mulib_shebang_sh_t *p,const char *in,size_t length,const char *expected,int status){
  FILE *source=tmpfile(),*dest=tmpfile();char buf[512];size_t n;
  assert(source&&dest);assert(fwrite(in,1,length,source)==length);rewind(source);
  assert(mulib_shebang_sh_process(p,source,dest)==status);
  rewind(dest);n=fread(buf,1,sizeof buf-1,dest);buf[n]=0;assert(!strcmp(buf,expected));
  fclose(source);fclose(dest);
}
int main(void){
  mulib_shebang_sh_t p;char *out=(char*)1;char name[258];
  assert(!mulib_shebang_sh_init(&p));
  assert(!mulib_shebang_sh_define(&p,"NAME","${other}"));
  transformed(&p,"${NAME} $${NAME} {{missing}} ${}","${other} ${NAME} {{missing}} ${}");
  assert(!strcmp(mulib_shebang_sh_lookup(&p,"NAME"),"${other}"));
  assert(!mulib_shebang_sh_define(&p,"NAME","new"));
  transformed(&p,"${NAME}","new");
  assert(!mulib_shebang_sh_undefine(&p,"NAME")&&!mulib_shebang_sh_lookup(&p,"NAME"));
  assert(!mulib_shebang_sh_undefine(&p,"NAME"));
  memset(name,'a',sizeof name);name[257]=0;
  assert(mulib_shebang_sh_define(&p,name,"x")==MULIB_SHEBANG_SH_INVALID);
  name[256]=0;assert(!mulib_shebang_sh_define(&p,name,"x"));
  p.strict_macros=1;
  assert(mulib_shebang_sh_transform(&p,"${missing}",1,&out)==MULIB_SHEBANG_SH_INVALID&&!out&&p.error[0]);
  transformed(&p,"okay","okay");assert(!p.error[0]);
  assert(mulib_shebang_sh_transform(&p,"",0,&out)==MULIB_SHEBANG_SH_INVALID&&!out);
  p.max_line=3;p.max_expansion=3;
  transformed(&p,"abc","abc");
  assert(mulib_shebang_sh_transform(&p,"abcd",1,&out)==MULIB_SHEBANG_SH_LIMIT&&!out);
  p.max_line=20;assert(!mulib_shebang_sh_define(&p,"X","abcd"));
  assert(mulib_shebang_sh_transform(&p,"${X}",1,&out)==MULIB_SHEBANG_SH_LIMIT&&!out);
  p.max_expansion=0;transformed(&p,"","");
  assert(mulib_shebang_sh_transform(&p,"x",1,&out)==MULIB_SHEBANG_SH_LIMIT&&!out);
  mulib_shebang_sh_free(&p);
  {
    const char *spec[]={"upper","lower","trim","squeeze","number","strip-comments","json-escape","shell-quote","prefix:a:","suffix::z"};
    const char *input[]={"aB","aB"," \tx \r","a\t \nb","x","x#y","\"\\\t\r\n\001","a'b","x","x"};
    const char *output[]={"AB","ab","x","a b","12:x","x","\\\"\\\\\\t\\r\\n\\u0001","'a'\\''b'","a:x","x:z"};
    for(size_t i=0;i<sizeof spec/sizeof *spec;i++){
      assert(!mulib_shebang_sh_init(&p));assert(!mulib_shebang_sh_add_filter(&p,spec[i]));
      transformed(&p,input[i],output[i]);mulib_shebang_sh_free(&p);
    }
  }
  assert(!mulib_shebang_sh_init(&p));
  assert(mulib_shebang_sh_add_filter(&p,"prefix")==MULIB_SHEBANG_SH_INVALID);
  assert(mulib_shebang_sh_add_filter(&p,"upper:x")==MULIB_SHEBANG_SH_INVALID);
  assert(!mulib_shebang_sh_add_filter(&p,"number"));
  processed(&p,"#!ignored\n\nlast",15,"2:\n3:last",0);
  processed(&p,"#!ignored",9,"",0);
  processed(&p,"",0,"",0);
  processed(&p,"x\0y",3,"",MULIB_SHEBANG_SH_INVALID);
  p.max_line=2;processed(&p,"abc",3,"",MULIB_SHEBANG_SH_LIMIT);
  p.max_line=100;p.skip_shebang=0;
  processed(&p,"#!x\n",4,"1:#!x\n",0);
  {FILE *in=tmpfile(),*bad=fopen("/dev/null","r");assert(in&&bad);
   fputs("x",in);rewind(in);assert(mulib_shebang_sh_process(&p,in,bad)==MULIB_SHEBANG_SH_IO);
   fclose(in);fclose(bad);}
  mulib_shebang_sh_free(&p);mulib_shebang_sh_free(&p);return 0;
}
