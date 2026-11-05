/* Load dependencies normally, then inject failures only into the new modules. */
#define MULIB_DISTRIBCOMP_IMPLEMENTED
#define MULIB_DISTRIBTASK_IMPLEMENTED
#define MULIB_MUMQ_IMPLEMENTED
#define MULIB_MUMQ_SERVER_IMPLEMENTED
#define MULIB_AES_IMPLEMENTED
#include "../examples/distrib-common.h"
#undef MULIB_DISTRIBCOMP_IMPLEMENTED
#undef MULIB_DISTRIBTASK_IMPLEMENTED
#undef MULIB_MUMQ_IMPLEMENTED
#undef MULIB_MUMQ_SERVER_IMPLEMENTED
#undef MULIB_AES_IMPLEMENTED
static long budget=-1;
static void *allocate(size_t n) {
  if (!budget) return NULL;
  if (budget>0) --budget;
  return malloc(n);
}
static void *zero_allocate(size_t n, size_t size) {
  if (!budget) return NULL;
  if (budget>0) --budget;
  return calloc(n,size);
}
#define malloc allocate
#define calloc zero_allocate
#include "mulib-aes.h"
#include "mulib-mumq.h"
#include "mulib-mumq-server.h"
#include "mulib-distribtask.h"
#include "mulib-distribcomp.h"
#undef malloc
#undef calloc
int main(void) {
  const char config[]="{\"cluster\":\"x\",\"nodes\":[\"a\"]}";
  const char source[]="distrib_task('x','a','mune','run','return 1')";
  unsigned char key[64]={0},nonce[12]={0},out[4]={9,9,9,9},tag[32];
  int success=0;
  for (long i=0;i<10;++i) {
    mulib_distribtask_error_t e; mulib_distribtask_plan_t *p;
    budget=i; p=mulib_distribtask_load(config,sizeof config-1,0,source,sizeof source-1,NULL,0,&e);
    if (p) { success=1; mulib_distribtask_destroy(p); break; }
    assert(e.code);
  }
  assert(success); budget=0;
  assert(!mulib_aes_seal(key,nonce,NULL,0,"test",4,out,tag)&&out[0]==9);
  budget=-1;
  {
    mulib_socket_t a=MULIB_SOCKET_INIT,b=MULIB_SOCKET_INIT; mulib_mumq_t *q;
    assert(!mulib_socket_pair(&a,&b)); budget=0;
    assert(!mulib_mumq_create(&a,NULL)&&a.fd>=0); budget=-1;
    q=mulib_mumq_create(&a,NULL); assert(q); budget=0;
    assert(mulib_mumq_send(q,1,"x","y",1)==MULIB_MUMQ_MEMORY); budget=-1;
    assert(!mulib_mumq_send(q,1,"x","y",1)&&q->tx==1);
    mulib_mumq_destroy(q); assert(!mulib_socket_close(&b));
  }
  return 0;
}
