/** @file mulib-dexcc.h
 * @brief IR-neutral verified integer DEX assembler, decoder and DEX035 writer.
 * Define MULIB_IMPL_DEXCC plus BUFFER, AUTHCRYPTO and ADLER32 once.
 * No SDK dependency. See manual/mulib-dexcc.md for the exact method profile. */
#ifndef MULIB_DEXCC_H
#define MULIB_DEXCC_H
#include "mulib-buffer.h"
#include "mulib-authcrypto.h"
#include "mulib-adler32.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_DEXCC_NONE SIZE_MAX
typedef enum mulib_dexcc_status {
  MULIB_DEXCC_OK, MULIB_DEXCC_INVALID, MULIB_DEXCC_UNSUPPORTED,
  MULIB_DEXCC_RANGE, MULIB_DEXCC_LIMIT, MULIB_DEXCC_NOMEM,
  MULIB_DEXCC_UNINITIALIZED, MULIB_DEXCC_BAD_TARGET, MULIB_DEXCC_INTEGRITY,
  MULIB_DEXCC_ADAPTER_ERROR
} mulib_dexcc_status_t;
typedef struct mulib_dexcc_diagnostic { mulib_dexcc_status_t status; size_t instruction; }
  mulib_dexcc_diagnostic_t;
typedef enum mulib_dexcc_opcode {
  MULIB_DEXCC_NOP, MULIB_DEXCC_MOVE, MULIB_DEXCC_CONST,
  MULIB_DEXCC_NEG, MULIB_DEXCC_NOT, MULIB_DEXCC_ADD, MULIB_DEXCC_SUB,
  MULIB_DEXCC_MUL, MULIB_DEXCC_DIV, MULIB_DEXCC_REM, MULIB_DEXCC_AND,
  MULIB_DEXCC_OR, MULIB_DEXCC_XOR, MULIB_DEXCC_SHL, MULIB_DEXCC_SHR,
  MULIB_DEXCC_USHR, MULIB_DEXCC_GOTO, MULIB_DEXCC_IF_EQ, MULIB_DEXCC_IF_NE,
  MULIB_DEXCC_IF_LT, MULIB_DEXCC_IF_GE, MULIB_DEXCC_IF_GT, MULIB_DEXCC_IF_LE,
  MULIB_DEXCC_RETURN, MULIB_DEXCC_OPCODE_COUNT
} mulib_dexcc_opcode_t;
/** a=destination for value ops; b,c=sources. Branches read a,b; RETURN reads a.
 * target is an instruction ordinal, never a byte offset. Unused fields are
 * zero, except target=NONE. All registers carry Java int (32-bit) values. */
typedef struct mulib_dexcc_instruction {
  mulib_dexcc_opcode_t opcode;
  uint16_t a, b, c;
  int32_t literal;
  size_t target;
} mulib_dexcc_instruction_t;
/** Borrowed static (int,...)->int method. Parameters occupy the last registers.
 * No calls, objects, wide values, exceptions handlers, debug info or payloads. */
typedef struct mulib_dexcc_method {
  const mulib_dexcc_instruction_t *code;
  size_t count;
  uint16_t registers, parameters;
} mulib_dexcc_method_t;
typedef struct mulib_dexcc_buffer {
  mulib_dexcc_instruction_t *code;
  mulib_dexcc_method_t method;
} mulib_dexcc_buffer_t;
/** Construct a record with canonical unused fields. */
mulib_dexcc_instruction_t mulib_dexcc_instruction (mulib_dexcc_opcode_t opcode);
/** Verify ranges, reachable CFG, all-path register initialization and returns.
 * limit bounds instructions and instruction*register analysis cells. */
mulib_dexcc_status_t mulib_dexcc_validate (const mulib_dexcc_method_t *method,
    size_t limit, mulib_dexcc_diagnostic_t *diagnostic);
/** Encode little-endian insns (not a code_item). Long conditionals are relaxed
 * to inverted if + goto/32. Failure leaves the output buffer unchanged. */
mulib_dexcc_status_t mulib_dexcc_encode (const mulib_dexcc_method_t *method,
    size_t limit, mulib_buffer_t *out, mulib_dexcc_diagnostic_t *diagnostic);
/** Decode supported integer DEX forms. Branches must land at instruction starts.
 * Also accepts const/4,/16,/high16, move variants, /2addr and /lit forms.
 * Output must be zero-initialized. It owns code; no input bytes are retained. */
mulib_dexcc_status_t mulib_dexcc_decode (const void *bytes, size_t length,
    uint16_t registers, uint16_t parameters, size_t limit,
    mulib_dexcc_buffer_t *out, mulib_dexcc_diagnostic_t *diagnostic);
/** Free decoded/imported instructions and reset. */
void mulib_dexcc_free (mulib_dexcc_buffer_t *buffer);
/** Any IR can expose a lowered register method through this pull adapter.
 * The callback fills one canonical record; nonzero means success. No globals,
 * implicit IR dependencies or retained callback data. Called once per record. */
typedef int (*mulib_dexcc_read_fn) (void *user, size_t index,
    mulib_dexcc_instruction_t *instruction);
typedef struct mulib_dexcc_ir {
  size_t count;
  uint16_t registers, parameters;
  mulib_dexcc_read_fn read;
  void *user;
} mulib_dexcc_ir_t;
/** Materialize and verify an arbitrary IR adapter transactionally. */
mulib_dexcc_status_t mulib_dexcc_import (const mulib_dexcc_ir_t *ir, size_t limit,
    mulib_dexcc_buffer_t *out, mulib_dexcc_diagnostic_t *diagnostic);
/** Emit a complete DEX035 file containing one public static integer method in
 * one public final class extending Object. ASCII slash-separated class
 * descriptor and simple method name; no constructor is required. SHA-1,
 * Adler-32, sorted/deduplicated IDs, map and class_data are generated. */
mulib_dexcc_status_t mulib_dexcc_file (const mulib_dexcc_method_t *method,
    const char *class_descriptor, const char *method_name, size_t limit,
    mulib_buffer_t *out, mulib_dexcc_diagnostic_t *diagnostic);
/** Validate header, bounds and hashes of a little-endian DEX035..040 image.
 * This is an envelope check, NOT full DEX verification. 041 containers rejected. */
mulib_dexcc_status_t mulib_dexcc_check_file (const void *bytes, size_t length,
    size_t limit, mulib_dexcc_diagnostic_t *diagnostic);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DEXCC) && !defined(MULIB_DEXCC_IMPLEMENTED)
#define MULIB_DEXCC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static mulib_dexcc_status_t mulib_dexcc_error_ (mulib_dexcc_diagnostic_t *d,
    mulib_dexcc_status_t s, size_t k)
{ if (d) { d->status = s; d->instruction = k; } return s; }
static uint32_t mulib_dexcc_get_ (const unsigned char *p, unsigned n)
{ uint32_t v = 0; unsigned k; for (k = 0; k < n; ++k) v |= (uint32_t)p[k] << (k * 8); return v; }
static void mulib_dexcc_put_ (unsigned char *p, uint32_t v, unsigned n)
{ unsigned k; for (k = 0; k < n; ++k) p[k] = (unsigned char)(v >> (k * 8)); }
static int32_t mulib_dexcc_signed_ (uint32_t v)
{ return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)(UINT32_MAX - v); }
static int mulib_dexcc_uleb_ (mulib_buffer_t *b, uint32_t v)
{
  unsigned char bytes[5]; size_t n = 0;
  do { bytes[n++] = (unsigned char)((v & 127) | (v > 127 ? 128 : 0)); v >>= 7; } while (v);
  return mulib_buffer_append (b, bytes, n);
}
static int mulib_dexcc_align_ (mulib_buffer_t *b)
{ static const unsigned char zero[4] = {0}; return mulib_buffer_append (b, zero, (4 - b->length % 4) % 4); }
mulib_dexcc_instruction_t mulib_dexcc_instruction (mulib_dexcc_opcode_t op)
{
  mulib_dexcc_instruction_t i; memset (&i, 0, sizeof (i));
  i.opcode = op; i.target = MULIB_DEXCC_NONE; return i;
}
void mulib_dexcc_free (mulib_dexcc_buffer_t *b)
{ if (b) { free (b->code); memset (b, 0, sizeof (*b)); } }
static int mulib_dexcc_branch_ (int op)
{ return op >= MULIB_DEXCC_IF_EQ && op <= MULIB_DEXCC_IF_LE; }
static int mulib_dexcc_def_ (int op)
{ return op >= MULIB_DEXCC_MOVE && op <= MULIB_DEXCC_USHR; }
static int mulib_dexcc_reads_ (const mulib_dexcc_instruction_t *i, unsigned r)
{
  if (i->opcode == MULIB_DEXCC_RETURN) return i->a == r;
  if (mulib_dexcc_branch_ (i->opcode)) return i->a == r || i->b == r;
  if (i->opcode == MULIB_DEXCC_MOVE || i->opcode == MULIB_DEXCC_NEG || i->opcode == MULIB_DEXCC_NOT) return i->b == r;
  return i->opcode >= MULIB_DEXCC_ADD && i->opcode <= MULIB_DEXCC_USHR && (i->b == r || i->c == r);
}
mulib_dexcc_status_t mulib_dexcc_validate (const mulib_dexcc_method_t *m,
    size_t limit, mulib_dexcc_diagnostic_t *d)
{
  unsigned char *state = NULL, *seen = NULL;
  size_t *head = NULL, *next = NULL, *from = NULL;
  size_t k = 0, j, r, cells, ec = 0;
  int changed;
  mulib_dexcc_status_t s = MULIB_DEXCC_INVALID;
  if (!m || !m->code || !m->count || !m->registers || m->parameters > m->registers) goto done;
  s = MULIB_DEXCC_LIMIT;
  if (m->count > limit || m->count > limit / m->registers
      || m->count > SIZE_MAX / (2 * sizeof (size_t))) goto done;
  cells = m->count * m->registers;
  for (k = 0; k < m->count; ++k)
    {
      const mulib_dexcc_instruction_t *i = &m->code[k];
      int op = i->opcode;
      s = MULIB_DEXCC_UNSUPPORTED;
      if (op < 0 || op >= MULIB_DEXCC_OPCODE_COUNT) goto done;
      s = MULIB_DEXCC_RANGE;
      if ((mulib_dexcc_def_ (op) || op == MULIB_DEXCC_RETURN || mulib_dexcc_branch_ (op)) && i->a >= m->registers) goto done;
      if ((op == MULIB_DEXCC_MOVE || op == MULIB_DEXCC_NEG || op == MULIB_DEXCC_NOT
           || (op >= MULIB_DEXCC_ADD && op <= MULIB_DEXCC_USHR) || mulib_dexcc_branch_ (op)) && i->b >= m->registers) goto done;
      if (op >= MULIB_DEXCC_ADD && op <= MULIB_DEXCC_USHR && i->c >= m->registers) goto done;
      s = MULIB_DEXCC_INVALID;
      if ((op != MULIB_DEXCC_CONST && i->literal) || (op != MULIB_DEXCC_GOTO && !mulib_dexcc_branch_ (op) && i->target != MULIB_DEXCC_NONE)
          || (!(op >= MULIB_DEXCC_ADD && op <= MULIB_DEXCC_USHR) && i->c)
          || ((op == MULIB_DEXCC_NOP || op == MULIB_DEXCC_GOTO) && (i->a || i->b))
          || ((op == MULIB_DEXCC_CONST || op == MULIB_DEXCC_RETURN) && i->b)) goto done;
      s = MULIB_DEXCC_BAD_TARGET;
      if ((op == MULIB_DEXCC_GOTO || mulib_dexcc_branch_ (op)) && i->target >= m->count) goto done;
      if (k == m->count - 1 && op != MULIB_DEXCC_GOTO && op != MULIB_DEXCC_RETURN) goto done;
    }
  state = (unsigned char *)malloc (cells); seen = (unsigned char *)calloc (m->count, 1);
  head = (size_t *)malloc (m->count * sizeof (*head));
  next = (size_t *)malloc (m->count * 2 * sizeof (*next)); from = (size_t *)malloc (m->count * 2 * sizeof (*from));
  s = MULIB_DEXCC_NOMEM;
  if (!state || !seen || !head || !next || !from) goto done;
  for (k = 0; k < m->count; ++k) head[k] = MULIB_DEXCC_NONE;
  for (k = 0; k < m->count; ++k)
    {
      const mulib_dexcc_instruction_t *i = &m->code[k];
      size_t targets[2], n = 0;
      if (i->opcode == MULIB_DEXCC_GOTO || mulib_dexcc_branch_ (i->opcode)) targets[n++] = i->target;
      if (i->opcode != MULIB_DEXCC_GOTO && i->opcode != MULIB_DEXCC_RETURN) targets[n++] = k + 1;
      for (j = 0; j < n; ++j) { from[ec] = k; next[ec] = head[targets[j]]; head[targets[j]] = ec++; }
    }
  seen[0] = 1;
  do {
    changed = 0;
    for (k = 0; k < m->count; ++k) if (!seen[k])
      for (j = head[k]; j != MULIB_DEXCC_NONE; j = next[j]) if (seen[from[j]]) { seen[k] = 1; changed = 1; break; }
  } while (changed);
  s = MULIB_DEXCC_BAD_TARGET;
  for (k = 0; k < m->count; ++k) if (!seen[k]) goto done;
  memset (state, 1, cells);
  for (r = 0; r < m->registers; ++r) state[r] = r >= (size_t)(m->registers - m->parameters);
  do {
    changed = 0;
    for (k = 0; k < m->count; ++k)
      for (r = 0; r < m->registers; ++r)
        {
          unsigned char value = k ? 1 : (unsigned char)(r >= (size_t)(m->registers - m->parameters));
          for (j = head[k]; j != MULIB_DEXCC_NONE; j = next[j])
            {
              const mulib_dexcc_instruction_t *i = &m->code[from[j]];
              if (!(mulib_dexcc_def_ (i->opcode) && i->a == r)) value &= state[from[j] * m->registers + r];
            }
          if (value != state[k * m->registers + r]) { state[k * m->registers + r] = value; changed = 1; }
        }
  } while (changed);
  s = MULIB_DEXCC_UNINITIALIZED;
  for (k = 0; k < m->count; ++k) for (r = 0; r < m->registers; ++r)
    if (mulib_dexcc_reads_ (&m->code[k], (unsigned)r) && !state[k * m->registers + r]) goto done;
  s = MULIB_DEXCC_OK;
done:
  free (state); free (seen); free (head); free (next); free (from);
  return mulib_dexcc_error_ (d, s, k);
}
mulib_dexcc_status_t mulib_dexcc_import (const mulib_dexcc_ir_t *ir, size_t limit,
    mulib_dexcc_buffer_t *out, mulib_dexcc_diagnostic_t *d)
{
  mulib_dexcc_buffer_t b; size_t k; mulib_dexcc_status_t s;
  memset (&b, 0, sizeof (b));
  if (!out || out->code || out->method.code || out->method.count || !ir || !ir->read || !ir->count)
    return mulib_dexcc_error_ (d, MULIB_DEXCC_INVALID, 0);
  if (ir->count > limit || ir->count > SIZE_MAX / sizeof (*b.code)) return mulib_dexcc_error_ (d, MULIB_DEXCC_LIMIT, 0);
  b.code = (mulib_dexcc_instruction_t *)malloc (ir->count * sizeof (*b.code));
  if (!b.code) return mulib_dexcc_error_ (d, MULIB_DEXCC_NOMEM, 0);
  for (k = 0; k < ir->count; ++k)
    {
      b.code[k] = mulib_dexcc_instruction (MULIB_DEXCC_NOP);
      if (!ir->read (ir->user, k, &b.code[k])) { mulib_dexcc_free (&b); return mulib_dexcc_error_ (d, MULIB_DEXCC_ADAPTER_ERROR, k); }
    }
  b.method.code = b.code; b.method.count = ir->count; b.method.registers = ir->registers; b.method.parameters = ir->parameters;
  s = mulib_dexcc_validate (&b.method, limit, d);
  if (s == MULIB_DEXCC_OK) *out = b; else mulib_dexcc_free (&b);
  return s;
}
static size_t mulib_dexcc_width_ (int op)
{
  return op == MULIB_DEXCC_MOVE || op == MULIB_DEXCC_CONST || op == MULIB_DEXCC_GOTO ? 3
    : mulib_dexcc_branch_ (op) ? 5 : op >= MULIB_DEXCC_ADD && op <= MULIB_DEXCC_USHR ? 2 : 1;
}
mulib_dexcc_status_t mulib_dexcc_encode (const mulib_dexcc_method_t *m,
    size_t limit, mulib_buffer_t *out, mulib_dexcc_diagnostic_t *d)
{
  mulib_buffer_t b = {0}; size_t *offsets = NULL, k = 0, units = 0;
  mulib_dexcc_status_t s;
  if (!out) return mulib_dexcc_error_ (d, MULIB_DEXCC_INVALID, 0);
  s = mulib_dexcc_validate (m, limit, d); if (s != MULIB_DEXCC_OK) return s;
  offsets = (size_t *)malloc (m->count * sizeof (*offsets));
  if (!offsets) return mulib_dexcc_error_ (d, MULIB_DEXCC_NOMEM, 0);
  s = MULIB_DEXCC_LIMIT;
  for (k = 0; k < m->count; ++k)
    { size_t n = mulib_dexcc_width_ (m->code[k].opcode); if (units > INT32_MAX - n) goto done; offsets[k] = units; units += n; }
  if (units > limit || units > SIZE_MAX / 2) goto done;
  s = MULIB_DEXCC_NOMEM;
  for (k = 0; k < m->count; ++k)
    {
      const mulib_dexcc_instruction_t *i = &m->code[k];
      unsigned char bytes[10] = {0}; size_t n = mulib_dexcc_width_ (i->opcode);
      int op = i->opcode;
      s = MULIB_DEXCC_RANGE;
      if ((op != MULIB_DEXCC_MOVE && mulib_dexcc_def_ (op) && i->a > 255)
          || (op >= MULIB_DEXCC_ADD && op <= MULIB_DEXCC_USHR && (i->b > 255 || i->c > 255))
          || ((op == MULIB_DEXCC_NEG || op == MULIB_DEXCC_NOT) && (i->a > 15 || i->b > 15))
          || (mulib_dexcc_branch_ (op) && (i->a > 15 || i->b > 15)) || (op == MULIB_DEXCC_RETURN && i->a > 255)) goto done;
      if (op == MULIB_DEXCC_MOVE) { bytes[0] = 3; mulib_dexcc_put_ (bytes + 2, i->a, 2); mulib_dexcc_put_ (bytes + 4, i->b, 2); }
      else if (op == MULIB_DEXCC_CONST) { bytes[0] = 0x14; bytes[1] = (unsigned char)i->a; mulib_dexcc_put_ (bytes + 2, (uint32_t)i->literal, 4); }
      else if (op == MULIB_DEXCC_NEG || op == MULIB_DEXCC_NOT) { bytes[0] = op == MULIB_DEXCC_NEG ? 0x7b : 0x7c; bytes[1] = (unsigned char)(i->a | i->b << 4); }
      else if (op >= MULIB_DEXCC_ADD && op <= MULIB_DEXCC_USHR) { bytes[0] = (unsigned char)(0x90 + op - MULIB_DEXCC_ADD); bytes[1] = (unsigned char)i->a; bytes[2] = (unsigned char)i->b; bytes[3] = (unsigned char)i->c; }
      else if (op == MULIB_DEXCC_RETURN) { bytes[0] = 0x0f; bytes[1] = (unsigned char)i->a; }
      else if (op == MULIB_DEXCC_GOTO) { bytes[0] = 0x2a; mulib_dexcc_put_ (bytes + 2, (uint32_t)((int64_t)offsets[i->target] - (int64_t)offsets[k]), 4); }
      else if (mulib_dexcc_branch_ (op))
        {
          unsigned dex = 0x32u + (unsigned)(op - MULIB_DEXCC_IF_EQ);
          bytes[0] = (unsigned char)(dex ^ 1u); bytes[1] = (unsigned char)(i->a | i->b << 4); bytes[2] = 5;
          bytes[4] = 0x2a; mulib_dexcc_put_ (bytes + 6, (uint32_t)((int64_t)offsets[i->target] - (int64_t)offsets[k] - 2), 4);
        }
      s = MULIB_DEXCC_NOMEM;
      if (!mulib_buffer_append (&b, bytes, n * 2)) goto done;
    }
  s = mulib_buffer_append (out, b.data, b.length) ? MULIB_DEXCC_OK : MULIB_DEXCC_NOMEM;
done:
  free (offsets); mulib_buffer_free (&b); return mulib_dexcc_error_ (d, s, k);
}
/* Decoder expands literal and zero-test forms using one private scratch register.
 * Every original instruction start maps to the first expanded instruction. */
mulib_dexcc_status_t mulib_dexcc_decode (const void *data, size_t length,
    uint16_t regs, uint16_t params, size_t limit, mulib_dexcc_buffer_t *out,
    mulib_dexcc_diagnostic_t *d)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_dexcc_buffer_t b; size_t *map = NULL, *targets = NULL;
  size_t units = length / 2, at = 0, count = 0, k = 0;
  int scratch = 0;
  mulib_dexcc_status_t s = MULIB_DEXCC_INVALID;
  memset (&b, 0, sizeof (b));
  if (!out || out->code || out->method.code || out->method.count || !data || !length || length % 2 || !regs || params > regs) goto done;
  s = MULIB_DEXCC_LIMIT;
  if (units > limit / 2 || units > SIZE_MAX / (2 * sizeof (*b.code))) goto done;
  b.code = (mulib_dexcc_instruction_t *)calloc (units * 2, sizeof (*b.code));
  map = (size_t *)malloc (units * sizeof (*map)); targets = (size_t *)malloc (units * 2 * sizeof (*targets));
  s = MULIB_DEXCC_NOMEM; if (!b.code || !map || !targets) goto done;
  for (k = 0; k < units; ++k) map[k] = MULIB_DEXCC_NONE;
  while (at < units)
    {
      unsigned op = p[at * 2], hi = p[at * 2 + 1];
      size_t width = 1; int64_t delta = 0; int branch = 0, lit = 0, reverse = 0;
      mulib_dexcc_instruction_t i = mulib_dexcc_instruction (MULIB_DEXCC_NOP);
      map[at] = count; k = at;
      if (op == 2 || op == 0x13 || op == 0x15 || op == 0x29 || (op >= 0x32 && op <= 0x3d)
          || (op >= 0x90 && op <= 0x9a) || (op >= 0xd0 && op <= 0xe2)) width = 2;
      if (op == 3 || op == 0x14 || op == 0x2a) width = 3;
      s = MULIB_DEXCC_RANGE; if (width > units - at) goto done;
      if (op == 0) { if (hi) { s = MULIB_DEXCC_UNSUPPORTED; goto done; } }
      else if (op >= 1 && op <= 3)
        {
          i.opcode = MULIB_DEXCC_MOVE;
          i.a = (uint16_t)(op == 1 ? hi & 15 : op == 2 ? hi : mulib_dexcc_get_ (p + at * 2 + 2, 2));
          i.b = (uint16_t)(op == 1 ? hi >> 4 : mulib_dexcc_get_ (p + at * 2 + (op == 2 ? 2 : 4), 2));
          if (op == 3 && hi) goto done;
        }
      else if (op == 0x0f) { i.opcode = MULIB_DEXCC_RETURN; i.a = (uint16_t)hi; }
      else if (op >= 0x12 && op <= 0x15)
        {
          i.opcode = MULIB_DEXCC_CONST; i.a = (uint16_t)(op == 0x12 ? hi & 15 : hi);
          if (op == 0x12) i.literal = (int32_t)(hi >> 4) - ((hi & 128) ? 16 : 0);
          if (op == 0x13) { uint32_t x = mulib_dexcc_get_ (p + at * 2 + 2, 2); i.literal = (int32_t)x - ((x & 32768) ? 65536 : 0); }
          if (op == 0x14) i.literal = mulib_dexcc_signed_ (mulib_dexcc_get_ (p + at * 2 + 2, 4));
          if (op == 0x15) i.literal = mulib_dexcc_signed_ (mulib_dexcc_get_ (p + at * 2 + 2, 2) << 16);
        }
      else if (op == 0x7b || op == 0x7c) { i.opcode = op == 0x7b ? MULIB_DEXCC_NEG : MULIB_DEXCC_NOT; i.a = (uint16_t)(hi & 15); i.b = (uint16_t)(hi >> 4); }
      else if (op >= 0x90 && op <= 0x9a) { i.opcode = (mulib_dexcc_opcode_t)(MULIB_DEXCC_ADD + op - 0x90); i.a = (uint16_t)hi; i.b = p[at * 2 + 2]; i.c = p[at * 2 + 3]; }
      else if (op >= 0xb0 && op <= 0xba) { i.opcode = (mulib_dexcc_opcode_t)(MULIB_DEXCC_ADD + op - 0xb0); i.a = i.b = (uint16_t)(hi & 15); i.c = (uint16_t)(hi >> 4); }
      else if (op >= 0xd0 && op <= 0xe2)
        {
          unsigned x = op >= 0xd8 ? op - 0xd8 : op - 0xd0;
          i.opcode = (mulib_dexcc_opcode_t)(MULIB_DEXCC_ADD + x); reverse = x == 1;
          i.a = (uint16_t)(op >= 0xd8 ? hi : hi & 15); i.b = (uint16_t)(op >= 0xd8 ? p[at * 2 + 2] : hi >> 4);
          i.c = regs; lit = 1;
          if (op >= 0xd8) i.literal = (int32_t)p[at * 2 + 3] - (p[at * 2 + 3] >= 128 ? 256 : 0);
          else { uint32_t x16 = mulib_dexcc_get_ (p + at * 2 + 2, 2); i.literal = (int32_t)x16 - ((x16 & 32768) ? 65536 : 0); }
        }
      else if (op >= 0x28 && op <= 0x2a)
        {
          i.opcode = MULIB_DEXCC_GOTO; branch = 1;
          delta = op == 0x28 ? (int)hi - (hi >= 128 ? 256 : 0) : op == 0x29
            ? (int32_t)mulib_dexcc_get_ (p + at * 2 + 2, 2) - ((p[at * 2 + 3] & 128) ? 65536 : 0)
            : mulib_dexcc_signed_ (mulib_dexcc_get_ (p + at * 2 + 2, 4));
          if ((op != 0x28 && hi) || (op != 0x2a && !delta)) goto done;
        }
      else if (op >= 0x32 && op <= 0x3d)
        {
          i.opcode = (mulib_dexcc_opcode_t)(MULIB_DEXCC_IF_EQ + (op >= 0x38 ? op - 0x38 : op - 0x32));
          i.a = (uint16_t)(op >= 0x38 ? hi : hi & 15); i.b = (uint16_t)(op >= 0x38 ? regs : hi >> 4);
          if (op >= 0x38) lit = 1;
          delta = (int32_t)mulib_dexcc_get_ (p + at * 2 + 2, 2) - ((p[at * 2 + 3] & 128) ? 65536 : 0);
          branch = 1; if (!delta) goto done;
        }
      else { s = MULIB_DEXCC_UNSUPPORTED; goto done; }
      s = MULIB_DEXCC_RANGE;
      if ((mulib_dexcc_def_ (i.opcode) || i.opcode == MULIB_DEXCC_RETURN || mulib_dexcc_branch_ (i.opcode)) && i.a >= regs) goto done;
      if ((i.opcode == MULIB_DEXCC_MOVE || i.opcode == MULIB_DEXCC_NEG || i.opcode == MULIB_DEXCC_NOT
          || (i.opcode >= MULIB_DEXCC_ADD && i.opcode <= MULIB_DEXCC_USHR)
          || (mulib_dexcc_branch_ (i.opcode) && !lit)) && i.b >= regs) goto done;
      if (i.opcode >= MULIB_DEXCC_ADD && i.opcode <= MULIB_DEXCC_USHR && !lit && i.c >= regs) goto done;
      if (lit)
        {
          mulib_dexcc_instruction_t c = mulib_dexcc_instruction (MULIB_DEXCC_CONST);
          if (regs == UINT16_MAX) { s = MULIB_DEXCC_LIMIT; goto done; }
          /* Incoming arguments must remain at the end: remap below when a
           * scratch is needed, including parameter-bearing methods. */
          c.a = regs; c.literal = i.literal; i.literal = 0; scratch = 1;
          b.code[count] = c; targets[count++] = MULIB_DEXCC_NONE;
          if (reverse) { uint16_t tmp = i.b; i.b = i.c; i.c = tmp; }
        }
      if (branch)
        {
          int64_t target = (int64_t)at + delta;
          if (target < 0 || (uint64_t)target >= units) { s = MULIB_DEXCC_BAD_TARGET; goto done; }
          targets[count] = (size_t)target;
        }
      else targets[count] = MULIB_DEXCC_NONE;
      b.code[count++] = i; at += width;
    }
  for (k = 0; k < count; ++k) if (targets[k] != MULIB_DEXCC_NONE)
    {
      if (map[targets[k]] == MULIB_DEXCC_NONE) { s = MULIB_DEXCC_BAD_TARGET; goto done; }
      b.code[k].target = map[targets[k]];
    }
  if (scratch)
    for (k = 0; k < count; ++k)
      {
        mulib_dexcc_instruction_t *i = &b.code[k];
        unsigned boundary = regs - params;
#define MULIB_DEXCC_REMAP_(v) ((uint16_t)((v) == regs ? boundary : (v) >= boundary ? (unsigned)(v) + 1 : (v)))
        if (mulib_dexcc_def_ (i->opcode) || i->opcode == MULIB_DEXCC_RETURN || mulib_dexcc_branch_ (i->opcode)) i->a = MULIB_DEXCC_REMAP_ (i->a);
        if (i->opcode == MULIB_DEXCC_MOVE || i->opcode == MULIB_DEXCC_NEG || i->opcode == MULIB_DEXCC_NOT
            || (i->opcode >= MULIB_DEXCC_ADD && i->opcode <= MULIB_DEXCC_USHR) || mulib_dexcc_branch_ (i->opcode)) i->b = MULIB_DEXCC_REMAP_ (i->b);
        if (i->opcode >= MULIB_DEXCC_ADD && i->opcode <= MULIB_DEXCC_USHR) i->c = MULIB_DEXCC_REMAP_ (i->c);
#undef MULIB_DEXCC_REMAP_
      }
  b.method.code = b.code; b.method.count = count; b.method.registers = (uint16_t)(regs + scratch); b.method.parameters = params;
  s = mulib_dexcc_validate (&b.method, limit, d);
  if (s == MULIB_DEXCC_OK) { *out = b; memset (&b, 0, sizeof (b)); }
done:
  free (map); free (targets); mulib_dexcc_free (&b); return mulib_dexcc_error_ (d, s, k);
}
static int mulib_dexcc_name_ (const char *s, int descriptor)
{
  size_t k; int segment = 0;
  if (!s || !*s || (descriptor && *s++ != 'L')) return 0;
  for (k = 0; s[k]; ++k)
    {
      unsigned char c = (unsigned char)s[k];
      if (k >= 1024) return 0;
      if (descriptor && c == ';') return segment && !s[k + 1];
      if (descriptor && c == '/') { if (!segment) return 0; segment = 0; continue; }
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '$' || c == '-')) return 0;
      segment = 1;
    }
  return !descriptor && segment;
}
mulib_dexcc_status_t mulib_dexcc_file (const mulib_dexcc_method_t *m,
    const char *cls, const char *name, size_t limit, mulib_buffer_t *out,
    mulib_dexcc_diagnostic_t *d)
{
  mulib_buffer_t b = {0}, code = {0}; unsigned char h[112] = {0}, item[16] = {0};
  const char *strings[5], *sorted[5]; char *shorty = NULL;
  uint32_t ids[5], types[3], string_off, type_off, proto_off, method_off, class_off, data_off;
  uint32_t param_off = 0, code_off, class_data, string_data, map_off, ns = 0, nm = 0;
  uint32_t map_type[12], map_count[12], map_offset[12];
  size_t k = 0, j; mulib_dexcc_status_t s;
  if (!out || !mulib_dexcc_name_ (cls, 1) || !strcmp (cls, "Ljava/lang/Object;") || !mulib_dexcc_name_ (name, 0))
    return mulib_dexcc_error_ (d, MULIB_DEXCC_INVALID, 0);
  s = mulib_dexcc_encode (m, limit, &code, d); if (s != MULIB_DEXCC_OK) return s;
  shorty = (char *)malloc ((size_t)m->parameters + 2);
  s = MULIB_DEXCC_NOMEM; if (!shorty) goto done;
  memset (shorty, 'I', (size_t)m->parameters + 1); shorty[(size_t)m->parameters + 1] = 0;
  strings[0] = "I"; strings[1] = cls; strings[2] = "Ljava/lang/Object;"; strings[3] = name; strings[4] = shorty;
  for (k = 0; k < 5; ++k)
    {
      for (j = 0; j < ns && strcmp (strings[k], sorted[j]); ++j) {}
      if (j == ns) sorted[ns++] = strings[k];
    }
  for (k = 1; k < ns; ++k) { const char *v = sorted[k]; j = k; while (j && strcmp (v, sorted[j - 1]) < 0) { sorted[j] = sorted[j - 1]; --j; } sorted[j] = v; }
  for (k = 0; k < 5; ++k) for (j = 0; j < ns; ++j) if (!strcmp (strings[k], sorted[j])) ids[k] = (uint32_t)j;
  /* Type IDs are sorted by descriptor string index. */
  for (k = 0; k < 3; ++k) types[k] = ids[k];
  for (k = 1; k < 3; ++k) { uint32_t v = types[k]; j = k; while (j && v < types[j - 1]) { types[j] = types[j - 1]; --j; } types[j] = v; }
  for (k = 0; k < 3; ++k) for (j = 0; j < 3; ++j) if (ids[k] == types[j]) ids[k] = (uint32_t)j + 0x10000u;
  for (k = 0; k < 3; ++k) ids[k] -= 0x10000u;
  string_off = 112; type_off = string_off + ns * 4; proto_off = type_off + 12;
  method_off = proto_off + 12; class_off = method_off + 8; data_off = class_off + 32;
  memcpy (h, "dex\n035\0", 8); mulib_dexcc_put_ (h + 36, 112, 4); mulib_dexcc_put_ (h + 40, 0x12345678, 4);
  mulib_dexcc_put_ (h + 56, ns, 4); mulib_dexcc_put_ (h + 60, string_off, 4);
  mulib_dexcc_put_ (h + 64, 3, 4); mulib_dexcc_put_ (h + 68, type_off, 4);
  mulib_dexcc_put_ (h + 72, 1, 4); mulib_dexcc_put_ (h + 76, proto_off, 4);
  mulib_dexcc_put_ (h + 88, 1, 4); mulib_dexcc_put_ (h + 92, method_off, 4);
  mulib_dexcc_put_ (h + 96, 1, 4); mulib_dexcc_put_ (h + 100, class_off, 4);
  if (!mulib_buffer_append (&b, h, 112)) goto done;
  memset (h, 0, sizeof (h));
  while (b.length < data_off) { size_t n = data_off - b.length; if (n > sizeof (h)) n = sizeof (h); if (!mulib_buffer_append (&b, h, n)) goto done; }
#define MULIB_DEXCC_MAP_(t,n,o) do { map_type[nm]=(t); map_count[nm]=(n); map_offset[nm++]=(o); } while (0)
  MULIB_DEXCC_MAP_ (0, 1, 0); MULIB_DEXCC_MAP_ (1, ns, string_off); MULIB_DEXCC_MAP_ (2, 3, type_off);
  MULIB_DEXCC_MAP_ (3, 1, proto_off); MULIB_DEXCC_MAP_ (5, 1, method_off); MULIB_DEXCC_MAP_ (6, 1, class_off);
  if (m->parameters)
    {
      param_off = (uint32_t)b.length; mulib_dexcc_put_ (item, m->parameters, 4);
      if (!mulib_buffer_append (&b, item, 4)) goto done;
      for (k = 0; k < m->parameters; ++k) { mulib_dexcc_put_ (item, ids[0], 2); if (!mulib_buffer_append (&b, item, 2)) goto done; }
      if (!mulib_dexcc_align_ (&b)) goto done;
      MULIB_DEXCC_MAP_ (0x1001, 1, param_off);
    }
  code_off = (uint32_t)b.length; memset (item, 0, 16);
  mulib_dexcc_put_ (item, m->registers, 2); mulib_dexcc_put_ (item + 2, m->parameters, 2);
  mulib_dexcc_put_ (item + 12, (uint32_t)(code.length / 2), 4);
  if (!mulib_buffer_append (&b, item, 16) || !mulib_buffer_append (&b, code.data, code.length)) goto done;
  MULIB_DEXCC_MAP_ (0x2001, 1, code_off);
  class_data = (uint32_t)b.length;
  if (!mulib_buffer_append (&b, "\0\0\1\0\0\11", 6) || !mulib_dexcc_uleb_ (&b, code_off)) goto done;
  MULIB_DEXCC_MAP_ (0x2000, 1, class_data);
  string_data = (uint32_t)b.length;
  for (k = 0; k < ns; ++k)
    {
      mulib_dexcc_put_ (b.data + string_off + k * 4, (uint32_t)b.length, 4);
      if (!mulib_dexcc_uleb_ (&b, (uint32_t)strlen (sorted[k])) || !mulib_buffer_append (&b, sorted[k], strlen (sorted[k]) + 1)) goto done;
    }
  MULIB_DEXCC_MAP_ (0x2002, ns, string_data);
  if (!mulib_dexcc_align_ (&b)) goto done;
  map_off = (uint32_t)b.length; MULIB_DEXCC_MAP_ (0x1000, 1, map_off);
  mulib_dexcc_put_ (item, nm, 4); if (!mulib_buffer_append (&b, item, 4)) goto done;
  for (k = 0; k < nm; ++k)
    { memset (item, 0, 12); mulib_dexcc_put_ (item, map_type[k], 2); mulib_dexcc_put_ (item + 4, map_count[k], 4); mulib_dexcc_put_ (item + 8, map_offset[k], 4); if (!mulib_buffer_append (&b, item, 12)) goto done; }
#undef MULIB_DEXCC_MAP_
  s = MULIB_DEXCC_LIMIT; if (b.length > UINT32_MAX || b.length > limit) goto done;
  for (k = 0; k < 3; ++k) mulib_dexcc_put_ (b.data + type_off + k * 4, types[k], 4);
  mulib_dexcc_put_ (b.data + proto_off, ids[4], 4); mulib_dexcc_put_ (b.data + proto_off + 4, ids[0], 4); mulib_dexcc_put_ (b.data + proto_off + 8, param_off, 4);
  mulib_dexcc_put_ (b.data + method_off, ids[1], 2); mulib_dexcc_put_ (b.data + method_off + 4, ids[3], 4);
  mulib_dexcc_put_ (b.data + class_off, ids[1], 4); mulib_dexcc_put_ (b.data + class_off + 4, 0x11, 4); mulib_dexcc_put_ (b.data + class_off + 8, ids[2], 4);
  mulib_dexcc_put_ (b.data + class_off + 16, UINT32_MAX, 4); mulib_dexcc_put_ (b.data + class_off + 24, class_data, 4);
  mulib_dexcc_put_ (b.data + 32, (uint32_t)b.length, 4); mulib_dexcc_put_ (b.data + 52, map_off, 4);
  mulib_dexcc_put_ (b.data + 104, (uint32_t)b.length - data_off, 4); mulib_dexcc_put_ (b.data + 108, data_off, 4);
  (void)mulib_authcrypto_hash (MULIB_AUTHCRYPTO_SHA1, b.data + 32, b.length - 32, b.data + 12, 20);
  mulib_dexcc_put_ (b.data + 8, mulib_adler32 (b.data + 12, b.length - 12), 4);
  s = mulib_buffer_append (out, b.data, b.length) ? MULIB_DEXCC_OK : MULIB_DEXCC_NOMEM;
done:
  free (shorty); mulib_buffer_free (&b); mulib_buffer_free (&code); return mulib_dexcc_error_ (d, s, k);
}
mulib_dexcc_status_t mulib_dexcc_check_file (const void *data, size_t n, size_t limit,
    mulib_dexcc_diagnostic_t *d)
{
  const unsigned char *p = (const unsigned char *)data;
  unsigned char hash[20], seen[8192]; size_t k;
  uint32_t data_off, map_off, map_count;
  static const unsigned stride[] = {4, 4, 12, 8, 8, 32};
  if (!data || n < 112) return mulib_dexcc_error_ (d, MULIB_DEXCC_INVALID, 0);
  if (n > limit || n > UINT32_MAX) return mulib_dexcc_error_ (d, MULIB_DEXCC_LIMIT, 0);
  if (memcmp (p, "dex\n", 4) || p[7] || !((p[4] == '0' && p[5] == '3' && (p[6] == '5' || p[6] == '7' || p[6] == '8' || p[6] == '9'))
      || !memcmp (p + 4, "040", 3)) || mulib_dexcc_get_ (p + 40, 4) != 0x12345678)
    return mulib_dexcc_error_ (d, MULIB_DEXCC_UNSUPPORTED, 0);
  if (mulib_dexcc_get_ (p + 32, 4) != n || mulib_dexcc_get_ (p + 36, 4) != 112)
    return mulib_dexcc_error_ (d, MULIB_DEXCC_INVALID, 0);
  data_off = mulib_dexcc_get_ (p + 108, 4);
  if (mulib_dexcc_get_ (p + 64, 4) > 65535 || mulib_dexcc_get_ (p + 72, 4) > 65535)
    return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, 64);
  for (k = 0; k < 6; ++k)
    {
      uint32_t count = mulib_dexcc_get_ (p + 56 + k * 8, 4), off = mulib_dexcc_get_ (p + 60 + k * 8, 4);
      if ((!count && off) || (count && (off < 112 || off % 4 || off > n || count > (n - off) / stride[k])))
        return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, 56 + k * 8);
      if (count && (off > data_off || count > (data_off - off) / stride[k]))
        return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, 56 + k * 8);
      if (count) {
        size_t j;
        for (j = 0; j < k; ++j) {
          uint32_t prev_count = mulib_dexcc_get_ (p + 56 + j * 8, 4), prev_off = mulib_dexcc_get_ (p + 60 + j * 8, 4);
          if (prev_count && off < prev_off + (uint64_t)prev_count * stride[j])
            return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, 56 + k * 8);
        }
      }
    }
  {
    uint32_t off = mulib_dexcc_get_ (p + 108, 4), size = mulib_dexcc_get_ (p + 104, 4), map = mulib_dexcc_get_ (p + 52, 4);
    if (off < 112 || off % 4 || size % 4 || off > n || size != n - off
        || map < off || map % 4 || map > n - 4 || mulib_dexcc_get_ (p + map, 4) > (n - map - 4) / 12)
      return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, 52);
  }
  (void)mulib_authcrypto_hash (MULIB_AUTHCRYPTO_SHA1, p + 32, n - 32, hash, 20);
  if (memcmp (hash, p + 12, 20) || mulib_dexcc_get_ (p + 8, 4) != mulib_adler32 (p + 12, n - 12))
    return mulib_dexcc_error_ (d, MULIB_DEXCC_INTEGRITY, 8);
  map_off = mulib_dexcc_get_ (p + 52, 4); map_count = mulib_dexcc_get_ (p + map_off, 4);
  memset (seen, 0, sizeof (seen));
  for (k = 0; k < map_count; ++k)
    {
      const unsigned char *item = p + map_off + 4 + k * 12;
      uint32_t type = mulib_dexcc_get_ (item, 2), count = mulib_dexcc_get_ (item + 4, 4), off = mulib_dexcc_get_ (item + 8, 4);
      if (mulib_dexcc_get_ (item + 2, 2) || !count || off >= n
          || (seen[type / 8] & (1u << (type % 8)))
          || (k && off <= mulib_dexcc_get_ (item - 4, 4)))
        return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, map_off + 4 + k * 12);
      seen[type / 8] |= (unsigned char)(1u << (type % 8));
      if ((!type && (count != 1 || off)) || (type == 0x1000 && (count != 1 || off != map_off))
          || (type >= 0x1000 && off < data_off))
        return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, map_off + 4 + k * 12);
      if (type >= 1 && type <= 6
          && (count != mulib_dexcc_get_ (p + 56 + (type - 1) * 8, 4)
              || off != mulib_dexcc_get_ (p + 60 + (type - 1) * 8, 4)))
        return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, map_off + 4 + k * 12);
    }
  if (!(seen[0] & 1) || !(seen[0x1000 / 8] & 1)) return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, map_off);
  for (k = 1; k <= 6; ++k)
    if (mulib_dexcc_get_ (p + 56 + (k - 1) * 8, 4) && !(seen[0] & (1u << k)))
      return mulib_dexcc_error_ (d, MULIB_DEXCC_RANGE, map_off);
  return mulib_dexcc_error_ (d, MULIB_DEXCC_OK, 0);
}
#ifdef __cplusplus
}
#endif
#endif
