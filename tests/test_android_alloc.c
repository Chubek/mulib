#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static size_t fail_at, calls, live;
static void *checked_malloc (size_t n)
{ void *p; if (++calls == fail_at) return NULL; p = malloc (n); if (p) ++live; return p; }
static void *checked_calloc (size_t n, size_t size)
{ void *p; assert (!size || n <= SIZE_MAX / size); p = checked_malloc (n * size); if (p) memset (p,0,n * size); return p; }
static void *checked_realloc (void *old, size_t n)
{ void *p; int had = old != NULL; assert (n); if (++calls == fail_at) return NULL; p = realloc (old,n); if (p && !had) ++live; return p; }
static void checked_free (void *p) { if (p) { assert (live); --live; } free (p); }
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#include "../examples/android_runtime_setup.h"
#undef malloc
#undef calloc
#undef realloc
#undef free
int main (void)
{
  size_t action, k;
  const unsigned char java[] = {0x1a,0x1b,0x60,0xac};
  const unsigned char dex[] = {0x14,0,42,0,0,0,0x0f,0};
  mulib_dexcc_instruction_t code[2]; mulib_dexcc_method_t m;
  code[0] = mulib_dexcc_instruction (MULIB_DEXCC_CONST); code[0].literal = 42;
  code[1] = mulib_dexcc_instruction (MULIB_DEXCC_RETURN);
  m.code = code; m.count = 2; m.registers = 1; m.parameters = 0;
  for (action = 0; action < 4; ++action) {
    int success = 0;
    for (k = 1; k < 1000; ++k) {
      mulib_jvm_method_t *method = NULL; mulib_buffer_t file = {0}; mulib_dexcc_buffer_t decoded = {0};
      mulib_mir_status_t s = MULIB_MIR_OK; mulib_dexcc_status_t ds = MULIB_DEXCC_OK;
      mulib_jvm_code_t c = {java,sizeof (java),2,2,2,NULL,NULL,0};
      calls = 0; fail_at = k;
      if (action == 0) s = mulib_jvm_load_code (&c,100000,&method,NULL);
      if (action == 1) s = mulib_art_load_code (dex,sizeof (dex),1,0,100000,&method,NULL);
      if (action == 2) ds = mulib_dexcc_file (&m,"Ltest/Alloc;","answer",100000,&file,NULL);
      if (action == 3) ds = mulib_dexcc_decode (dex,sizeof (dex),1,0,100000,&decoded,NULL);
      success = s == MULIB_MIR_OK && ds == MULIB_DEXCC_OK;
      if (!success) assert ((s == MULIB_MIR_NO_MEMORY || ds == MULIB_DEXCC_NOMEM) && !method && !file.data && !decoded.code);
      assert (mulib_art_destroy (&method) == MULIB_MIR_OK); mulib_buffer_free (&file); mulib_dexcc_free (&decoded);
      assert (!live); if (success) break;
    }
    assert (success && k > 5);
  }
  if (mulib_jit_supported ()) {
    mulib_art_method_t *method = NULL;
    size_t baseline;
    fail_at = 0;
    assert (mulib_art_compile (&m,100000,&method,NULL) == MULIB_MIR_OK);
    baseline = live;
    /* Both the native owner allocation and nested MIR image allocation must
     * report OOM even when the caller does not request diagnostic details. */
    for (k = 1; k <= 2; ++k) {
      calls = 0; fail_at = k;
      assert (mulib_art_jit (method,NULL) == MULIB_MIR_NO_MEMORY && live == baseline);
    }
    fail_at = 0;
    assert (mulib_art_destroy (&method) == MULIB_MIR_OK && !live);
  }
  return 0;
}
