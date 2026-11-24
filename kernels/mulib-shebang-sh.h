/* mulib-shebang-sh.h -- programmable POSIX shebang text processor.
 * Define MULIB_IMPL_SHEBANG_SH once. Standalone C11 single-header; optional
 * Mune integration is on by default. Define MULIB_SHEBANG_SH_NO_MUNE to omit.
 * POSIX execution needs -D_POSIX_C_SOURCE=200809L before system includes.
 * POSIX kernel shebangs normally receive at most one optional argument; main()
 * parses that argument as its OWN mini-argv before the script filename.
 * Text processing supports ${NAME} and {{NAME}} macros, positional ${0},
 * ${1}... arguments, filters, Lua `filter(line, number)` and `macro(name)`.
 * --exec=PROGRAM runs the transformed script through that explicit program.
 * No implicit shell execution and no claim of arbitrary interpreter emulation.
 */
#ifndef MULIB_SHEBANG_SH_H
#define MULIB_SHEBANG_SH_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifndef MULIB_SHEBANG_SH_NO_MUNE
#include "mulib-mune.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_shebang_sh_macro {char *name,*value;}mulib_shebang_sh_macro_t;
typedef struct mulib_shebang_sh_filter {char *name,*argument;}mulib_shebang_sh_filter_t;
typedef struct mulib_shebang_sh {
  mulib_shebang_sh_macro_t *macros;size_t macros_count,macros_capacity;
  mulib_shebang_sh_filter_t *filters;size_t filters_count,filters_capacity;
  size_t max_line,max_expansion;
  int skip_shebang, strict_macros;
  char error[192];
#ifndef MULIB_SHEBANG_SH_NO_MUNE
  mulib_mune_state_t *lua;
#endif
}mulib_shebang_sh_t;
enum {MULIB_SHEBANG_SH_OK=0,MULIB_SHEBANG_SH_INVALID=-1,
      MULIB_SHEBANG_SH_NOMEM=-2,MULIB_SHEBANG_SH_IO=-3,
      MULIB_SHEBANG_SH_LIMIT=-4,MULIB_SHEBANG_SH_LUA=-5,
      MULIB_SHEBANG_SH_UNSUPPORTED=-6};
int mulib_shebang_sh_init(mulib_shebang_sh_t *p);
void mulib_shebang_sh_free(mulib_shebang_sh_t *p);
int mulib_shebang_sh_define(mulib_shebang_sh_t *p,const char *name,const char *value);
/** Borrow a value until its definition changes or p is freed; NULL if absent. */
const char *mulib_shebang_sh_lookup(const mulib_shebang_sh_t *p,const char *name);
/** Remove a definition, if present. */
int mulib_shebang_sh_undefine(mulib_shebang_sh_t *p,const char *name);
int mulib_shebang_sh_add_filter(mulib_shebang_sh_t *p,const char *spec);
/** Transform one line; returned output is malloc-owned (NULL on error).
 * Number is 1-based. Limits count bytes excluding the terminating NUL. */
int mulib_shebang_sh_transform(mulib_shebang_sh_t *p,const char *line,
                               size_t number,char **out);
int mulib_shebang_sh_process(mulib_shebang_sh_t *p,FILE *source,FILE *output);
/** Load Lua source. Exposes native define(name,value), add_filter(name).
 * Optional Lua globals `macro(name)` and `filter(line,number)` are called
 * during text processing. Lua callbacks execute synchronously on one thread. */
int mulib_shebang_sh_lua(mulib_shebang_sh_t *p,const char *source,size_t length);
int mulib_shebang_sh_lua_file(mulib_shebang_sh_t *p,const char *path);
/** Return process exit status in 0..255. Standalone: shebang_sh_main(argc,argv). */
int mulib_shebang_sh_main(int argc,char **argv);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SHEBANG_SH) && !defined(MULIB_SHEBANG_SH_IMPLEMENTED)
#define MULIB_SHEBANG_SH_IMPLEMENTED
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#define MULIB_SHEBANG_SH_POSIX 1
#else
#define MULIB_SHEBANG_SH_POSIX 0
#endif
static char *mulib_shebang_sh_dup_(const char *s){size_t n;char *p;
  if(!s)return NULL;
  n=strlen(s);if(n==SIZE_MAX)return NULL;
  p=(char*)malloc(n+1);if(p)memcpy(p,s,n+1);return p;
}
static int mulib_shebang_sh_append_(char **s,size_t *len,size_t *cap,
                                    const char *value,size_t n,size_t max){
  size_t next;char *temp;
  if(*len>max||n>max-*len||*len==SIZE_MAX||n>SIZE_MAX-*len-1){errno=ERANGE;return 0;}
  if(*len+n+1>*cap){next=*cap?*cap:64;
    while(next<*len+n+1){if(next>SIZE_MAX/2){next=*len+n+1;break;}next*=2;}
    temp=(char*)realloc(*s,next);if(!temp){errno=ENOMEM;return 0;}*s=temp;*cap=next;
  }
  if(n)memcpy(*s+*len,value,n);
  *len+=n;(*s)[*len]=0;return 1;
}
static void mulib_shebang_sh_error_(mulib_shebang_sh_t *p,const char *reason){
  if(p)snprintf(p->error,sizeof p->error,"%s",reason);
}
int mulib_shebang_sh_init(mulib_shebang_sh_t *p){
  if(!p)return MULIB_SHEBANG_SH_INVALID;
  memset(p,0,sizeof *p);p->max_line=1024*1024;p->max_expansion=8*1024*1024;p->skip_shebang=1;
  return 0;
}
static int mulib_shebang_sh_name_(const char *name){size_t i;
  if(!name||!((*name>='A'&&*name<='Z')||(*name>='a'&&*name<='z')||*name=='_'||(*name>='0'&&*name<='9')))return 0;
  for(i=1;name[i];i++)if(i>=256||!((name[i]>='A'&&name[i]<='Z')||
    (name[i]>='a'&&name[i]<='z')||(name[i]>='0'&&name[i]<='9')||name[i]=='_'))return 0;
  return 1;
}
const char *mulib_shebang_sh_lookup(const mulib_shebang_sh_t *p,const char *name){
  if(!p||!name)return NULL;
  for(size_t i=0;i<p->macros_count;i++)if(!strcmp(p->macros[i].name,name))return p->macros[i].value;
  return NULL;
}
int mulib_shebang_sh_undefine(mulib_shebang_sh_t *p,const char *name){
  if(!p||!mulib_shebang_sh_name_(name))return MULIB_SHEBANG_SH_INVALID;
  for(size_t i=0;i<p->macros_count;i++)if(!strcmp(p->macros[i].name,name)){
    free(p->macros[i].name);free(p->macros[i].value);
    memmove(p->macros+i,p->macros+i+1,(p->macros_count-i-1)*sizeof *p->macros);
    p->macros_count--;break;
  }
  return 0;
}
int mulib_shebang_sh_define(mulib_shebang_sh_t *p,const char *name,const char *value){
  mulib_shebang_sh_macro_t *a;char *copy,*key;size_t cap;
  if(!p||!mulib_shebang_sh_name_(name)||!value)return MULIB_SHEBANG_SH_INVALID;
  copy=mulib_shebang_sh_dup_(value);if(!copy)return MULIB_SHEBANG_SH_NOMEM;
  for(size_t i=0;i<p->macros_count;i++)if(!strcmp(p->macros[i].name,name)){
    free(p->macros[i].value);p->macros[i].value=copy;return 0;
  }
  key=mulib_shebang_sh_dup_(name);if(!key){free(copy);return MULIB_SHEBANG_SH_NOMEM;}
  if(p->macros_count==p->macros_capacity){
    cap=p->macros_capacity?p->macros_capacity*2:8;
    if(cap<p->macros_capacity||cap>SIZE_MAX/sizeof *a){free(key);free(copy);return MULIB_SHEBANG_SH_NOMEM;}
    a=(mulib_shebang_sh_macro_t*)realloc(p->macros,cap*sizeof *a);
    if(!a){free(key);free(copy);return MULIB_SHEBANG_SH_NOMEM;}
    p->macros=a;p->macros_capacity=cap;
  }
  p->macros[p->macros_count++]=(mulib_shebang_sh_macro_t){key,copy};return 0;
}
int mulib_shebang_sh_add_filter(mulib_shebang_sh_t *p,const char *spec){
  static const char *const builtins[]={"upper","lower","trim","squeeze","number",
    "strip-comments","json-escape","shell-quote","prefix","suffix"};
  const char *colon;size_t length;char *name=NULL,*arg=NULL;int known=0;
  mulib_shebang_sh_filter_t *a;size_t cap;
  if(!p||!spec||!*spec)return MULIB_SHEBANG_SH_INVALID;
  colon=strchr(spec,':');length=colon?(size_t)(colon-spec):strlen(spec);
  name=(char*)malloc(length+1);if(!name)return MULIB_SHEBANG_SH_NOMEM;
  memcpy(name,spec,length);name[length]=0;
  for(size_t i=0;i<sizeof builtins/sizeof *builtins;i++)if(!strcmp(name,builtins[i]))known=1;
  if(!known||((!strcmp(name,"prefix")||!strcmp(name,"suffix"))?!colon:!!colon)){
    free(name);return MULIB_SHEBANG_SH_INVALID;
  }
  if(colon){arg=mulib_shebang_sh_dup_(colon+1);if(!arg){free(name);return MULIB_SHEBANG_SH_NOMEM;}}
  if(p->filters_count==p->filters_capacity){cap=p->filters_capacity?p->filters_capacity*2:8;
    if(cap<p->filters_capacity||cap>SIZE_MAX/sizeof *a){free(name);free(arg);return MULIB_SHEBANG_SH_NOMEM;}
    a=(mulib_shebang_sh_filter_t*)realloc(p->filters,cap*sizeof *a);
    if(!a){free(name);free(arg);return MULIB_SHEBANG_SH_NOMEM;}
    p->filters=a;p->filters_capacity=cap;
  }
  p->filters[p->filters_count++]=(mulib_shebang_sh_filter_t){name,arg};return 0;
}
#ifndef MULIB_SHEBANG_SH_NO_MUNE
static char *mulib_shebang_sh_copy_bytes_(const char *s,size_t n){
  char *p;
  if(!s||n==SIZE_MAX||memchr(s,0,n))return NULL;
  p=(char*)malloc(n+1);if(!p)return NULL;
  memcpy(p,s,n);p[n]=0;return p;
}
static mulib_mune_status_t mulib_shebang_sh_lua_define_(mulib_mune_state_t *st,
  const mulib_mune_value_t *args,size_t count,mulib_mune_result_t *out,void *ctx){
  size_t n,m;const char *a,*b;char *key,*val;int rc;(void)st;(void)out;
  if(count!=2||args[0].type!=MULIB_MUNE_STRING||args[1].type!=MULIB_MUNE_STRING)return MULIB_MUNE_INVALID;
  a=mulib_mune_value_string(args[0],&n);b=mulib_mune_value_string(args[1],&m);
  key=mulib_shebang_sh_copy_bytes_(a,n);val=mulib_shebang_sh_copy_bytes_(b,m);
  if(!key||!val){free(key);free(val);return MULIB_MUNE_INVALID;}
  rc=mulib_shebang_sh_define((mulib_shebang_sh_t*)ctx,key,val);free(key);free(val);
  return rc?MULIB_MUNE_RUNTIME:MULIB_MUNE_OK;
}
static mulib_mune_status_t mulib_shebang_sh_lua_add_filter_(mulib_mune_state_t *st,
  const mulib_mune_value_t *args,size_t count,mulib_mune_result_t *out,void *ctx){
  size_t n;const char *a;char *copy;int rc;(void)st;(void)out;
  if(count!=1||args[0].type!=MULIB_MUNE_STRING)return MULIB_MUNE_INVALID;
  a=mulib_mune_value_string(args[0],&n);copy=mulib_shebang_sh_copy_bytes_(a,n);
  if(!copy)return MULIB_MUNE_INVALID;
  rc=mulib_shebang_sh_add_filter((mulib_shebang_sh_t*)ctx,copy);free(copy);
  return rc?MULIB_MUNE_RUNTIME:MULIB_MUNE_OK;
}
int mulib_shebang_sh_lua(mulib_shebang_sh_t *p,const char *source,size_t length){
  mulib_mune_result_t *result=NULL;mulib_mune_error_t err={0};int rc;
  if(!p||!source)return MULIB_SHEBANG_SH_INVALID;
  p->error[0]=0;
  if(!p->lua){p->lua=mulib_mune_state_create(NULL);
    if(!p->lua)return MULIB_SHEBANG_SH_NOMEM;
    if(mulib_mune_bind(p->lua,"define",mulib_shebang_sh_lua_define_,p)!=MULIB_MUNE_OK||
       mulib_mune_bind(p->lua,"add_filter",mulib_shebang_sh_lua_add_filter_,p)!=MULIB_MUNE_OK){
      mulib_mune_state_destroy(p->lua);p->lua=NULL;
      mulib_shebang_sh_error_(p,"Lua binding error");return MULIB_SHEBANG_SH_LUA;
    }
  }
  rc=mulib_mune_run_source(p->lua,source,length,&result,&err)==MULIB_MUNE_OK?0:MULIB_SHEBANG_SH_LUA;
  if(rc)mulib_shebang_sh_error_(p,err.message?err.message:"Lua execution error");
  mulib_mune_result_destroy(result);return rc;
}
static int mulib_shebang_sh_lua_string_(mulib_shebang_sh_t *p,const char *func,
 const char *s,size_t line,char **out,int *present){
  mulib_mune_value_t fn,args[2],rv;mulib_mune_result_t *res=NULL;
  mulib_mune_error_t err={0};size_t n;int rc;
  *present=0;
  if(!p->lua)return 0;
  if(mulib_mune_get_global(p->lua,func,&fn)!=MULIB_MUNE_OK)return MULIB_SHEBANG_SH_LUA;
  if(fn.type==MULIB_MUNE_NIL)return 0;
  if(fn.type!=MULIB_MUNE_FUNCTION){mulib_shebang_sh_error_(p,"Lua callback must be a function");return MULIB_SHEBANG_SH_LUA;}
  if(mulib_mune_make_string(p->lua,s,strlen(s),&args[0])!=MULIB_MUNE_OK)return MULIB_SHEBANG_SH_NOMEM;
  args[1]=mulib_mune_integer((int64_t)line);
  rc=mulib_mune_call(p->lua,fn,args,!strcmp(func,"macro")?1:2,&res,&err)!=MULIB_MUNE_OK;
  if(rc){mulib_shebang_sh_error_(p,err.message?err.message:"Lua callback failed");mulib_mune_result_destroy(res);return MULIB_SHEBANG_SH_LUA;}
  if(!res||!mulib_mune_result_count(res)){
    mulib_mune_result_destroy(res);return 0;
  }
  rv=mulib_mune_result_get(res,0);
  if(rv.type==MULIB_MUNE_STRING){const char *txt=mulib_mune_value_string(rv,&n);
    if(n>p->max_expansion){mulib_mune_result_destroy(res);return MULIB_SHEBANG_SH_LIMIT;}
    if(!txt||memchr(txt,0,n)){
      mulib_shebang_sh_error_(p,"Lua callback returned binary text");
      mulib_mune_result_destroy(res);return MULIB_SHEBANG_SH_INVALID;
    }
    *out=mulib_shebang_sh_copy_bytes_(txt,n);*present=1;
    if(!*out){mulib_mune_result_destroy(res);return MULIB_SHEBANG_SH_NOMEM;}
  }else if(rv.type!=MULIB_MUNE_NIL){
    mulib_shebang_sh_error_(p,"Lua callback must return a string or nil");
    mulib_mune_result_destroy(res);return MULIB_SHEBANG_SH_LUA;
  }
  mulib_mune_result_destroy(res);return 0;
}
#else
int mulib_shebang_sh_lua(mulib_shebang_sh_t *p,const char *s,size_t n){
  (void)p;(void)s;(void)n;return MULIB_SHEBANG_SH_UNSUPPORTED;
}
#endif
int mulib_shebang_sh_lua_file(mulib_shebang_sh_t *p,const char *path){
  FILE *f;long n;size_t got;char *data;int rc;
  if(!p||!path)return MULIB_SHEBANG_SH_INVALID;
  mulib_shebang_sh_error_(p,"cannot read Lua source (maximum 1 MiB)");
  if(!(f=fopen(path,"rb")))return MULIB_SHEBANG_SH_IO;
  if(fseek(f,0,SEEK_END)||(n=ftell(f))<0||n>1048576||fseek(f,0,SEEK_SET)){
    fclose(f);return MULIB_SHEBANG_SH_IO;
  }
  data=(char*)malloc((size_t)n+1);if(!data){fclose(f);return MULIB_SHEBANG_SH_NOMEM;}
  got=fread(data,1,(size_t)n,f);fclose(f);data[got]=0;
  rc=got==(size_t)n?mulib_shebang_sh_lua(p,data,got):MULIB_SHEBANG_SH_IO;
  free(data);return rc;
}
static int mulib_shebang_sh_substitute_(mulib_shebang_sh_t *p,const char *src,char **out){
  char *dest=NULL;size_t len=0,cap=0,i=0;
  while(src[i]){
    if(src[i]=='$'&&src[i+1]=='$'){
      if(!mulib_shebang_sh_append_(&dest,&len,&cap,"$",1,p->max_expansion))goto oom;
      i+=2;continue;
    }
    if((src[i]=='$'&&src[i+1]=='{')||(src[i]=='{'&&src[i+1]=='{')){
      int brace=src[i]=='{';size_t from=i+(brace?2:2),end=from;
      const char *replacement;char *key;size_t n;
      while(src[end]&&src[end]!='}')end++;
      if(src[end]!='}'||(brace&&src[end+1]!='}'))goto copy;
      n=end-from;if(!n||n>256)goto copy;
      key=(char*)malloc(n+1);if(!key){free(dest);return MULIB_SHEBANG_SH_NOMEM;}
      memcpy(key,src+from,n);key[n]=0;
      replacement=mulib_shebang_sh_lookup(p,key);
      if(!replacement){
#ifndef MULIB_SHEBANG_SH_NO_MUNE
        char *lua=NULL;int present=0;
        int rc=mulib_shebang_sh_lua_string_(p,"macro",key,0,&lua,&present);
        if(rc){free(key);free(dest);return rc;}
        if(present){if(!mulib_shebang_sh_append_(&dest,&len,&cap,lua,strlen(lua),p->max_expansion)){
            free(lua);free(key);goto oom;
          }free(lua);free(key);i=end+1+(brace?1:0);continue;}
#endif
        if(p->strict_macros){free(key);free(dest);mulib_shebang_sh_error_(p,"undefined macro");return MULIB_SHEBANG_SH_INVALID;}
        free(key);goto copy;
      }
      free(key);
      if(!mulib_shebang_sh_append_(&dest,&len,&cap,replacement,strlen(replacement),p->max_expansion))goto oom;
      i=end+1+(brace?1:0);continue;
    }
copy:
    if(!mulib_shebang_sh_append_(&dest,&len,&cap,src+i,1,p->max_expansion))goto oom;
    i++;
  }
  if(!dest)dest=mulib_shebang_sh_dup_("");
  if(!dest)return MULIB_SHEBANG_SH_NOMEM;
  *out=dest;return 0;
oom:{int rc=errno==ENOMEM?MULIB_SHEBANG_SH_NOMEM:MULIB_SHEBANG_SH_LIMIT;
  free(dest);mulib_shebang_sh_error_(p,"line exceeded memory or expansion budget");return rc;}
}
static int mulib_shebang_sh_apply_(mulib_shebang_sh_t *p,const mulib_shebang_sh_filter_t *f,
                                   const char *s,size_t number,char **out){
  char *d=NULL;size_t len=0,cap=0,n=strlen(s),i;const char *name=f->name;
  if(n>p->max_expansion)return MULIB_SHEBANG_SH_LIMIT;
  if(!strcmp(name,"trim")){size_t a=0,b=n;
    while(a<b&&isspace((unsigned char)s[a]))a++;
    while(b>a&&isspace((unsigned char)s[b-1]))b--;
    if(!mulib_shebang_sh_append_(&d,&len,&cap,s+a,b-a,p->max_expansion))goto bad;
  }else if(!strcmp(name,"upper")||!strcmp(name,"lower")){
    d=mulib_shebang_sh_dup_(s);if(!d)return MULIB_SHEBANG_SH_NOMEM;
    for(i=0;i<n;i++)d[i]=(char)(!strcmp(name,"upper")?toupper((unsigned char)d[i]):tolower((unsigned char)d[i]));
  }else if(!strcmp(name,"squeeze")){int space=0;
    for(i=0;i<n;i++){int isspace_ = isspace((unsigned char)s[i]);
      if(isspace_&&space)continue;
      {char c=(char)(isspace_?' ':s[i]);if(!mulib_shebang_sh_append_(&d,&len,&cap,&c,1,p->max_expansion))goto bad;}
      space=isspace_;
    }
  }else if(!strcmp(name,"strip-comments")){
    for(i=0;i<n&&s[i]!='#';i++){}
    if(!mulib_shebang_sh_append_(&d,&len,&cap,s,i,p->max_expansion))goto bad;
  }else if(!strcmp(name,"number")){char buf[64];int z=snprintf(buf,sizeof buf,"%zu:",number);
    if(z<0||(size_t)z>=sizeof buf||!mulib_shebang_sh_append_(&d,&len,&cap,buf,(size_t)z,p->max_expansion)||
       !mulib_shebang_sh_append_(&d,&len,&cap,s,n,p->max_expansion))goto bad;
  }else if(!strcmp(name,"prefix")||!strcmp(name,"suffix")){
    const char *arg=f->argument?f->argument:"";
    if(!strcmp(name,"prefix")){if(!mulib_shebang_sh_append_(&d,&len,&cap,arg,strlen(arg),p->max_expansion))goto bad;}
    if(!mulib_shebang_sh_append_(&d,&len,&cap,s,n,p->max_expansion))goto bad;
    if(!strcmp(name,"suffix")){if(!mulib_shebang_sh_append_(&d,&len,&cap,arg,strlen(arg),p->max_expansion))goto bad;}
  }else if(!strcmp(name,"shell-quote")){
    if(!mulib_shebang_sh_append_(&d,&len,&cap,"'",1,p->max_expansion))goto bad;
    for(i=0;i<n;i++){
      if(s[i]=='\''){if(!mulib_shebang_sh_append_(&d,&len,&cap,"'\\''",4,p->max_expansion))goto bad;}
      else if(!mulib_shebang_sh_append_(&d,&len,&cap,s+i,1,p->max_expansion))goto bad;
    }
    if(!mulib_shebang_sh_append_(&d,&len,&cap,"'",1,p->max_expansion))goto bad;
  }else if(!strcmp(name,"json-escape")){
    static const char hex[]="0123456789abcdef";
    for(i=0;i<n;i++){unsigned char c=(unsigned char)s[i];char temp[6];size_t x=0;
      if(c=='"'||c=='\\'){temp[0]='\\';temp[1]=(char)c;x=2;}
      else if(c=='\n'||c=='\r'||c=='\t'){temp[0]='\\';temp[1]=c=='\n'?'n':(c=='\r'?'r':'t');x=2;}
      else if(c<32){temp[0]='\\';temp[1]='u';temp[2]='0';temp[3]='0';temp[4]=hex[c>>4];temp[5]=hex[c&15];x=6;}
      else {temp[0]=(char)c;x=1;}
      if(!mulib_shebang_sh_append_(&d,&len,&cap,temp,x,p->max_expansion))goto bad;
    }
  }else return MULIB_SHEBANG_SH_INVALID;
  if(!d)d=mulib_shebang_sh_dup_("");
  if(!d)return MULIB_SHEBANG_SH_NOMEM;
  *out=d;return 0;
bad:{int rc=errno==ENOMEM?MULIB_SHEBANG_SH_NOMEM:MULIB_SHEBANG_SH_LIMIT;free(d);return rc;}
}
int mulib_shebang_sh_transform(mulib_shebang_sh_t *p,const char *line,size_t number,char **out){
  char *s=NULL;int rc;
  if(out)*out=NULL;
  if(!p||!line||!out||!number)return MULIB_SHEBANG_SH_INVALID;
  p->error[0]=0;
  if(strlen(line)>p->max_line){mulib_shebang_sh_error_(p,"input line exceeds max_line");return MULIB_SHEBANG_SH_LIMIT;}
  rc=mulib_shebang_sh_substitute_(p,line,&s);if(rc)return rc;
  for(size_t i=0;i<p->filters_count;i++){
    char *next=NULL;rc=mulib_shebang_sh_apply_(p,&p->filters[i],s,number,&next);
    free(s);if(rc){mulib_shebang_sh_error_(p,"filter exceeded memory or expansion budget");return rc;}s=next;
  }
#ifndef MULIB_SHEBANG_SH_NO_MUNE
  if(p->lua){char *next=NULL;int present=0;
    rc=mulib_shebang_sh_lua_string_(p,"filter",s,number,&next,&present);
    if(rc){free(s);return rc;}
    if(present){
      if(strlen(next)>p->max_expansion){free(next);free(s);return MULIB_SHEBANG_SH_LIMIT;}
      free(s);s=next;
    }
  }
#endif
  *out=s;return 0;
}
int mulib_shebang_sh_process(mulib_shebang_sh_t *p,FILE *source,FILE *output){
  char *s=NULL;size_t len=0,cap=0,line=0;int c,rc=0;
  if(!p||!source||!output)return MULIB_SHEBANG_SH_INVALID;
  p->error[0]=0;
  while((c=fgetc(source))!=EOF){
    if(c==0){rc=MULIB_SHEBANG_SH_INVALID;
      mulib_shebang_sh_error_(p,"binary NUL byte in text script");break;}
    if(c=='\n'){
      line++;
      if(!(line==1&&p->skip_shebang&&len>=2&&s[0]=='#'&&s[1]=='!')){
        char *result=NULL;
        if(!s){s=mulib_shebang_sh_dup_("");if(!s){rc=MULIB_SHEBANG_SH_NOMEM;break;}}
        rc=mulib_shebang_sh_transform(p,s,line,&result);
        if(rc)break;
        if(fwrite(result,1,strlen(result),output)!=strlen(result)||fputc('\n',output)==EOF)rc=MULIB_SHEBANG_SH_IO;
        free(result);if(rc)break;
      }
      len=0;if(s)s[0]=0;
    }else if(!mulib_shebang_sh_append_(&s,&len,&cap,(char*)&(unsigned char){(unsigned char)c},1,p->max_line)){
      rc=errno==ENOMEM?MULIB_SHEBANG_SH_NOMEM:MULIB_SHEBANG_SH_LIMIT;
      mulib_shebang_sh_error_(p,"input line exceeded memory or line budget");break;
    }
  }
  if(!rc&&ferror(source))rc=MULIB_SHEBANG_SH_IO;
  if(!rc&&len){line++;
    if(!(line==1&&p->skip_shebang&&s[0]=='#'&&s[1]=='!')){
      char *result=NULL;rc=mulib_shebang_sh_transform(p,s,line,&result);
      if(!rc&&fwrite(result,1,strlen(result),output)!=strlen(result))rc=MULIB_SHEBANG_SH_IO;
      free(result);
    }
  }
  if(!rc&&fflush(output))rc=MULIB_SHEBANG_SH_IO;
  if(rc==MULIB_SHEBANG_SH_IO)mulib_shebang_sh_error_(p,"stream read, write or flush failed");
  free(s);return rc;
}
void mulib_shebang_sh_free(mulib_shebang_sh_t *p){
  if(!p)return;
#ifndef MULIB_SHEBANG_SH_NO_MUNE
  if(p->lua)mulib_mune_state_destroy(p->lua);
#endif
  for(size_t i=0;i<p->macros_count;i++){free(p->macros[i].name);free(p->macros[i].value);}
  for(size_t i=0;i<p->filters_count;i++){free(p->filters[i].name);free(p->filters[i].argument);}
  free(p->macros);free(p->filters);memset(p,0,sizeof *p);
}
static int mulib_shebang_sh_cli_split_(const char *s,char ***out,size_t *out_n){
  char **words=NULL,*buf=NULL;size_t count=0,cap=0,len=0,bcap=0;int quote=0,escape=0,token=0;
  for(size_t i=0;;i++){
    unsigned char c=(unsigned char)s[i];
    if(escape){if(!c)goto bad;escape=0;token=1;
      if(!mulib_shebang_sh_append_(&buf,&len,&bcap,(char*)&c,1,65535))goto bad;
      continue;}
    if(c=='\\'&&quote!='\''){escape=1;token=1;continue;}
    if(quote){if(!c)goto bad;if(c==quote){quote=0;continue;}}
    else if(c=='\''||c=='"'){quote=c;token=1;continue;}
    else if(!c||isspace(c)){
      if(token){char **next;char *word=mulib_shebang_sh_dup_(buf?buf:"");if(!word)goto bad;
        if(count==cap){size_t n=cap?cap*2:8;next=(char**)realloc(words,n*sizeof *next);
          if(!next){free(word);goto bad;}words=next;cap=n;}
        words[count++]=word;len=0;if(buf)buf[0]=0;token=0;
      }
      if(!c)break;
      continue;
    }
    token=1;
    if(!mulib_shebang_sh_append_(&buf,&len,&bcap,(char*)&c,1,65535))goto bad;
  }
  free(buf);*out=words;*out_n=count;return 0;
bad:for(size_t i=0;i<count;i++)free(words[i]);free(words);free(buf);return -1;
}
static int mulib_shebang_sh_cli_define_(mulib_shebang_sh_t *p,const char *s){
  const char *eq=strchr(s,'=');char *name;size_t n;int rc;
  if(!eq||eq==s)return -1;
  n=(size_t)(eq-s);name=(char*)malloc(n+1);if(!name)return -1;
  memcpy(name,s,n);name[n]=0;rc=mulib_shebang_sh_define(p,name,eq+1);free(name);return rc;
}
static int mulib_shebang_sh_cli_size_(const char *s,size_t *out){
  size_t n=0;
  if(!s||!*s)return 0;
  for(;*s;s++){
    if(*s<'0'||*s>'9'||n>(SIZE_MAX-(size_t)(*s-'0'))/10)return 0;
    n=n*10+(size_t)(*s-'0');
  }
  *out=n;return 1;
}
/* Linux's optional shebang argument and a standalone argv token can be
 * identical. Only infer a bundle for a separated option/value or another
 * option outside quotes; --shebang-args provides an explicit override. */
static int mulib_shebang_sh_cli_bundle_(const char *s){
  size_t n;int quote=0;
  if(!s||s[0]!='-')return 0;
  n=strcspn(s," \t");
  if(!s[n])return 0;
  if((n==2&&(!strncmp(s,"-D",2)||!strncmp(s,"-F",2)))||
     (n==8&&!strncmp(s,"--define",8))||(n==8&&!strncmp(s,"--filter",8))||
     (n==5&&!strncmp(s,"--lua",5))||(n==6&&!strncmp(s,"--exec",6))||
     (n==10&&!strncmp(s,"--lua-code",10))||
     (n==10&&!strncmp(s,"--max-line",10))||(n==15&&!strncmp(s,"--max-expansion",15)))return 1;
  for(size_t i=0;s[i];i++){
    if(s[i]=='\\'&&quote!='\''&&s[i+1]){i++;continue;}
    if(quote){if(s[i]==quote)quote=0;continue;}
    if(s[i]=='\''||s[i]=='"'){quote=s[i];continue;}
    if(isspace((unsigned char)s[i])){
      while(isspace((unsigned char)s[i+1]))i++;
      if(s[i+1]=='-')return 1;
    }
  }
  return 0;
}
static int mulib_shebang_sh_exec_(const char *program,const char *script,
                                   char **args,int count){
#if MULIB_SHEBANG_SH_POSIX
  char **argv;pid_t pid;int raw=0;
  if(!program||!script||count<0)return 126;
  argv=(char**)calloc((size_t)count+3,sizeof *argv);if(!argv)return 126;
  argv[0]=(char*)program;argv[1]=(char*)script;
  for(int i=0;i<count;i++)argv[i+2]=args[i];
  fflush(NULL);pid=fork();
  if(pid<0){free(argv);return 126;}
  if(pid==0){execvp(program,argv);_exit(127);}
  free(argv);
  while(waitpid(pid,&raw,0)<0){if(errno!=EINTR)return 126;}
  return WIFEXITED(raw)?WEXITSTATUS(raw):WIFSIGNALED(raw)?128+WTERMSIG(raw):126;
#else
  (void)program;(void)script;(void)args;(void)count;return 126;
#endif
}
int mulib_shebang_sh_main(int argc,char **argv){
  mulib_shebang_sh_t p;char **opts=NULL,**expanded=NULL;size_t opts_n=0;
  int k=1,ret=0,script_index=-1,params=0;const char *exec_program=NULL,*lua_file=NULL,*lua_code=NULL;
  FILE *in=NULL,*out=stdout;char *tmpname=NULL;
#if MULIB_SHEBANG_SH_POSIX
  int tempfd=-1;
#endif
  if(argc<2||!argv){fprintf(stderr,"shebang-sh: expected options and a script path\n");return 2;}
  for(int i=0;i<argc;i++)if(!argv[i])return 2;
  if(mulib_shebang_sh_init(&p))return 2;
  /* In a shebang invocation argv[1] is the whole optional argument on Linux;
   * standalone invocations use ordinary argv tokens. */
  if(!strcmp(argv[1],"--shebang-args")||mulib_shebang_sh_cli_bundle_(argv[1])){
    int from=!strcmp(argv[1],"--shebang-args")?3:2;
    if(argc<from||mulib_shebang_sh_cli_split_(argv[from-1],&opts,&opts_n))goto fail;
    if(opts_n>(size_t)INT_MAX-(size_t)argc)goto fail;
    expanded=(char**)calloc(opts_n+(size_t)argc,sizeof *expanded);if(!expanded)goto fail;
    for(size_t i=0;i<opts_n;i++)expanded[i]=opts[i];
    for(int i=from;i<argc;i++)expanded[opts_n+(size_t)i-(size_t)from]=argv[i];
    argc=(int)(opts_n+(size_t)argc-(size_t)from);argv=expanded;k=0;
  }
  for(;k<argc;k++){
    const char *a=argv[k];const char *v=NULL;
    if(!strcmp(a,"--")){k++;break;}
    if(!strcmp(a,"--help")){fprintf(stdout,"shebang-sh: [-F FILTER] [-D NAME=VALUE] [--lua FILE] [--lua-code SOURCE]\n  [--exec PROGRAM] [--strict-macros] [--keep-shebang]\n  [--max-line N] [--max-expansion N] [--] SCRIPT|- [ARGS...]\n  --shebang-args 'OPTIONS' SCRIPT [ARGS...] explicitly splits an option bundle\n");goto clean;}
    if(!strcmp(a,"--strict-macros")){p.strict_macros=1;continue;}
    if(!strcmp(a,"--keep-shebang")){p.skip_shebang=0;continue;}
    if(!strcmp(a,"-F")||!strcmp(a,"--filter")||!strcmp(a,"-D")||!strcmp(a,"--define")||!strcmp(a,"--lua")||!strcmp(a,"--exec")||!strcmp(a,"--lua-code")||!strcmp(a,"--max-line")||!strcmp(a,"--max-expansion")){
      if(k+1>=argc)goto fail;
      v=argv[++k];
    }else if(!strncmp(a,"--filter=",9))v=a+9;
    else if(!strncmp(a,"--define=",9))v=a+9;
    else if(!strncmp(a,"--lua=",6))v=a+6;
    else if(!strncmp(a,"--lua-code=",11))v=a+11;
    else if(!strncmp(a,"--exec=",7))v=a+7;
    else if(!strncmp(a,"--max-line=",11))v=a+11;
    else if(!strncmp(a,"--max-expansion=",16))v=a+16;
    else if(!strncmp(a,"-F",2)&&a[2])v=a+2;
    else if(!strncmp(a,"-D",2)&&a[2])v=a+2;
    else if(a[0]=='-'&&a[1])goto fail;
    else break;
    if(!strcmp(a,"--max-line")||!strncmp(a,"--max-line=",11)){
      if(!mulib_shebang_sh_cli_size_(v,&p.max_line))goto fail;
      continue;
    }
    if(!strcmp(a,"--max-expansion")||!strncmp(a,"--max-expansion=",16)){
      if(!mulib_shebang_sh_cli_size_(v,&p.max_expansion))goto fail;
      continue;
    }
    if(!strcmp(a,"--lua-code")||!strncmp(a,"--lua-code=",11)){lua_code=v;continue;}
    if(!strcmp(a,"--lua")||!strncmp(a,"--lua=",6)){lua_file=v;continue;}
    if(!strcmp(a,"--exec")||!strncmp(a,"--exec=",7)){exec_program=v;continue;}
    if(!strcmp(a,"-D")||!strcmp(a,"--define")||!strncmp(a,"--define=",9)||!strncmp(a,"-D",2)){
      if(mulib_shebang_sh_cli_define_(&p,v))goto fail;
    }else if(mulib_shebang_sh_add_filter(&p,v))goto fail;
  }
  if(k>=argc)goto fail;
  script_index=k;params=argc-k-1;
  if(mulib_shebang_sh_define(&p,"0",argv[script_index]))goto fail;
  for(int i=0;i<params;i++){char name[32];snprintf(name,sizeof name,"%d",i+1);
    if(mulib_shebang_sh_define(&p,name,argv[script_index+i+1]))goto fail;
  }
  if(lua_file&&mulib_shebang_sh_lua_file(&p,lua_file))goto fail;
  if(lua_code&&mulib_shebang_sh_lua(&p,lua_code,strlen(lua_code)))goto fail;
   in=!strcmp(argv[script_index],"-")?stdin:fopen(argv[script_index],"rb");
   if(!in){mulib_shebang_sh_error_(&p,strerror(errno));goto fail;}
  if(exec_program){
#if MULIB_SHEBANG_SH_POSIX
    const char *dir=getenv("TMPDIR");size_t n;
    if(!dir||!*dir)dir="/tmp";
    n=strlen(dir);
    if(n>SIZE_MAX-32)goto fail;
    tmpname=(char*)malloc(n+32);if(!tmpname)goto fail;
    snprintf(tmpname,n+32,"%s/mulib-shebang-XXXXXX",dir);
    tempfd=mkstemp(tmpname);if(tempfd<0)goto fail;
    if(fcntl(tempfd,F_SETFD,FD_CLOEXEC)<0)goto fail;
    out=fdopen(tempfd,"wb");if(!out)goto fail;tempfd=-1;
#else
    goto fail;
#endif
  }
  ret=mulib_shebang_sh_process(&p,in,out);
  if(in!=stdin&&fclose(in)&&!ret)ret=MULIB_SHEBANG_SH_IO;
  in=NULL;
  if(exec_program){if(fclose(out))ret=MULIB_SHEBANG_SH_IO;out=stdout;
    if(!ret)ret=mulib_shebang_sh_exec_(exec_program,tmpname,argv+script_index+1,params);
#if MULIB_SHEBANG_SH_POSIX
    unlink(tmpname);
#endif
  }
  if(ret)fprintf(stderr,"shebang-sh: processing/child status (%d): %s\n",ret,p.error);
  ret=ret<0?1:ret;goto clean;
fail:fprintf(stderr,"shebang-sh: invalid option or input: %s\n",p.error);ret=2;
clean:
  if(in&&in!=stdin)fclose(in);
#if MULIB_SHEBANG_SH_POSIX
  if(tempfd>=0){close(tempfd);
    unlink(tmpname);
  }
  if(exec_program&&out!=stdout){fclose(out);unlink(tmpname);}
#endif
  free(tmpname);
  mulib_shebang_sh_free(&p);
  for(size_t i=0;i<opts_n;i++)free(opts[i]);
  free(opts);free(expanded);
  return ret;
}
#endif
