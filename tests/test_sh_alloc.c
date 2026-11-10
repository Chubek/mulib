#include <stdlib.h>
#include <assert.h>
static size_t calls,fail_at,live;
static void *allocate(size_t n){
  void *p;if(++calls==fail_at)return NULL;p=malloc(n);if(p)live++;return p;
}
static void *zero_allocate(size_t n,size_t z){
  void *p;if(++calls==fail_at)return NULL;p=calloc(n,z);if(p)live++;return p;
}
static void *resize(void *p,size_t n){
  int was_null=p==NULL;void *q;
  if(++calls==fail_at)return NULL;
  q=realloc(p,n);if(q&&was_null)live++;return q;
}
static void release(void *p){if(p){assert(live);live--;}free(p);}
#define malloc allocate
#define calloc zero_allocate
#define realloc resize
#define free release
#define MULIB_PARALLEL_SH_NO_MUNE
#define MULIB_SHEBANG_SH_NO_MUNE
#define MULIB_IMPL_PARALLEL_SH
#define MULIB_IMPL_SHEBANG_SH
#include "../include/mulib-parallel-sh.h"
#include "../include/mulib-shebang-sh.h"

int main(void){
  size_t limit=1;
  for(size_t at=1;at<=limit;at++){
    mulib_parallel_sh_t p;mulib_parallel_sh_channel_t *c;int rc;
    calls=0;fail_at=at;
    assert(!mulib_parallel_sh_init(&p,1));
    for(int i=0;i<20;i++){
      rc=mulib_parallel_sh_add_input(&p,"echo {/.} {}","dir/file.ext");
      assert(rc==0||rc==MULIB_PARALLEL_SH_NOMEM);
    }
    rc=mulib_parallel_sh_channel_create(&p,"channel",&c);
    assert(rc==0||rc==MULIB_PARALLEL_SH_NOMEM);
    if(!rc){
      for(int i=0;i<20;i++){
        rc=mulib_parallel_sh_channel_send(c,"message");
        assert(rc==0||rc==MULIB_PARALLEL_SH_NOMEM);
      }
    }
    if(calls+1>limit)limit=calls+1;
    mulib_parallel_sh_free(&p);assert(!live);
  }
  limit=1;
  for(size_t at=1;at<=limit;at++){
    mulib_shebang_sh_t p;char *out=NULL;int rc;
    calls=0;fail_at=at;
    assert(!mulib_shebang_sh_init(&p));
    rc=mulib_shebang_sh_define(&p,"X","a'b");
    assert(rc==0||rc==MULIB_SHEBANG_SH_NOMEM);
    for(int i=0;i<12;i++){
      rc=mulib_shebang_sh_add_filter(&p,"prefix:abc");
      assert(rc==0||rc==MULIB_SHEBANG_SH_NOMEM);
    }
    rc=mulib_shebang_sh_add_filter(&p,"shell-quote");
    assert(rc==0||rc==MULIB_SHEBANG_SH_NOMEM);
    rc=mulib_shebang_sh_transform(&p,"${X}",1,&out);
    assert(rc==0||rc==MULIB_SHEBANG_SH_NOMEM);
    if(rc)assert(!out);
    free(out);
    if(calls+1>limit)limit=calls+1;
    mulib_shebang_sh_free(&p);assert(!live);
  }
  return 0;
}
