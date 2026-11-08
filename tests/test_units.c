/* Cross-target code generation runs on any host; no emitted code is executed. */
#include <stdlib.h>
#include <stdint.h>
static size_t fail_after = SIZE_MAX;
static void *unit_test_malloc (size_t n)
{
  if (fail_after != SIZE_MAX) {
    if (!fail_after) return NULL;
    --fail_after;
  }
  return malloc (n);
}
#define malloc unit_test_malloc
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_MPU
#define MULIB_IMPL_MCU
#define MULIB_IMPL_GPU
#define MULIB_IMPL_AMD64
#define MULIB_IMPL_AARCH64
#define MULIB_IMPL_RISCV64
#define MULIB_IMPL_AVR32
#define MULIB_IMPL_XTENSA
#define MULIB_IMPL_AMDGPU
#define MULIB_IMPL_NVPTX
#define MULIB_IMPL_ASSEMBLER
#define MULIB_IMPL_DSYMSPEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_LINKER
#include "mulib-amd64.h"
#include "mulib-aarch64.h"
#include "mulib-riscv64.h"
#include "mulib-avr32.h"
#include "mulib-xtensa.h"
#include "mulib-amdgpu.h"
#include "mulib-nvptx.h"
#undef malloc
#include <assert.h>
#include <string.h>

static void golden (const mulib_unit_t *c, const char *form,
    const uint64_t *values, size_t count, const unsigned char *expected, size_t n)
{
  unsigned char bytes[32], before[32];
  uint64_t decoded[16] = {0};
  size_t length = 99, consumed = 99;
  mulib_emitter_t image;
  mulib_archml_error_t e;
  memset (bytes, 0xa5, sizeof (bytes));
  assert (mulib_unit_encode (c, form, values, count, bytes, sizeof (bytes), &length) == MULIB_ARCHML_OK);
  assert (length == n && !memcmp (bytes, expected, n));
  assert (mulib_unit_decode (c, form, bytes, n, decoded, count, &consumed) == MULIB_ARCHML_OK);
  assert (consumed == n && (!count || !memcmp (decoded, values, count * sizeof (*values))));
  consumed = 99;
  for (size_t i = 0; i < count; ++i) decoded[i] = 123;
  assert (mulib_unit_decode (c, form, bytes, n - 1, decoded, count, &consumed) == MULIB_ARCHML_LIMIT);
  assert (consumed == 99);
  for (size_t i = 0; i < count; ++i) assert (decoded[i] == 123);
  memcpy (before, bytes, sizeof (bytes)); length = 99;
  assert (mulib_unit_encode (c, form, values, count, bytes, n - 1, &length) == MULIB_ARCHML_LIMIT);
  assert (length == 99 && !memcmp (before, bytes, sizeof (bytes)));
  assert (mulib_emitter_init (&image, bytes, sizeof (bytes), c->order) == 0);
  assert (mulib_unit_emit (c, &image, form, values, count, &e) == MULIB_ARCHML_OK);
  assert (e.code == MULIB_ARCHML_OK && !e.message && image.size == n && !memcmp (bytes, expected, n));
  image.capacity = image.size;
  assert (mulib_unit_emit (c, &image, form, values, count, &e) == MULIB_ARCHML_LIMIT);
  assert (e.code == MULIB_ARCHML_LIMIT && e.message && image.size == n && !memcmp (bytes, before, sizeof (bytes)));
  image.order = c->order == MULIB_EMITTER_LITTLE ? MULIB_EMITTER_BIG : MULIB_EMITTER_LITTLE;
  assert (mulib_unit_emit (c, &image, form, values, count, &e) == MULIB_ARCHML_INVALID);
  assert (image.size == n && !memcmp (bytes, before, sizeof (bytes)));
}

static void processors (void)
{
  mulib_amd64_t x = {0};
  mulib_aarch64_t a = {0};
  mulib_riscv64_t r = {0};
  mulib_avr32_t v = {0};
  mulib_xtensa_t t = {0};
  mulib_amdgpu_t g = {0};
  mulib_archml_error_t e;
  mulib_archml_register_t reg;
  const char *features[] = {"base"};
  const uint64_t memory[] = {9, 12, (uint64_t)-16};
  const uint64_t add[] = {0, 1, 2}, rv[] = {10, 11, 12}, avr[] = {2, 3}, xt[] = {2, 3, 4};
  const unsigned char xb[] = {0x4d,0x8b,0x8c,0x24,0xf0,0xff,0xff,0xff};
  const unsigned char ab[] = {0x20,0,2,0x8b}, rb[] = {0x33,0x85,0xc5,0};
  const unsigned char vb[] = {6,2}, tb[] = {0x40,0x23,0x80}, gb[] = {1,2,0,0x80};
  unsigned char buffer[32]; size_t length = 99;
  assert (mulib_amd64_init_ex (&x, &e) && !e.message);
  assert (!mulib_amd64_init_ex (&x, &e) && e.code == MULIB_ARCHML_INVALID);
  assert (mulib_aarch64_init (&a) && !mulib_aarch64_init (&a));
  assert (mulib_riscv64_init (&r) && !mulib_riscv64_init (&r));
  assert (mulib_avr32_init (&v) && !mulib_avr32_init (&v));
  assert (mulib_xtensa_init (&t) && !mulib_xtensa_init (&t));
  assert (mulib_amdgpu_init (&g) && !mulib_amdgpu_init (&g));
  assert (!strcmp (mulib_archml_name (mulib_aarch64_document (&a)), "arm64"));
  assert (a.bits == 64 && r.bits == 64 && v.bits == 32 && t.bits == 32);
  assert (v.family == MULIB_UNIT_MCU && g.family == MULIB_UNIT_GPU);
  golden (mulib_amd64_mpu (&x), "load64-base-disp32", memory, 3, xb, sizeof (xb));
  golden (&a, "add64", add, 3, ab, sizeof (ab));
  golden (&r, "add", rv, 3, rb, sizeof (rb));
  golden (&v, "add", avr, 2, vb, sizeof (vb));
  golden (&t, "add", xt, 3, tb, sizeof (tb));
  golden (&g, "s_add_u32", add, 3, gb, sizeof (gb));
  assert (mulib_mpu_register_info (mulib_amd64_mpu (&x), "r9", &reg) && reg.encoding == 9);
  assert (mulib_riscv64_register_info (&r, "a0", &reg) && reg.encoding == 10);
  assert (mulib_aarch64_register_info (&a, "x0", &reg) && reg.bits == 64);
  assert (mulib_avr32_register_info (&v, "sp", &reg) && reg.encoding == 13);
  assert (mulib_xtensa_register_info (&t, "sp", &reg) && reg.encoding == 1);
  assert (mulib_amdgpu_register_info (&g, "s103", &reg) && reg.encoding == 103);
  assert (!mulib_aarch64_register_info (&a, "r0", &reg));
  assert (mulib_riscv64_enabled (&r, "add", features, 1) == 1);
  assert (mulib_riscv64_enabled (&r, "add", NULL, 0) == 0);
  assert (mulib_riscv64_enabled (&r, "no-form", features, 1) == -1);
  assert (mulib_mpu_enabled (mulib_amd64_mpu (&x), "addsd", features, 1) == 0);
  {
    const uint64_t branch[] = {1, (uint64_t)-1048576};
    const unsigned char bytes[] = {0xef,0,0,0x80};
    uint64_t bad[] = {1,3};
    golden (&r, "jal", branch, 2, bytes, sizeof (bytes));
    memset (buffer, 0xa5, sizeof (buffer));
    assert (mulib_riscv64_encode (&r, "jal", bad, 2, buffer, sizeof (buffer), &length) == MULIB_ARCHML_RANGE);
    bad[1] = 1048576;
    assert (mulib_riscv64_encode (&r, "jal", bad, 2, buffer, sizeof (buffer), &length) == MULIB_ARCHML_RANGE);
    assert (length == 99 && buffer[0] == 0xa5);
    bad[0] = 15; bad[1] = 3;
    assert (mulib_avr32_encode (&v, "add", bad, 2, buffer, sizeof (buffer), &length) == MULIB_ARCHML_RANGE);
    bad[0] = 0; bad[1] = 255;
    assert (mulib_amdgpu_encode (&g, "s_mov_b32", bad, 2, buffer, sizeof (buffer), &length) == MULIB_ARCHML_RANGE);
  }
  {
    mulib_emitter_t image;
    mulib_assembler_error_t old = {"stale", 123};
    assert (!mulib_emitter_init (&image, buffer, sizeof (buffer), MULIB_EMITTER_LITTLE));
    assert (mulib_amd64_emit (NULL, &image, NULL, "ret", NULL, 0, &old) == MULIB_ASSEMBLER_EINVAL);
    assert (old.message && old.linker_result == 0 && image.size == 0);
    assert (mulib_amd64_emit (&x, &image, NULL, "ret", NULL, 0, &old) == MULIB_ASSEMBLER_OK);
    assert (!old.message && !old.linker_result && image.size == 1 && buffer[0] == 0xc3);
    assert (mulib_amd64_emit_form (&x, &image, "load64-base-disp32", memory, 3, &e) == MULIB_ARCHML_OK);
    assert (image.size == 9 && !memcmp (buffer + 1, xb, sizeof (xb)));
  }
  mulib_amd64_free (&x); mulib_amd64_free (&x); assert (!mulib_amd64_mpu (&x));
  mulib_aarch64_free (&a); mulib_aarch64_free (&a);
  mulib_riscv64_free (&r); mulib_avr32_free (&v); mulib_xtensa_free (&t); mulib_amdgpu_free (&g);
  assert (mulib_aarch64_init (&a)); mulib_aarch64_free (&a);
  assert (mulib_amd64_encode (&x, "ret", NULL, 0, buffer, sizeof (buffer), &length) == MULIB_ARCHML_INVALID);
  assert (!mulib_aarch64_init (NULL) && !mulib_riscv64_init (NULL) && !mulib_avr32_init (NULL));
  assert (!mulib_xtensa_init (NULL) && !mulib_amdgpu_init (NULL) && !mulib_amd64_init (NULL));
  mulib_amd64_free (NULL); mulib_aarch64_free (NULL); mulib_riscv64_free (NULL);
  mulib_avr32_free (NULL); mulib_xtensa_free (NULL); mulib_amdgpu_free (NULL);
}

static void text_gpu (void)
{
  mulib_nvptx_t c = {0};
  mulib_archml_error_t e;
  mulib_emitter_t image;
  unsigned char bytes[256], before[256];
  const char *operands[] = {"%r0", "%r1", "%r2"};
  const char expected[] = "add.s32 %r0, %r1, %r2;\n";
  size_t length = 99;
  uint64_t decoded[] = {99,99,99};
  assert (mulib_nvptx_init_ex (&c, &e) && !e.message);
  assert (!mulib_nvptx_init (&c));
  memset (bytes, 0xa5, sizeof (bytes));
  assert (!mulib_emitter_init (&image, bytes, sizeof (bytes), MULIB_EMITTER_BIG));
  assert (mulib_nvptx_emit (&c, &image, "add.s32", operands, 3, &e) == MULIB_ARCHML_OK);
  assert (image.size == sizeof (expected) - 1 && !memcmp (bytes, expected, image.size));
  assert (bytes[image.size] == 0xa5 && !e.message);
  memcpy (before, bytes, sizeof (bytes));
  assert (mulib_nvptx_encode (&c, "add.s32", NULL, 0, bytes, sizeof (bytes), &length) == MULIB_ARCHML_MISMATCH);
  assert (mulib_nvptx_decode (&c, "add.s32", bytes, sizeof (bytes), decoded, 3, &length) == MULIB_ARCHML_MISMATCH);
  assert (length == 99 && decoded[0] == 99 && !memcmp (before, bytes, sizeof (bytes)));
  assert (mulib_nvptx_emit (&c, &image, "missing", operands, 3, &e) == MULIB_ARCHML_MISMATCH && e.message);
  assert (mulib_nvptx_emit (&c, &image, "add.s32", operands, 2, &e) == MULIB_ARCHML_INVALID);
  operands[0] = "%r0; trap;";
  assert (mulib_nvptx_emit (&c, &image, "add.s32", operands, 3, &e) == MULIB_ARCHML_INVALID);
  operands[0] = "%";
  assert (mulib_nvptx_emit (&c, &image, "add.s32", operands, 3, &e) == MULIB_ARCHML_INVALID);
  operands[0] = "%r0"; image.capacity = image.size;
  assert (mulib_nvptx_emit (&c, &image, "add.s32", operands, 3, &e) == MULIB_ARCHML_LIMIT);
  assert (!memcmp (before, bytes, sizeof (bytes)) && image.size == sizeof (expected) - 1);
  image.capacity = sizeof (bytes);
  for (size_t i = 0; i < mulib_archml_count (c.document, "operation"); ++i) {
    const mulib_archml_node_t *op = mulib_archml_at (c.document, "operation", i);
    image.size = 0;
    assert (mulib_nvptx_emit (&c, &image, mulib_archml_child (op, 1)->text, operands, 3, &e) == MULIB_ARCHML_OK);
    assert (bytes[image.size - 1] == '\n' && bytes[image.size - 2] == ';');
  }
  mulib_nvptx_free (&c); mulib_nvptx_free (&c); mulib_nvptx_free (NULL);
  assert (!mulib_nvptx_init (NULL));
  assert (mulib_nvptx_emit (&c, &image, "add.s32", operands, 3, &e) == MULIB_ARCHML_INVALID);
}

static void contract (void)
{
  char source[] = "(archml 1 future (target future (bits 32) (endian big))"
    " (pcode-op nop (arity 0 0) (semantics \"Advance PC.\"))"
    " (instruction nop (operands) (encoding (base \"1234\")) (pcode (nop))))";
  const unsigned char expected[] = {0x12,0x34};
  mulib_mpu_t c = {0};
  mulib_archml_error_t e;
  mulib_archml_options_t options = {1,0,0,0};
  assert (mulib_mpu_init (&c, "nvptx", &e) == MULIB_ARCHML_MISMATCH && !c.document);
  assert (mulib_mcu_init (&c, "aarch64", &e) == MULIB_ARCHML_MISMATCH);
  assert (mulib_gpu_init (&c, "avr32", &e) == MULIB_ARCHML_MISMATCH);
  assert (mulib_unit_init (&c, (mulib_unit_family)0, "amd64", &e) == MULIB_ARCHML_INVALID);
  assert (mulib_mpu_parse (&c, source, strlen (source), &options, &e) == MULIB_ARCHML_LIMIT && !c.document);
  assert (mulib_mpu_parse (&c, "(", 1, NULL, &e) == MULIB_ARCHML_SYNTAX && !c.document);
  assert (mulib_mpu_parse (&c, source, strlen (source), NULL, &e) == MULIB_ARCHML_OK && !e.message);
  assert (mulib_mpu_parse (&c, source, strlen (source), NULL, &e) == MULIB_ARCHML_INVALID);
  source[0] = '!'; /* Owned source, not a borrowed parse buffer. */
  golden (&c, "nop", NULL, 0, expected, sizeof (expected));
  mulib_mpu_free (&c);
  source[0] = '(';
  assert (mulib_mcu_parse (&c, source, strlen (source), NULL, NULL) == MULIB_ARCHML_OK);
  assert (c.family == MULIB_UNIT_MCU); mulib_mcu_free (&c);
  assert (mulib_gpu_parse (&c, source, strlen (source), NULL, NULL) == MULIB_ARCHML_OK);
  assert (c.family == MULIB_UNIT_GPU); mulib_gpu_free (&c);
  assert (!mulib_unit_document (NULL) && !mulib_unit_find (NULL, "instruction", "nop"));
  mulib_unit_free (NULL);
  fail_after = 0;
  assert (mulib_mpu_init (&c, "amd64", &e) == MULIB_ARCHML_NOMEM && !c.document);
  fail_after = SIZE_MAX;
  /* Walk every malloc failure in a small initialization, then verify reuse. */
  for (size_t i = 0;; ++i) {
    int rc;
    fail_after = i;
    rc = mulib_mcu_init (&c, "avr32", &e);
    fail_after = SIZE_MAX;
    if (rc == MULIB_ARCHML_OK) { mulib_mcu_free (&c); break; }
    assert (rc < 0 && e.message && !c.document && i < 1000);
  }
  assert (mulib_mcu_init (&c, "avr32", NULL) == MULIB_ARCHML_OK); mulib_mcu_free (&c);
}
int main (void)
{
  contract (); processors (); text_gpu ();
  return 0;
}
