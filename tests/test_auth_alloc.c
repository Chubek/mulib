/* Auth allocation failures and credential erasure, including nested allocations. */
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static unsigned attempts, fail_at, live;
static struct { void *pointer; size_t size; } allocations[2];
static void *
auth_malloc (size_t size)
{
  void *p;
  if (++attempts==fail_at) return NULL;
  p=malloc(size);
  assert(p);
  for (unsigned i=0; i<2; ++i) if (!allocations[i].pointer) {
    allocations[i].pointer=p; allocations[i].size=size; ++live;
    /* Expose incomplete wipes, including uninitialized parts of allocations. */
    memset(p,0xa5,size);
    return p;
  }
  assert(0 && "unexpected auth allocation");
  return NULL;
}
static void
auth_free (void *p)
{
  for (unsigned i=0; i<2; ++i) if (allocations[i].pointer==p) {
    const unsigned char *bytes=(const unsigned char *)p;
    for (size_t j=0; j<allocations[i].size; ++j) assert(bytes[j]==0);
    allocations[i].pointer=NULL; --live; free(p); return;
  }
  assert(0 && "untracked auth free");
}
#define MULIB_IMPL_AUTH
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_SECURITY
#define malloc auth_malloc
#define free auth_free
#include "mulib-auth.h"
#undef malloc
#undef free

static int called;
static int
verify (void *context, const char *user, size_t un, const char *password, size_t pn)
{
  (void)context;
  assert(un==1 && user[0]=='a' && pn==1 && password[0]=='b');
  ++called;
  return 1;
}
int main (void)
{
  char out[8]="keep";
  size_t n=99;
  attempts=0; fail_at=1;
  assert(!mulib_auth_basic("Basic YTpi",out,sizeof out,&n));
  assert(!strcmp(out,"keep") && n==99 && !live);
  for (unsigned failure=1; failure<=2; ++failure) {
    attempts=0; fail_at=failure; called=0;
    assert(!mulib_auth_basic_authorize("Basic YTpi",verify,NULL));
    assert(!called && !live);
  }
  attempts=0; fail_at=0;
  assert(mulib_auth_basic_authorize("Basic YTpi",verify,NULL));
  assert(called==1 && !live);
  assert(!mulib_auth_basic("Basic YToK",out,sizeof out,&n) && !live);
  assert(!mulib_auth_basic("Basic YTpi",out,2,&n) && !live);
  assert(!mulib_auth_basic("Basic ????",out,sizeof out,&n) && !live);
  assert(!strcmp(out,"keep") && n==99);
  return 0;
}
