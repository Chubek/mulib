#undef NDEBUG
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail_allocation;

static void *
test_malloc(size_t size)
{
  return fail_allocation ? NULL : malloc(size);
}

#define MULIB_IMPL_XMACIR
#define MULIB_XMACIR_MALLOC_ test_malloc
#include "mulib-xmacir.h"

static int
parse_text(const char *source, mulib_xmacir_program_t *program, mulib_xmacir_error_t *error)
{
  return mulib_xmacir_parse(source, strlen(source), program, error);
}

static void
assert_empty(const mulib_xmacir_program_t *program)
{
  assert(!program->mode && !program->entry[0] && !program->bridge[0]);
  assert(!program->context_tag[0] && !program->error_tag[0]);
  assert(!program->instructions && !program->count && !program->bytes && !program->length);
}

static void
assert_format(const mulib_xmacir_program_t *program, const char *expected)
{
  char *text = NULL;
  size_t length = 0;
  mulib_xmacir_error_t error = { 9, "stale error" };
  assert(mulib_xmacir_format(program, &text, &length, &error));
  assert(error.offset == 0 && error.message == NULL);
  assert(length == strlen(expected) && memcmp(text, expected, length + 1) == 0);
  free(text);
}

static void
test_u32(void)
{
  static const char source[] =
    "\n\tXM_U32 \t( sample_1 )\t\n"
    "\nXM_PUSH( 0004294967295 )\nXM_DUP()\nXM_SWAP()\n"
    "XM_PUSH(2)\nXM_ADD()\nXM_SUB()\nXM_PUSH(3)\nXM_MUL()\n"
    "XM_PUSH(4)\nXM_AND()\nXM_PUSH(5)\nXM_OR()\nXM_PUSH(0)\nXM_XOR()\n"
    "XM_DUP()\nXM_DROP()\n XM_RET ( )\t";
  static const char canonical[] =
    "XM_U32(sample_1)\n"
    "XM_PUSH(4294967295)\nXM_DUP()\nXM_SWAP()\n"
    "XM_PUSH(2)\nXM_ADD()\nXM_SUB()\nXM_PUSH(3)\nXM_MUL()\n"
    "XM_PUSH(4)\nXM_AND()\nXM_PUSH(5)\nXM_OR()\nXM_PUSH(0)\nXM_XOR()\n"
    "XM_DUP()\nXM_DROP()\nXM_RET()\n";
  static const mulib_xmacir_opcode_t expected[] = {
    MULIB_XMACIR_PUSH, MULIB_XMACIR_DUP, MULIB_XMACIR_SWAP, MULIB_XMACIR_PUSH,
    MULIB_XMACIR_ADD, MULIB_XMACIR_SUB, MULIB_XMACIR_PUSH, MULIB_XMACIR_MUL,
    MULIB_XMACIR_PUSH, MULIB_XMACIR_AND, MULIB_XMACIR_PUSH, MULIB_XMACIR_OR,
    MULIB_XMACIR_PUSH, MULIB_XMACIR_XOR, MULIB_XMACIR_DUP, MULIB_XMACIR_DROP,
    MULIB_XMACIR_RET
  };
  mulib_xmacir_program_t program = {0};
  mulib_xmacir_error_t error = { 7, "stale error" };
  char *slice = (char *)malloc(sizeof(source) - 1);
  size_t i;
  assert(slice);
  memcpy(slice, source, sizeof(source) - 1);
  assert(mulib_xmacir_parse(slice, sizeof(source) - 1, &program, &error));
  free(slice);
  assert(!error.message && !error.offset && mulib_xmacir_validate(&program, NULL));
  assert(program.mode == MULIB_XMACIR_U32 && strcmp(program.entry, "sample_1") == 0);
  assert(program.count == sizeof(expected) / sizeof(*expected));
  assert(!program.bytes && !program.length && !program.bridge[0]);
  for (i = 0; i < program.count; ++i)
    assert(program.instructions[i].opcode == expected[i]);
  assert(program.instructions[0].value == 4294967295UL);
  assert(program.instructions[3].value == 2 && program.instructions[12].value == 0);
  assert_format(&program, canonical);
  mulib_xmacir_free(&program);
  assert_empty(&program);
  mulib_xmacir_free(&program);
  mulib_xmacir_free(NULL);
}

static void
test_blob(void)
{
  static const char source[] =
    "XM_BLOB( launch , mulib_termscript_bridge, mulib_termscript_vm, mulib_termscript_error )\n"
    "XM_BYTES(h)\nXM_BYTES(h00010203)\nXM_BYTES( h0405060708090a0b0c0d0e0f )\n"
    "XM_BYTES(h101112131415161718191a1b1c1d1e1fff)\nXM_END()\n\n";
  static const char canonical[] =
    "XM_BLOB(launch,mulib_termscript_bridge,mulib_termscript_vm,mulib_termscript_error)\n"
    "XM_BYTES(h000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f)\n"
    "XM_BYTES(hff)\nXM_END()\n";
  static const char empty[] = "XM_BLOB(launch,run_bridge,vm_tag,error_tag)\nXM_END()\n";
  mulib_xmacir_program_t program = {0};
  size_t i;
  assert(parse_text(source, &program, NULL));
  assert(program.mode == MULIB_XMACIR_BLOB && program.length == 33);
  assert(program.count == 0 && program.instructions == NULL);
  assert(strcmp(program.entry, "launch") == 0);
  assert(strcmp(program.bridge, "mulib_termscript_bridge") == 0);
  assert(strcmp(program.context_tag, "mulib_termscript_vm") == 0);
  assert(strcmp(program.error_tag, "mulib_termscript_error") == 0);
  for (i = 0; i < 32; ++i) assert(program.bytes[i] == i);
  assert(program.bytes[32] == 255);
  assert_format(&program, canonical);
  mulib_xmacir_free(&program);
  fail_allocation = 1;
  assert(parse_text(empty, &program, NULL));
  fail_allocation = 0;
  assert(program.bytes == NULL && program.length == 0);
  assert_format(&program, empty);
  mulib_xmacir_free(&program);
}

static void
reject_bytes(const char *source, size_t length)
{
  mulib_xmacir_program_t sentinel = {0};
  mulib_xmacir_error_t error = { 0, NULL };
  sentinel.mode = MULIB_XMACIR_BLOB;
  strcpy(sentinel.entry, "untouched");
  assert(!mulib_xmacir_parse(source, length, &sentinel, &error));
  assert(error.message && *error.message && error.offset <= length);
  assert(sentinel.mode == MULIB_XMACIR_BLOB && strcmp(sentinel.entry, "untouched") == 0);
  assert(!sentinel.instructions && !sentinel.count && !sentinel.bytes && !sentinel.length);
}

static void
test_rejections(void)
{
  static const char *const rejected[] = {
    "", "\t\n", "#include <stdio.h>\n", "XM_SUB()\n", "XM_U32(f)\n",
    "XM_U32(f)\r\nXM_PUSH(0)\nXM_RET()\n", "XM_U32(f)\nXM_RET()\n",
    "XM_U32(f)\nXM_PUSH(0) XM_RET()\n", "XM_U32(f)\nXM_PUSH(\n0)\nXM_RET()\n",
    "XM_U32(f) /* comment */\nXM_PUSH(0)\nXM_RET()\n",
    "XM_U32(f)\nXM_PUSH(4294967296)\nXM_RET()\n",
    "XM_U32(f)\nXM_PUSH(-1)\nXM_RET()\n", "XM_U32(f)\nXM_PUSH(+1)\nXM_RET()\n",
    "XM_U32(f)\nXM_PUSH(0xff)\nXM_RET()\n", "XM_U32(f)\nXM_PUSH(1u)\nXM_RET()\n",
    "XM_U32(f)\nXM_PUSH(0)\nXM_DUP()\nXM_RET()\n",
    "XM_U32(f)\nXM_PUSH(0)\nXM_ADD()\nXM_RET()\n",
    "XM_U32(f)\nXM_PUSH(0)\nXM_RET(0)\n", "XM_U32(f)\nXM_PUSH(0)\nXM_RET()\nXM_RET()\n",
    "XM_U32(f)\nXM_PUSH(0)\nXM_RET()\nXM_END()\n",
    "XM_U32(f)\nXM_PUSH(0)\nXM_END()\n", "XM_BLOB(f,g,c,e)\nXM_RET()\n",
    "XM_BLOB(f,g,c,e)\nXM_END()\nXM_BYTES(h00)\n", "XM_BLOB(f,g,c,e)\nXM_BYTES(h00)\n",
    "XM_BLOB(f,g,c,e)\nXM_BYTES(hA1)\nXM_END()\n", "XM_BLOB(f,g,c,e)\nXM_BYTES(h0F)\nXM_END()\n",
    "XM_BLOB(f,g,c,e)\nXM_BYTES(h0)\nXM_END()\n", "XM_BLOB(f,g,c,e)\nXM_BYTES( h 00)\nXM_END()\n",
    "XM_BLOB(f,g,c,e)\nXM_BYTES(00)\nXM_END()\n", "XM_BLOB(f,g,c,e)\nXM_BYTES(hz0)\nXM_END()\n",
    "XM_BLOB(f,g,c,e)\nXM_BYTES(h000000000000000000000000000000000000000000000000000000000000000000)\nXM_END()\n",
    "XM_BLOB(f,g,c)\nXM_END()\n", "XM_BLOB(f,g,c,e,extra)\nXM_END()\n",
    "XM_BLOB(f,f,c,e)\nXM_END()\n", "XM_BLOB(f,g,c,e)\nXM_END(0)\n",
    "XM_BLOB(f,g,c,e)\nXM_END()\n#\n", "XM_U32(f\\\n)\nXM_PUSH(0)\nXM_RET()\n",
    "XM_U32(f)\v\nXM_PUSH(0)\nXM_RET()\n", "XM_U32(f)\nXM_PUSH(0)\nXM_RET()\n;",
    "XM_U32(f)\nXM_PUSH(0)\nXM_RET()\n\xe2\x80\x8b"
  };
  static const char *const bad_names[] = {
    "", "_temp", "9temp", "f-ref", "f;return", "\"name\"", "`include'",
    "asm", "int", "bool", "nullptr", "typeof_unqual", "static_assert",
    "main", "NULL", "size_t", "UINT_MAX", "int8_t", "malloc", "cosl",
    "m4_transform", "XM_DROP", "define", "syscmd", "sinclude", "dnl", "changequote"
  };
  static const char nul[] = "XM_U32(f)\nXM_PUSH(0)\0\nXM_RET()\n";
  mulib_xmacir_program_t program = {0};
  mulib_xmacir_error_t error;
  char text[200], long_name[65];
  size_t i;
  for (i = 0; i < sizeof(rejected) / sizeof(*rejected); ++i)
    reject_bytes(rejected[i], strlen(rejected[i]));
  reject_bytes(nul, sizeof(nul) - 1);
  reject_bytes(NULL, 0);
  reject_bytes(NULL, 1);
  assert(!mulib_xmacir_parse("x", 1, NULL, &error) && error.message);
  for (i = 0; i < sizeof(bad_names) / sizeof(*bad_names); ++i) {
    size_t slot;
    int n = snprintf(text, sizeof(text), "XM_U32(%s)\nXM_PUSH(0)\nXM_RET()\n", bad_names[i]);
    assert(n > 0 && (size_t)n < sizeof(text));
    reject_bytes(text, (size_t)n);
    for (slot = 0; slot < 4; ++slot) {
      n = snprintf(text, sizeof(text), "XM_BLOB(%s,%s,%s,%s)\nXM_END()\n",
                   slot == 0 ? bad_names[i] : "a", slot == 1 ? bad_names[i] : "b",
                   slot == 2 ? bad_names[i] : "c", slot == 3 ? bad_names[i] : "d");
      assert(n > 0 && (size_t)n < sizeof(text));
      reject_bytes(text, (size_t)n);
    }
  }
  memset(long_name, 'a', sizeof(long_name) - 1);
  long_name[64] = '\0';
  snprintf(text, sizeof(text), "XM_U32(%s)\nXM_PUSH(0)\nXM_RET()\n", long_name);
  reject_bytes(text, strlen(text));
  long_name[63] = '\0';
  snprintf(text, sizeof(text), "XM_U32(%s)\nXM_PUSH(0)\nXM_RET()\n", long_name);
  assert(parse_text(text, &program, NULL));
  assert(strlen(program.entry) == 63);
  mulib_xmacir_free(&program);
  assert(!parse_text("XM_U32(f)\nXM_DROP()\n", &program, &error));
  assert(error.offset == sizeof("XM_U32(f)\n") - 1);
}

static void
test_direct_and_atomic(void)
{
  mulib_xmacir_instruction_t ops[] = {
    { MULIB_XMACIR_PUSH, 4 }, { MULIB_XMACIR_RET, 42 }, { MULIB_XMACIR_SWAP, 0 }
  };
  mulib_xmacir_program_t program = {0};
  mulib_xmacir_program_t output = {0};
  mulib_xmacir_error_t error;
  unsigned char bytes[1] = {1};
  char dummy = 'x', *text = &dummy;
  size_t length = 17;
  program.mode = MULIB_XMACIR_U32;
  strcpy(program.entry, "work");
  program.instructions = ops;
  program.count = 2;
  assert_format(&program, "XM_U32(work)\nXM_PUSH(4)\nXM_RET()\n");
  program.count = 3;
  assert(!mulib_xmacir_validate(&program, &error) && error.offset == 1);
  assert(!mulib_xmacir_format(&program, &text, &length, &error));
  assert(text == &dummy && length == 17);
  program.count = 1;
  assert(!mulib_xmacir_validate(&program, &error) && error.offset == 1);
  ops[0].opcode = (mulib_xmacir_opcode_t)999;
  assert(!mulib_xmacir_validate(&program, &error) && error.offset == 0);
  ops[0].opcode = MULIB_XMACIR_PUSH;
  program.count = 2;
#if ULONG_MAX > 4294967295UL
  ops[0].value = 4294967296UL;
  assert(!mulib_xmacir_validate(&program, &error) && error.offset == 0);
  ops[0].value = 4;
#endif
  memset(program.entry, 'a', sizeof(program.entry));
  assert(!mulib_xmacir_validate(&program, &error));
  strcpy(program.entry, "work");
  program.bytes = bytes;
  assert(!mulib_xmacir_validate(&program, &error));
  program.bytes = NULL;
  assert(!mulib_xmacir_validate(NULL, &error));
  assert(!mulib_xmacir_format(&program, NULL, &length, &error) && length == 17);
  assert(!mulib_xmacir_format(&program, &text, NULL, &error) && text == &dummy);
  fail_allocation = 1;
  reject_bytes("XM_U32(f)\nXM_PUSH(1)\nXM_RET()\n", sizeof("XM_U32(f)\nXM_PUSH(1)\nXM_RET()\n") - 1);
  reject_bytes("XM_BLOB(f,b,c,e)\nXM_BYTES(h01)\nXM_END()\n",
               sizeof("XM_BLOB(f,b,c,e)\nXM_BYTES(h01)\nXM_END()\n") - 1);
  assert(!mulib_xmacir_format(&program, &text, &length, &error));
  assert(error.message && text == &dummy && length == 17);
  fail_allocation = 0;
  assert(parse_text("XM_BLOB(f,b,c,e)\nXM_END()\n", &output, &error));
  output.length = 1;
  assert(!mulib_xmacir_validate(&output, &error));
  output.bytes = bytes;
  assert(mulib_xmacir_validate(&output, &error));
#if UCHAR_MAX > 255
  bytes[0] = 256;
  assert(!mulib_xmacir_validate(&output, &error));
  bytes[0] = 1;
#endif
  output.instructions = ops;
  assert(!mulib_xmacir_validate(&output, &error));
}

static void
test_limits(void)
{
  const char header[] = "XM_U32(f)\n";
  const char push[] = "XM_PUSH(1)\n";
  const char drop[] = "XM_DROP()\n";
  const char pair[] = "XM_DUP()\nXM_DROP()\n";
  const char ret[] = "XM_RET()\n";
  const char extra[] = "XM_BYTES(h00)\n";
  mulib_xmacir_program_t program = {0}, parsed = {0};
  mulib_xmacir_error_t error;
  char *text = (char *)malloc(MULIB_XMACIR_MAX_TEXT + 1), *cur, *canon;
  size_t i, length;
  assert(text);
  memset(text, ' ', MULIB_XMACIR_MAX_TEXT + 1);
  assert(!mulib_xmacir_parse(text, MULIB_XMACIR_MAX_TEXT + 1, &parsed, &error));
  assert(error.offset == MULIB_XMACIR_MAX_TEXT);

  memcpy(text, header, sizeof(header) - 1);
  cur = text + sizeof(header) - 1;
  memcpy(cur, push, sizeof(push) - 1);
  cur += sizeof(push) - 1;
  memcpy(cur, ret, sizeof(ret) - 1);
  assert(mulib_xmacir_parse(text, MULIB_XMACIR_MAX_TEXT, &parsed, &error));
  mulib_xmacir_free(&parsed);

  cur = text + sizeof(header) - 1;
  for (i = 0; i < MULIB_XMACIR_MAX_STACK; ++i, cur += sizeof(push) - 1)
    memcpy(cur, push, sizeof(push) - 1);
  for (i = 1; i < MULIB_XMACIR_MAX_STACK; ++i, cur += sizeof(drop) - 1)
    memcpy(cur, drop, sizeof(drop) - 1);
  memcpy(cur, ret, sizeof(ret) - 1);
  assert(mulib_xmacir_parse(text, (size_t)(cur - text) + sizeof(ret) - 1, &parsed, &error));
  mulib_xmacir_free(&parsed);
  memcpy(text + sizeof(header) - 1 + MULIB_XMACIR_MAX_STACK * (sizeof(push) - 1),
         push, sizeof(push) - 1);
  assert(!mulib_xmacir_parse(text, (size_t)(cur - text) + sizeof(ret) - 1, &parsed, &error));
  assert(strstr(error.message, "stack overflow"));

  cur = text + sizeof(header) - 1;
  memcpy(cur, push, sizeof(push) - 1);
  cur += sizeof(push) - 1;
  for (i = 1; i + 1 < MULIB_XMACIR_MAX_INSTRUCTIONS; i += 2, cur += sizeof(pair) - 1)
    memcpy(cur, pair, sizeof(pair) - 1);
  memcpy(cur, ret, sizeof(ret) - 1);
  assert(mulib_xmacir_parse(text, (size_t)(cur - text) + sizeof(ret) - 1, &parsed, &error));
  assert(parsed.count == MULIB_XMACIR_MAX_INSTRUCTIONS);
  mulib_xmacir_free(&parsed);
  memcpy(cur, push, sizeof(push) - 1);
  cur += sizeof(push) - 1;
  memcpy(cur, ret, sizeof(ret) - 1);
  assert(!mulib_xmacir_parse(text, (size_t)(cur - text) + sizeof(ret) - 1, &parsed, &error));
  assert(strstr(error.message, "instruction limit"));

  program.mode = MULIB_XMACIR_BLOB;
  strcpy(program.entry, "wrapper");
  strcpy(program.bridge, "bridge");
  strcpy(program.context_tag, "ctx");
  strcpy(program.error_tag, "error");
  program.length = MULIB_XMACIR_MAX_BYTES;
  program.bytes = (unsigned char *)malloc(program.length);
  assert(program.bytes);
  memset(program.bytes, 0xf1, program.length);
  program.bytes[program.length - 1] = 0;
  assert(mulib_xmacir_format(&program, &canon, &length, &error));
  assert(length + sizeof(extra) < MULIB_XMACIR_MAX_TEXT);
  assert(mulib_xmacir_parse(canon, length, &parsed, &error));
  assert(parsed.length == program.length && memcmp(parsed.bytes, program.bytes, program.length) == 0);
  mulib_xmacir_free(&parsed);
  i = length - (sizeof("XM_END()\n") - 1);
  memcpy(text, canon, i);
  memcpy(text + i, extra, sizeof(extra) - 1);
  memcpy(text + i + sizeof(extra) - 1, canon + i, length - i);
  assert(!mulib_xmacir_parse(text, length + sizeof(extra) - 1, &parsed, &error));
  assert(error.offset == i && strstr(error.message, "BLOB byte limit"));
  free(canon);
  free(program.bytes);
  free(text);
}

static unsigned long rng_state = 93277;

static unsigned
next_byte(void)
{
  rng_state = (rng_state * 1664525UL + 1013904223UL) & 4294967295UL;
  return (unsigned)(rng_state >> 24);
}

static void
test_generated(void)
{
  static const char seed[] = "XM_BLOB(launch,run_bridge,context_tag,error_tag)\n"
                             "XM_BYTES(h8a7776)\nXM_END()\n";
  size_t i, j;
  for (i = 0; i < 1000; ++i) {
    mulib_xmacir_program_t program = {0}, reparsed = {0};
    mulib_xmacir_error_t error;
    char text[sizeof(seed) - 1], *canon, *repeat;
    size_t n, length, repeat_length;
    memcpy(text, seed, sizeof(text));
    for (j = 0; j < i % 5; ++j) {
      size_t at = next_byte() % sizeof(text);
      ((unsigned char *)text)[at] = (unsigned char)next_byte();
    }
    n = i % 7 ? sizeof(text) : next_byte() % sizeof(text);
    if (!mulib_xmacir_parse(text, n, &program, &error)) {
      assert(error.message && error.offset <= n);
      assert_empty(&program);
      continue;
    }
    assert(mulib_xmacir_format(&program, &canon, &length, &error));
    assert(mulib_xmacir_parse(canon, length, &reparsed, &error));
    assert(mulib_xmacir_format(&reparsed, &repeat, &repeat_length, &error));
    assert(length == repeat_length && memcmp(canon, repeat, length + 1) == 0);
    free(repeat);
    free(canon);
    mulib_xmacir_free(&reparsed);
    mulib_xmacir_free(&program);
  }
}

int
main(void)
{
  test_u32();
  test_blob();
  test_rejections();
  test_direct_and_atomic();
  test_limits();
  test_generated();
  puts("xmacir: ok");
  return 0;
}
