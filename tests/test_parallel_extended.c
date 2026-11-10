#define MULIB_PARALLEL_SH_NO_MUNE
#define MULIB_IMPL_PARALLEL_SH
#include "../include/mulib-parallel-sh.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
struct ctx {mulib_parallel_sh_channel_t *c;};
static void *sender(void *arg){struct ctx *ctx=(struct ctx*)arg;
  assert(mulib_parallel_sh_channel_send(ctx->c,"message")==0);
  assert(mulib_parallel_sh_channel_close(ctx->c)==0);return NULL;
}
int main(void){mulib_parallel_sh_t p;mulib_parallel_sh_channel_t *c;
  pthread_t thread;struct ctx ctx;char *out=NULL;
  assert(mulib_parallel_sh_init(&p,3)==0);
  assert(mulib_parallel_sh_channel_create(&p,"bus",&c)==0);
  ctx.c=c;assert(!pthread_create(&thread,NULL,sender,&ctx));
  assert(mulib_parallel_sh_channel_receive_wait(c,&out,1000)==0);
  assert(!strcmp(out,"message"));free(out);
  assert(!pthread_join(thread,NULL));
  assert(mulib_parallel_sh_channel_receive_wait(c,&out,0)==MULIB_PARALLEL_SH_CLOSED);
  assert(mulib_parallel_sh_add(&p,"false")==0);
  assert(mulib_parallel_sh_add(&p,"true")==0);
  p.retries=1;p.halt_on_error=1;
  assert(mulib_parallel_sh_run(&p)==MULIB_PARALLEL_SH_FAILED);
  assert(p.failed>=1);assert(p.jobs[0].attempts==2);
  mulib_parallel_sh_free(&p);return 0;
}
