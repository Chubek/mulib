#include "../examples/android_runtime_setup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int writer (void *user, const char *bytes, size_t n)
{ return fwrite (bytes, 1, n, (FILE *)user) == n; }
static void arithmetic (void)
{
  const unsigned char java[] = {0x1a, 0x1b, 0x60, 0xac}; /* (a+b) */
  mulib_jvm_code_t code = {java, sizeof (java), 2, 2, 2, NULL, NULL, 0};
  mulib_jvm_method_t *m = NULL; mulib_mir_diagnostic_t d;
  const int32_t args[] = {INT32_MAX, 1}; int32_t result = 123;
  assert (mulib_jvm_load_code (&code, 100000, &m, &d) == MULIB_MIR_OK);
  assert (mulib_jvm_graph (m)->graph.node_count);
  assert (mulib_jvm_invoke (m, args, 2, 100000, &result) == MULIB_MIR_FINISHED && result == INT32_MIN);
  if (mulib_jit_supported ()) {
    assert (mulib_jvm_jit (m, &d) == MULIB_MIR_OK);
    result = 123;
    assert (mulib_jvm_invoke (m, args, 2, 100000, &result) == MULIB_MIR_FINISHED && result == INT32_MIN);
  }
  assert (mulib_jvm_destroy (&m) == MULIB_MIR_OK && !m);
}
static int32_t as_int (uint32_t v)
{ return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)(UINT32_MAX - v); }
static int32_t expected (unsigned op, int32_t a, int32_t b)
{
  uint32_t x = (uint32_t)a, y = (uint32_t)b, count = y & 31;
  switch (op) {
    case 0x60: return as_int (x + y); case 0x64: return as_int (x - y); case 0x68: return as_int (x * y);
    case 0x6c: return a == INT32_MIN && b == -1 ? INT32_MIN : a / b;
    case 0x70: return a == INT32_MIN && b == -1 ? 0 : a % b;
    case 0x78: return as_int (x << count);
    case 0x7a: return as_int ((x >> count) | (a < 0 && count ? UINT32_MAX << (32 - count) : 0));
    case 0x7c: return as_int (x >> count); case 0x7e: return as_int (x & y);
    case 0x80: return as_int (x | y); default: return as_int (x ^ y);
  }
}
static void differential (void)
{
  static const unsigned char ops[] = {0x60,0x64,0x68,0x6c,0x70,0x78,0x7a,0x7c,0x7e,0x80,0x82};
  static const int32_t values[] = {INT32_MIN, INT32_MAX, -1, 0, 1, 31, 32, 63};
  size_t k, a, b; unsigned backend;
  for (k = 0; k < sizeof (ops); ++k) {
    unsigned char java[] = {0x1a,0x1b,0,0xac}; mulib_jvm_method_t *m = NULL;
    mulib_jvm_code_t c = {java,sizeof (java),2,2,2,NULL,NULL,0}; java[2] = ops[k];
    assert (mulib_jvm_load_code (&c, 100000, &m, NULL) == MULIB_MIR_OK);
    for (backend = 0; backend < 2; ++backend) {
      if (backend) { if (!mulib_jit_supported ()) break; assert (mulib_jvm_jit (m, NULL) == MULIB_MIR_OK); }
      for (a = 0; a < sizeof (values) / sizeof (*values); ++a)
        for (b = 0; b < sizeof (values) / sizeof (*values); ++b) {
          int32_t args[] = {values[a], values[b]}, result = 123;
          mulib_mir_status_t s = mulib_jvm_invoke (m, args, 2, 100000, &result);
          if ((ops[k] == 0x6c || ops[k] == 0x70) && !args[1]) assert (s == MULIB_MIR_DIVIDE_BY_ZERO && result == 123);
          else assert (s == MULIB_MIR_FINISHED && result == expected (ops[k], args[0], args[1]));
        }
    }
    assert (mulib_jvm_destroy (&m) == MULIB_MIR_OK);
  }
}
static void loops (void)
{
  static const unsigned char java[] = {0x03,0x3c,0x03,0x3d,0x1c,0x1a,0xa2,0x00,0x0d,
    0x1b,0x1c,0x60,0x3c,0x84,0x02,0x01,0xa7,0xff,0xf4,0x1b,0xac};
  mulib_jvm_code_t c = {java,sizeof (java),3,2,1,NULL,NULL,0}; mulib_jvm_method_t *m = NULL;
  mulib_mir_frame_t f = {0}; int32_t arg = 10, result = -1;
  assert (mulib_jvm_load_code (&c, 1000000, &m, NULL) == MULIB_MIR_OK);
  assert (mulib_jvm_frame_init (m, &arg, 1, &f) == MULIB_MIR_OK);
  assert (mulib_jvm_run (m, &f, 0) == MULIB_MIR_STEP_LIMIT && !f.steps);
  assert (mulib_jvm_run (m, &f, 1) == MULIB_MIR_STEP_LIMIT && f.steps == 1);
  assert (mulib_jvm_run (m, &f, 100000) == MULIB_MIR_FINISHED && f.result.integer == 45);
  assert (mulib_jvm_run (m, &f, 0) == MULIB_MIR_FINISHED);
  mulib_mir_frame_free (&f);
  if (mulib_jit_supported ()) { assert (mulib_jvm_jit (m, NULL) == MULIB_MIR_OK); assert (mulib_jvm_invoke (m, &arg, 1, 100000, &result) == MULIB_MIR_FINISHED && result == 45); }
  assert (mulib_jvm_destroy (&m) == MULIB_MIR_OK);
}
static void hostile (void)
{
  const unsigned char underflow[] = {0x60,0xac}, target[] = {0xa7,0,1}, object[] = {0x01,0xac},
    uninit[] = {0x1b,0xac}, truncated[] = {0x11,0}, invalid_join[] = {0x03,0x99,0,4,0x04,0xac};
  const unsigned char *samples[] = {underflow,target,object,uninit,truncated,invalid_join};
  const size_t lengths[] = {sizeof (underflow),sizeof (target),sizeof (object),sizeof (uninit),sizeof (truncated),sizeof (invalid_join)};
  mulib_jvm_method_t *m = NULL; size_t k;
  for (k = 0; k < 6; ++k) {
    mulib_jvm_code_t c = {samples[k], lengths[k], 2, 2, 0, NULL, NULL, 0};
    assert (mulib_jvm_load_code (&c, 100000, &m, NULL) != MULIB_MIR_OK && !m);
  }
  assert (mulib_jvm_load_code (NULL, SIZE_MAX, &m, NULL) == MULIB_MIR_INVALID && !m);
  {
    unsigned char raw[] = {0x0f,0}; mulib_dexcc_buffer_t b = {0};
    assert (mulib_dexcc_decode (raw,sizeof (raw),1,0,1000,&b,NULL) == MULIB_DEXCC_UNINITIALIZED && !b.code);
    raw[0] = 0x2a;
    assert (mulib_dexcc_decode (raw,sizeof (raw),1,0,1000,&b,NULL) == MULIB_DEXCC_RANGE && !b.code);
  }
}
static void dex (const char *path)
{
  mulib_dexcc_instruction_t code[2]; mulib_dexcc_method_t method;
  mulib_buffer_t file = {0}; mulib_art_method_t *m = NULL; int32_t result;
  code[0] = mulib_dexcc_instruction (MULIB_DEXCC_CONST); code[0].literal = 42;
  code[1] = mulib_dexcc_instruction (MULIB_DEXCC_RETURN);
  method.code = code; method.count = 2; method.registers = 1; method.parameters = 0;
  assert (mulib_dexcc_file (&method, "Ltest/Answer;", "answer", 100000, &file, NULL) == MULIB_DEXCC_OK);
  assert (mulib_dexcc_check_file (file.data, file.length, 100000, NULL) == MULIB_DEXCC_OK);
  assert (mulib_art_load_dex (file.data, file.length, "Ltest/Answer;", "answer", 0, 100000, &m, NULL) == MULIB_MIR_OK);
  assert (mulib_art_invoke (m, NULL, 0, 100000, &result) == MULIB_MIR_FINISHED && result == 42);
  if (path) { FILE *f = fopen (path, "wb"); assert (f); assert (fwrite (file.data, 1, file.length, f) == file.length); assert (!fclose (f)); }
  assert (mulib_art_destroy (&m) == MULIB_MIR_OK);
  mulib_buffer_free (&file);
}
static void dex_parameters (void)
{
  mulib_dexcc_instruction_t code[2]; mulib_dexcc_method_t method;
  mulib_buffer_t bytes = {0}, file = {0}; mulib_dexcc_buffer_t decoded = {0};
  mulib_art_method_t *m = NULL; const int32_t args[] = {20,22}; int32_t result = -1;
  const unsigned char literal[] = {0xd8,0,0,1,0x0f,0};
  code[0] = mulib_dexcc_instruction (MULIB_DEXCC_ADD); code[0].b = 1; code[0].c = 2;
  code[1] = mulib_dexcc_instruction (MULIB_DEXCC_RETURN);
  method.code = code; method.count = 2; method.registers = 3; method.parameters = 2;
  assert (mulib_dexcc_encode (&method,100000,&bytes,NULL) == MULIB_DEXCC_OK);
  assert (bytes.length == 6 && !memcmp (bytes.data,"\x90\0\1\2\x0f\0",6));
  assert (mulib_dexcc_decode (bytes.data,bytes.length,3,2,100000,&decoded,NULL) == MULIB_DEXCC_OK);
  assert (mulib_dexcc_file (&decoded.method,"Lz/Test;","add",100000,&file,NULL) == MULIB_DEXCC_OK);
  assert (mulib_art_load_dex (file.data,file.length,"Lz/Test;","add",2,100000,&m,NULL) == MULIB_MIR_OK);
  assert (mulib_art_invoke (m,args,2,100000,&result) == MULIB_MIR_FINISHED && result == 42);
  assert (mulib_art_destroy (&m) == MULIB_MIR_OK);
  file.data[file.length - 1] ^= 1;
  assert (mulib_dexcc_check_file (file.data,file.length,100000,NULL) == MULIB_DEXCC_INTEGRITY);
  mulib_dexcc_free (&decoded); mulib_buffer_free (&bytes); mulib_buffer_free (&file);
  assert (mulib_art_load_code (literal,sizeof (literal),1,1,100000,&m,NULL) == MULIB_MIR_OK);
  assert (mulib_art_invoke (m,args,1,100000,&result) == MULIB_MIR_FINISHED && result == 21);
  assert (mulib_art_destroy (&m) == MULIB_MIR_OK);
}
static int adapter_read (void *user, size_t k, mulib_dexcc_instruction_t *out)
{ *out = ((const mulib_dexcc_instruction_t *)user)[k]; return 1; }
static void adapters (void)
{
  mulib_dexcc_instruction_t records[5]; mulib_dexcc_ir_t ir;
  mulib_dexcc_buffer_t b = {0}, decoded = {0}; mulib_buffer_t bytes = {0};
  mulib_art_method_t *m = NULL; int32_t result;
  records[0] = mulib_dexcc_instruction (MULIB_DEXCC_CONST); records[0].literal = 0;
  records[1] = mulib_dexcc_instruction (MULIB_DEXCC_CONST); records[1].a = 1; records[1].literal = 1;
  records[2] = mulib_dexcc_instruction (MULIB_DEXCC_IF_EQ); records[2].b = 1; records[2].target = 4;
  records[3] = mulib_dexcc_instruction (MULIB_DEXCC_CONST); records[3].literal = 42;
  records[4] = mulib_dexcc_instruction (MULIB_DEXCC_RETURN);
  ir.count = 5; ir.registers = 2; ir.parameters = 0; ir.read = adapter_read; ir.user = records;
  assert (mulib_dexcc_import (&ir,100000,&b,NULL) == MULIB_DEXCC_OK && b.code != records);
  assert (mulib_dexcc_encode (&b.method,100000,&bytes,NULL) == MULIB_DEXCC_OK);
  assert (mulib_dexcc_decode (bytes.data,bytes.length,2,0,100000,&decoded,NULL) == MULIB_DEXCC_OK);
  assert (mulib_art_compile (&decoded.method,100000,&m,NULL) == MULIB_MIR_OK);
  assert (mulib_art_invoke (m,NULL,0,100000,&result) == MULIB_MIR_FINISHED && result == 42);
  assert (mulib_art_destroy (&m) == MULIB_MIR_OK);
  mulib_dexcc_free (&b); mulib_dexcc_free (&decoded); mulib_buffer_free (&bytes);
}
static void class_file (const char *path)
{
  unsigned char bytes[4096]; size_t n, k; FILE *f = fopen (path,"rb");
  mulib_jvm_method_t *m = NULL; int32_t arg = 10, result;
  assert (f); n = fread (bytes,1,sizeof (bytes),f); assert (n < sizeof (bytes) && !ferror (f)); assert (!fclose (f));
  assert (mulib_jvm_load_class (bytes,n,"sum","(I)I",1000000,&m,NULL) == MULIB_MIR_OK);
  assert (mulib_jvm_invoke (m,&arg,1,100000,&result) == MULIB_MIR_FINISHED && result == 45);
  assert (mulib_jvm_destroy (&m) == MULIB_MIR_OK);
  for (k = 0; k < n; ++k) assert (mulib_jvm_load_class (bytes,k,"sum","(I)I",1000000,&m,NULL) != MULIB_MIR_OK && !m);
}
int main (int argc, char **argv)
{
  arithmetic (); differential (); loops (); hostile (); dex_parameters (); adapters ();
  dex (argc == 3 && !strcmp (argv[1], "--dex") ? argv[2] : NULL);
  if (argc == 3 && !strcmp (argv[1], "--class")) class_file (argv[2]);
  if (argc == 3 && !strcmp (argv[1], "--aot")) {
    const unsigned char bytes[] = {0x1a, 0x1b, 0x60, 0xac};
    mulib_jvm_code_t c = {bytes, sizeof (bytes), 2, 2, 2, NULL, NULL, 0};
    mulib_jvm_method_t *m = NULL; FILE *f = fopen (argv[2], "w"); assert (f);
    assert (mulib_jvm_load_code (&c, 100000, &m, NULL) == MULIB_MIR_OK);
    assert (mulib_jvm_emit_c (m, "mulib_generated_android", writer, f, NULL) == MULIB_MIR_OK);
    assert (!fclose (f)); assert (mulib_jvm_destroy (&m) == MULIB_MIR_OK);
  }
  return 0;
}
