#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
static size_t calls, fail_at = SIZE_MAX, live;
static void *arch_malloc (size_t n)
{ void *p; if (calls++ == fail_at) return NULL; p = malloc(n); if(p) ++live; return p; }
static void *arch_calloc (size_t n, size_t size)
{ void *p; if (calls++ == fail_at) return NULL; p = calloc(n,size); if(p) ++live; return p; }
static void *arch_realloc (void *old, size_t n)
{
  void *p; int fresh = old == NULL;
  assert(n); if(calls++ == fail_at) return NULL;
  p = realloc(old,n); if(p && fresh) ++live; return p;
}
static void arch_free (void *p)
{ if(p) { assert(live); --live; free(p); } }
#define malloc arch_malloc
#define calloc arch_calloc
#define realloc arch_realloc
#define free arch_free
#define main archml_baseline_main
#include "test_archml.c"
#undef main
#undef malloc
#undef calloc
#undef realloc
#undef free
int main (void)
{
  const char source[] = "(archml 1 t (target t (bits 64) (endian little))"
    "(feature a) (feature b (requires (ref feature a)))"
    "(x-data deep (tree (a (b (c (d (e (f (g (h (i (j (k (l (m (n (o (p x)))))))))))))))))))";
  mulib_archml_error_t e;
  mulib_archml_t *a = NULL;
  size_t allocations;
  assert (mulib_archml_parse(source,sizeof(source)-1,NULL,&a,&e) == MULIB_ARCHML_OK);
  allocations = calls; mulib_archml_destroy(a); assert(!live);
  for(size_t i=0;i<allocations;++i) {
    calls=0; fail_at=i; a=NULL;
    assert (mulib_archml_parse(source,sizeof(source)-1,NULL,&a,&e) == MULIB_ARCHML_NOMEM);
    assert (!a && e.code == MULIB_ARCHML_NOMEM && !live);
  }
  {
    FILE *f=fopen("archml-alloc-fixture.aml","wb"); assert(f);
    assert(fwrite(source,1,sizeof(source)-1,f)==sizeof(source)-1 && !fclose(f));
    calls=0; fail_at=SIZE_MAX; a=NULL;
    assert(mulib_archml_load("archml-alloc-fixture.aml",NULL,&a,&e)==MULIB_ARCHML_OK);
    allocations=calls; mulib_archml_destroy(a); assert(!live);
    for(size_t i=0;i<allocations;++i) {
      calls=0; fail_at=i; a=NULL;
      assert(mulib_archml_load("archml-alloc-fixture.aml",NULL,&a,&e)==MULIB_ARCHML_NOMEM);
      assert(!a && !live);
    }
    assert(!remove("archml-alloc-fixture.aml"));
  }
  /* Exhaust the explicit PARSEC stack/capture allocator at every growth. */
  mulib_parsec_parser_t chain[160]; mulib_parsec_result_t result;
  mulib_parsec_options_t options = {200,1000,200};
  chain[159]=mulib_parsec_byte('x');
  for(size_t i=159;i;--i) chain[i-1]=mulib_parsec_capture(&chain[i],i);
  calls=0; fail_at=SIZE_MAX;
  assert(mulib_parsec_run(chain,"x",1,&options,&result)==1);
  allocations=calls; mulib_parsec_result_free(&result); assert(!live);
  for(size_t i=0;i<allocations;++i) {
    calls=0; fail_at=i;
    assert(mulib_parsec_run(chain,"x",1,&options,&result)==-1);
    assert(result.error.code==MULIB_PARSEC_MEMORY && !result.captures && !result.count && !live);
  }
  puts("ArchML/PARSEC allocation failures: cleanup and outputs OK");
  return 0;
}
