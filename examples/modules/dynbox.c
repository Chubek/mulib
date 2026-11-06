#define MULIB_IMPL_DYNBOX
#include "mulib-dynbox.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static double multiply(int32_t a, double b) { return a * b; }
static void add_callback(const mulib_dynbox_cif_t *c, void *result, void *const *args, void *user) {
  (void)c; *(int32_t *)result = *(int32_t *)args[0] + *(int32_t *)user;
}
int main(void) {
  const mulib_dynbox_type_t *types[] = {&mulib_dynbox_type_sint32,&mulib_dynbox_type_double};
  mulib_dynbox_cif_t *c = NULL;
  mulib_dynbox_closure_t *closure = NULL;
  int32_t a = 6, extra = 4;
  double b = 1.25, result;
  const void *args[] = {&a,&b};
  mulib_dynbox_fn entry;
  int32_t (*add)(int32_t);
  if (!mulib_dynbox_supported()) { puts("dynbox: native ABI unavailable"); return 0; }
  assert(!mulib_dynbox_prepare(&c,&mulib_dynbox_type_double,2,types));
  assert(!mulib_dynbox_call(c,(mulib_dynbox_fn)multiply,&result,sizeof result,args));
  assert(result == 7.5); printf("native result: %.2f\n",result);
  assert(!mulib_dynbox_destroy(c)); c = NULL;
  assert(!mulib_dynbox_prepare(&c,types[0],1,types));
  assert(!mulib_dynbox_closure_create(&closure,c,add_callback,&extra));
  entry = mulib_dynbox_closure_function(closure); memcpy(&add,&entry,sizeof add);
  assert(add(8) == 12);
  assert(!mulib_dynbox_closure_destroy(closure)); assert(!mulib_dynbox_destroy(c));
  return 0;
}
