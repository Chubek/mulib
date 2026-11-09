#include "../examples/android_runtime_setup.h"
#define MULIB_IMPL_EZJNI
#include "mulib-ezjni.h"
#include <assert.h>
#include <string.h>
static uint32_t random_state = UINT32_C (0x12345678);
static uint32_t random_word (void)
{ random_state = random_state * UINT32_C (1664525) + UINT32_C (1013904223); return random_state; }
static void put32 (unsigned char *p, uint32_t v)
{ unsigned k; for (k = 0; k < 4; ++k) p[k] = (unsigned char)(v >> (k * 8)); }
static void corrupted_dex (void)
{
  mulib_dexcc_instruction_t code[2]; mulib_dexcc_method_t method;
  mulib_buffer_t file = {0}; unsigned char bytes[1024]; size_t k, j;
  code[0] = mulib_dexcc_instruction (MULIB_DEXCC_CONST); code[0].literal = 42;
  code[1] = mulib_dexcc_instruction (MULIB_DEXCC_RETURN);
  method.code = code; method.count = 2; method.registers = 1; method.parameters = 0;
  assert (mulib_dexcc_file (&method,"Ltest/Hostile;","answer",100000,&file,NULL) == MULIB_DEXCC_OK);
  assert (file.length < sizeof (bytes));
  for (k = 0; k < file.length; ++k) assert (mulib_dexcc_check_file (file.data,k,100000,NULL) != MULIB_DEXCC_OK);
  for (k = 0; k < 1200; ++k) {
    mulib_art_method_t *m = NULL;
    memcpy (bytes,file.data,file.length);
    for (j = 0; j < 4; ++j) bytes[32 + random_word () % (file.length - 32)] = (unsigned char)random_word ();
    assert (mulib_authcrypto_hash (MULIB_AUTHCRYPTO_SHA1,bytes + 32,file.length - 32,bytes + 12,20));
    put32 (bytes + 8,mulib_adler32 (bytes + 12,file.length - 12));
    (void)mulib_art_load_dex (bytes,file.length,"Ltest/Hostile;","answer",0,100000,&m,NULL);
    assert (mulib_art_destroy (&m) == MULIB_MIR_OK);
  }
  mulib_buffer_free (&file);
}
static void random_code (void)
{
  unsigned char bytes[40]; size_t k, j;
  for (k = 0; k < 3000; ++k) {
    size_t n = random_word () % sizeof (bytes);
    mulib_art_method_t *art = NULL; mulib_jvm_method_t *jvm = NULL;
    mulib_dexcc_buffer_t decoded = {0}; mulib_ezjni_signature_t signature;
    mulib_jvm_code_t c;
    for (j = 0; j < n; ++j) bytes[j] = (unsigned char)random_word ();
    (void)mulib_dexcc_decode (bytes,n,4,0,4096,&decoded,NULL); mulib_dexcc_free (&decoded);
    (void)mulib_art_load_code (bytes,n,4,0,4096,&art,NULL);
    assert (mulib_art_destroy (&art) == MULIB_MIR_OK);
    memset (&c,0,sizeof (c)); c.bytes = bytes; c.length = n; c.max_locals = c.max_stack = 4;
    (void)mulib_jvm_load_code (&c,4096,&jvm,NULL); assert (mulib_jvm_destroy (&jvm) == MULIB_MIR_OK);
    bytes[n] = 0;
    (void)mulib_ezjni_signature ((const char *)bytes,&signature);
    if (n >= 10) {
      memcpy (bytes,"\xca\xfe\xba\xbe\0\0\0\x31\0\x08",10);
      (void)mulib_jvm_load_class (bytes,n,"test","()I",4096,&jvm,NULL);
      assert (mulib_jvm_destroy (&jvm) == MULIB_MIR_OK);
    }
  }
}
int main (void) { corrupted_dex (); random_code (); return 0; }
