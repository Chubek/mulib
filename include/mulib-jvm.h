/** @file mulib-jvm.h
 * @brief Verified JVM integer-method frontend to ClickIR, with shared JIT/AOT.
 * Define MULIB_IMPL_JVM and ART's explicit transitive switches once.
 * Focused method VM, not a Java SE implementation or class library. */
#ifndef MULIB_JVM_H
#define MULIB_JVM_H
#include "mulib-art.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_art_method_t mulib_jvm_method_t;
/** Borrowed Code bytes. Only static (I...I)I methods are supported. Optional
 * constant tables use JVM pool indices and tag 3 for Integer. Unused pool slots
 * must not be read. max_stack counts category-1 Java int slots. */
typedef struct mulib_jvm_code {
  const unsigned char *bytes;
  size_t length;
  uint16_t max_locals, max_stack, parameters;
  const int32_t *integer_constants;
  const unsigned char *constant_tags;
  size_t constant_count;
} mulib_jvm_code_t;
/** Verify instruction boundaries, stack heights, locals' definite assignment,
 * branch targets and reachable code, then compile through ClickIR. *out=NULL.
 * Inputs are not retained. limit bounds source and all intermediate storage. */
mulib_mir_status_t mulib_jvm_load_code (const mulib_jvm_code_t *code, size_t limit,
    mulib_jvm_method_t **out, mulib_mir_diagnostic_t *diagnostic);
/** Select one static integer method from a JVM class file (major 45..71).
 * Structural parsing is bounded. Code exception tables are rejected; StackMap
 * attributes are not trusted, as integer stack/local verification is recomputed.
 * Unrelated methods need not belong to the executable subset. */
mulib_mir_status_t mulib_jvm_load_class (const void *bytes, size_t length,
    const char *method_name, const char *descriptor, size_t limit,
    mulib_jvm_method_t **out, mulib_mir_diagnostic_t *diagnostic);
/** Borrow the actual ClickIR compiler graph. */
const mulib_clickir_program_t *mulib_jvm_graph (const mulib_jvm_method_t *method);
/** Explicit baseline native JIT; UNSUPPORTED on non-Linux-AMD64 hosts. */
mulib_mir_status_t mulib_jvm_jit (mulib_jvm_method_t *method, mulib_mir_diagnostic_t *diagnostic);
/** Copy arguments into a resumable frame; release with mulib_mir_frame_free. */
mulib_mir_status_t mulib_jvm_frame_init (const mulib_jvm_method_t *method,
    const int32_t *arguments, size_t count, mulib_mir_frame_t *frame);
/** Run/resume with a MIR-operation budget, preserving JVM int wrap/shift/divide
 * semantics. Method must outlive frame; independent frames may share it. */
mulib_mir_status_t mulib_jvm_run (const mulib_jvm_method_t *method,
    mulib_mir_frame_t *frame, size_t budget);
/** Invoke once; result unchanged unless FINISHED. */
mulib_mir_status_t mulib_jvm_invoke (const mulib_jvm_method_t *method,
    const int32_t *arguments, size_t count, size_t budget, int32_t *result);
/** Emit C11 baseline AOT, including symbol_program() for frame initialization. */
mulib_mir_status_t mulib_jvm_emit_c (const mulib_jvm_method_t *method,
    const char *symbol, mulib_mir_write_fn writer, void *user,
    mulib_mir_diagnostic_t *diagnostic);
/** Destroy after releasing all frames; NULL safe, retains owner on OS failure. */
mulib_mir_status_t mulib_jvm_destroy (mulib_jvm_method_t **method);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_JVM) && !defined(MULIB_JVM_IMPLEMENTED)
#define MULIB_JVM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_jvm_decoded_ {
  unsigned opcode;
  size_t offset, target, local, pop, push;
  int32_t value;
} mulib_jvm_decoded_;
static mulib_mir_status_t mulib_jvm_error_ (mulib_mir_diagnostic_t *d, mulib_mir_status_t s, size_t at)
{ if (d) { d->status = s; d->record = at; } return s; }
static uint32_t mulib_jvm_be_ (const unsigned char *p, unsigned n)
{ uint32_t v = 0; unsigned k; for (k = 0; k < n; ++k) v = v << 8 | p[k]; return v; }
static int32_t mulib_jvm_signed_ (uint32_t v)
{ return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)(UINT32_MAX - v); }
static int mulib_jvm_cond_ (unsigned op) { return op >= 0x99 && op <= 0xa4; }
mulib_mir_status_t mulib_jvm_load_code (const mulib_jvm_code_t *c, size_t limit,
    mulib_jvm_method_t **out, mulib_mir_diagnostic_t *d)
{
  mulib_jvm_decoded_ *instructions = NULL;
  mulib_dexcc_instruction_t *code = NULL;
  size_t *map = NULL, *height = NULL, *expanded = NULL;
  size_t count = 0, at = 0, k = 0, pc = 0, regs, scratch, params;
  mulib_mir_status_t s = MULIB_MIR_INVALID;
  int changed;
  if (!out || *out || !c || !c->bytes || !c->length || !c->max_stack || c->parameters > c->max_locals
      || (c->constant_count && (!c->integer_constants || !c->constant_tags))) goto done;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (c->length > limit || c->length > SIZE_MAX / sizeof (*instructions)
      || c->length > SIZE_MAX / sizeof (*map) || c->constant_count > limit
      || c->parameters > 255 || c->parameters > limit || c->length > (limit - c->parameters) / 4
      || c->length > (SIZE_MAX / sizeof (*code) - c->parameters) / 4) goto done;
  regs = (size_t)c->max_locals + c->max_stack + 1 + c->parameters;
  if (regs > UINT16_MAX || regs > limit) goto done;
  instructions = (mulib_jvm_decoded_ *)calloc (c->length, sizeof (*instructions));
  map = (size_t *)malloc (c->length * sizeof (*map)); height = (size_t *)malloc (c->length * sizeof (*height));
  expanded = (size_t *)malloc (c->length * sizeof (*expanded));
  code = (mulib_dexcc_instruction_t *)calloc (c->length * 4 + c->parameters, sizeof (*code));
  s = MULIB_MIR_NO_MEMORY; if (!instructions || !map || !height || !expanded || !code) goto done;
  for (k = 0; k < c->length; ++k) map[k] = height[k] = MULIB_MIR_NONE;
  while (at < c->length)
    {
      mulib_jvm_decoded_ i;
      unsigned op = c->bytes[at]; size_t width = 1; int64_t delta = 0; int branch = 0;
      memset (&i, 0, sizeof (i)); i.opcode = op; i.offset = at; i.target = MULIB_MIR_NONE; k = at;
      if (op == 0x10 || op == 0x12 || op == 0x15 || op == 0x36) width = 2;
      if (op == 0x11 || op == 0x13 || op == 0x84 || mulib_jvm_cond_ (op) || op == 0xa7) width = 3;
      if (op == 0xc8) width = 5;
      if (op == 0xc4) { if (c->length - at < 2) { s = MULIB_MIR_BAD_REFERENCE; goto done; } width = c->bytes[at + 1] == 0x84 ? 6 : 4; }
      s = MULIB_MIR_BAD_REFERENCE; if (width > c->length - at) goto done;
      if (op == 0) {}
      else if (op >= 2 && op <= 8) { i.value = (int32_t)op - 3; i.opcode = 0x10; i.push = 1; }
      else if (op == 0x10 || op == 0x11)
        { uint32_t v = mulib_jvm_be_ (c->bytes + at + 1, (unsigned)width - 1); i.value = (int32_t)v - (op == 0x10 ? v >= 128 ? 256 : 0 : v >= 32768 ? 65536 : 0); i.opcode = 0x10; i.push = 1; }
      else if (op == 0x12 || op == 0x13)
        {
          uint32_t index = mulib_jvm_be_ (c->bytes + at + 1, (unsigned)width - 1);
          if (!index || index >= c->constant_count) goto done;
          if (c->constant_tags[index] != 3) { s = MULIB_MIR_UNSUPPORTED; goto done; }
          i.value = c->integer_constants[index]; i.opcode = 0x10; i.push = 1;
        }
      else if (op == 0x15 || (op >= 0x1a && op <= 0x1d))
        { i.opcode = 0x15; i.local = op == 0x15 ? c->bytes[at + 1] : op - 0x1a; i.push = 1; }
      else if (op == 0x36 || (op >= 0x3b && op <= 0x3e))
        { i.opcode = 0x36; i.local = op == 0x36 ? c->bytes[at + 1] : op - 0x3b; i.pop = 1; }
      else if (op == 0x84)
        { i.local = c->bytes[at + 1]; i.value = (int32_t)c->bytes[at + 2] - (c->bytes[at + 2] >= 128 ? 256 : 0); }
      else if (op == 0xc4)
        {
          i.opcode = c->bytes[at + 1]; i.local = mulib_jvm_be_ (c->bytes + at + 2, 2);
          if (i.opcode == 0x15) i.push = 1;
          else if (i.opcode == 0x36) i.pop = 1;
          else if (i.opcode == 0x84) { uint32_t v = mulib_jvm_be_ (c->bytes + at + 4, 2); i.value = (int32_t)v - (v >= 32768 ? 65536 : 0); }
          else { s = MULIB_MIR_UNSUPPORTED; goto done; }
        }
      else if (op == 0x57) i.pop = 1;
      else if (op == 0x59) { i.pop = 1; i.push = 2; }
      else if (op == 0x5f) { i.pop = i.push = 2; }
      else if (op == 0x74) i.pop = i.push = 1;
      else if (op == 0x60 || op == 0x64 || op == 0x68 || op == 0x6c || op == 0x70
          || op == 0x78 || op == 0x7a || op == 0x7c || op == 0x7e || op == 0x80 || op == 0x82)
        { i.pop = 2; i.push = 1; }
      else if (mulib_jvm_cond_ (op))
        { uint32_t v = mulib_jvm_be_ (c->bytes + at + 1, 2); delta = (int32_t)v - (v >= 32768 ? 65536 : 0); branch = 1; i.pop = op < 0x9f ? 1 : 2; }
      else if (op == 0xa7 || op == 0xc8)
        { uint32_t v = mulib_jvm_be_ (c->bytes + at + 1, (unsigned)width - 1); delta = op == 0xa7 ? (int64_t)v - (v >= 32768 ? 65536 : 0) : mulib_jvm_signed_ (v); branch = 1; i.opcode = 0xa7; }
      else if (op == 0xac) i.pop = 1;
      else { s = MULIB_MIR_UNSUPPORTED; goto done; }
      if ((i.opcode == 0x15 || i.opcode == 0x36 || i.opcode == 0x84) && i.local >= c->max_locals) goto done;
      if (branch) { int64_t target = (int64_t)at + delta; if (target < 0 || (uint64_t)target >= c->length) goto done; i.target = (size_t)target; }
      map[at] = count; instructions[count++] = i; at += width;
    }
  for (k = 0; k < count; ++k)
    if (instructions[k].target != MULIB_MIR_NONE)
      { size_t target = map[instructions[k].target]; if (target == MULIB_MIR_NONE) { s = MULIB_MIR_BAD_REFERENCE; goto done; } instructions[k].target = target; }
  height[0] = 0;
  do {
    changed = 0;
    for (k = 0; k < count; ++k) if (height[k] != MULIB_MIR_NONE)
      {
        const mulib_jvm_decoded_ *i = &instructions[k]; size_t h, targets[2], nt = 0, j;
        s = MULIB_MIR_BAD_TYPE;
        if (height[k] < i->pop) goto done;
        h = height[k] - i->pop + i->push;
        if (h > c->max_stack || (i->opcode == 0xac && height[k] != 1)) goto done;
        if (i->target != MULIB_MIR_NONE) targets[nt++] = i->target;
        if (i->opcode != 0xac && i->opcode != 0xa7) { if (k + 1 == count) { s = MULIB_MIR_BAD_RETURN; goto done; } targets[nt++] = k + 1; }
        for (j = 0; j < nt; ++j)
          { if (height[targets[j]] == MULIB_MIR_NONE) { height[targets[j]] = h; changed = 1; } else if (height[targets[j]] != h) goto done; }
      }
  } while (changed);
  s = MULIB_MIR_BAD_REFERENCE;
  for (k = 0; k < count; ++k) if (height[k] == MULIB_MIR_NONE) goto done;
  scratch = (size_t)c->max_locals + c->max_stack; params = scratch + 1;
  for (k = 0; k < c->parameters; ++k)
    { code[pc] = mulib_dexcc_instruction (MULIB_DEXCC_MOVE); code[pc].a = (uint16_t)k; code[pc++].b = (uint16_t)(params + k); }
  for (k = 0; k < count; ++k)
    {
      const mulib_jvm_decoded_ *i = &instructions[k];
      size_t top = (size_t)c->max_locals + height[k];
      mulib_dexcc_instruction_t v = mulib_dexcc_instruction (MULIB_DEXCC_NOP);
      expanded[k] = pc;
      if (i->opcode == 0x10) { v.opcode = MULIB_DEXCC_CONST; v.a = (uint16_t)top; v.literal = i->value; }
      else if (i->opcode == 0x15) { v.opcode = MULIB_DEXCC_MOVE; v.a = (uint16_t)top; v.b = (uint16_t)i->local; }
      else if (i->opcode == 0x36) { v.opcode = MULIB_DEXCC_MOVE; v.a = (uint16_t)i->local; v.b = (uint16_t)(top - 1); }
      else if (i->opcode == 0x84)
        {
          v.opcode = MULIB_DEXCC_CONST; v.a = (uint16_t)scratch; v.literal = i->value; code[pc++] = v;
          v = mulib_dexcc_instruction (MULIB_DEXCC_ADD); v.a = v.b = (uint16_t)i->local; v.c = (uint16_t)scratch;
        }
      else if (i->opcode == 0x59) { v.opcode = MULIB_DEXCC_MOVE; v.a = (uint16_t)top; v.b = (uint16_t)(top - 1); }
      else if (i->opcode == 0x5f)
        {
          v.opcode = MULIB_DEXCC_MOVE; v.a = (uint16_t)scratch; v.b = (uint16_t)(top - 1); code[pc++] = v;
          v.a = (uint16_t)(top - 1); v.b = (uint16_t)(top - 2); code[pc++] = v;
          v.a = (uint16_t)(top - 2); v.b = (uint16_t)scratch;
        }
      else if (i->opcode == 0x74) { v.opcode = MULIB_DEXCC_NEG; v.a = v.b = (uint16_t)(top - 1); }
      else if (i->opcode >= 0x60 && i->opcode <= 0x82)
        {
          switch (i->opcode) {
            case 0x60: v.opcode = MULIB_DEXCC_ADD; break; case 0x64: v.opcode = MULIB_DEXCC_SUB; break;
            case 0x68: v.opcode = MULIB_DEXCC_MUL; break; case 0x6c: v.opcode = MULIB_DEXCC_DIV; break;
            case 0x70: v.opcode = MULIB_DEXCC_REM; break; case 0x78: v.opcode = MULIB_DEXCC_SHL; break;
            case 0x7a: v.opcode = MULIB_DEXCC_SHR; break; case 0x7c: v.opcode = MULIB_DEXCC_USHR; break;
            case 0x7e: v.opcode = MULIB_DEXCC_AND; break; case 0x80: v.opcode = MULIB_DEXCC_OR; break;
            default: v.opcode = MULIB_DEXCC_XOR; break;
          }
          v.a = v.b = (uint16_t)(top - 2); v.c = (uint16_t)(top - 1);
        }
      else if (i->opcode == 0xa7) { v.opcode = MULIB_DEXCC_GOTO; v.target = i->target; }
      else if (mulib_jvm_cond_ (i->opcode))
        {
          unsigned cond = i->opcode < 0x9f ? i->opcode - 0x99 : i->opcode - 0x9f;
          if (i->opcode < 0x9f) { v.opcode = MULIB_DEXCC_CONST; v.a = (uint16_t)scratch; code[pc++] = v; }
          v = mulib_dexcc_instruction ((mulib_dexcc_opcode_t)(MULIB_DEXCC_IF_EQ + cond));
          v.a = (uint16_t)(top - i->pop); v.b = (uint16_t)(i->pop == 1 ? scratch : top - 1); v.target = i->target;
        }
      else if (i->opcode == 0xac) { v.opcode = MULIB_DEXCC_RETURN; v.a = (uint16_t)(top - 1); }
      code[pc++] = v;
    }
  for (k = 0; k < pc; ++k) if (code[k].target != MULIB_DEXCC_NONE) code[k].target = expanded[code[k].target];
  {
    mulib_dexcc_method_t m; m.code = code; m.count = pc; m.registers = (uint16_t)regs; m.parameters = c->parameters;
    s = mulib_art_compile (&m, limit, out, d);
  }
done:
  if (s != MULIB_MIR_OK) mulib_jvm_error_ (d, s, instructions && k < count ? instructions[k].offset : k);
  free (instructions); free (code); free (map); free (height); free (expanded); return s;
}
const mulib_clickir_program_t *mulib_jvm_graph (const mulib_jvm_method_t *m) { return mulib_art_graph (m); }
mulib_mir_status_t mulib_jvm_jit (mulib_jvm_method_t *m, mulib_mir_diagnostic_t *d) { return mulib_art_jit (m, d); }
mulib_mir_status_t mulib_jvm_frame_init (const mulib_jvm_method_t *m, const int32_t *a, size_t n, mulib_mir_frame_t *f) { return mulib_art_frame_init (m, a, n, f); }
mulib_mir_status_t mulib_jvm_run (const mulib_jvm_method_t *m, mulib_mir_frame_t *f, size_t n) { return mulib_art_run (m, f, n); }
mulib_mir_status_t mulib_jvm_invoke (const mulib_jvm_method_t *m, const int32_t *a, size_t n, size_t b, int32_t *r) { return mulib_art_invoke (m, a, n, b, r); }
mulib_mir_status_t mulib_jvm_emit_c (const mulib_jvm_method_t *m, const char *s, mulib_mir_write_fn w, void *u, mulib_mir_diagnostic_t *d) { return mulib_art_emit_c (m, s, w, u, d); }
mulib_mir_status_t mulib_jvm_destroy (mulib_jvm_method_t **m) { return mulib_art_destroy (m); }
typedef struct mulib_jvm_reader_ { const unsigned char *p; size_t n, at; } mulib_jvm_reader_;
static int mulib_jvm_take_ (mulib_jvm_reader_ *r, size_t n, const unsigned char **p)
{ if (n > r->n - r->at) return 0; if (p) *p = r->p + r->at; r->at += n; return 1; }
static int mulib_jvm_read_ (mulib_jvm_reader_ *r, unsigned n, uint32_t *v)
{ const unsigned char *p; if (!mulib_jvm_take_ (r, n, &p)) return 0; *v = mulib_jvm_be_ (p, n); return 1; }
static int mulib_jvm_utf_ (const unsigned char *p, const size_t *offsets,
    const unsigned char *tags, size_t count, uint32_t id, const char *s)
{
  size_t n = strlen (s); return id && id < count && tags[id] == 1
    && mulib_jvm_be_ (p + offsets[id], 2) == n && !memcmp (p + offsets[id] + 2, s, n);
}
static int mulib_jvm_attributes_ (mulib_jvm_reader_ *r)
{
  uint32_t count, k, name, n;
  if (!mulib_jvm_read_ (r, 2, &count)) return 0;
  for (k = 0; k < count; ++k)
    if (!mulib_jvm_read_ (r, 2, &name) || !mulib_jvm_read_ (r, 4, &n) || !mulib_jvm_take_ (r, n, NULL)) return 0;
  return 1;
}
mulib_mir_status_t mulib_jvm_load_class (const void *data, size_t n,
    const char *name, const char *descriptor, size_t limit,
    mulib_jvm_method_t **out, mulib_mir_diagnostic_t *d)
{
  mulib_jvm_reader_ r; size_t *offsets = NULL, k = 0;
  unsigned char *tags = NULL; int32_t *constants = NULL;
  uint32_t magic, minor, major, cp, value, fields, methods, j;
  mulib_jvm_code_t selected; int found = 0; size_t params = 0;
  mulib_mir_status_t s = MULIB_MIR_INVALID;
  memset (&selected, 0, sizeof (selected)); r.p = (const unsigned char *)data; r.n = n; r.at = 0;
  if (!out || *out || !data || !name || !*name || !descriptor || descriptor[0] != '(') goto done;
  while (descriptor[params + 1] == 'I') { if (++params > 255) goto done; }
  if (strcmp (descriptor + params + 1, ")I")) { s = MULIB_MIR_UNSUPPORTED; goto done; }
  s = MULIB_MIR_RESOURCE_LIMIT; if (n > limit) goto done;
  s = MULIB_MIR_INVALID;
  if (!mulib_jvm_read_ (&r, 4, &magic) || magic != UINT32_C (0xcafebabe)
      || !mulib_jvm_read_ (&r, 2, &minor) || !mulib_jvm_read_ (&r, 2, &major)
      || !mulib_jvm_read_ (&r, 2, &cp) || !cp) goto done;
  (void)minor;
  if (major < 45 || major > 71) { s = MULIB_MIR_UNSUPPORTED; goto done; }
  if (cp > limit || SIZE_MAX / cp < sizeof (*offsets)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
  offsets = (size_t *)calloc (cp, sizeof (*offsets)); tags = (unsigned char *)calloc (cp, 1);
  constants = (int32_t *)calloc (cp, sizeof (*constants));
  s = MULIB_MIR_NO_MEMORY; if (!offsets || !tags || !constants) goto done;
  s = MULIB_MIR_INVALID;
  for (k = 1; k < cp; ++k)
    {
      uint32_t tag, len; size_t width;
      if (!mulib_jvm_read_ (&r, 1, &tag)) goto done;
      tags[k] = (unsigned char)tag; offsets[k] = r.at;
      if (tag == 1) { if (!mulib_jvm_read_ (&r, 2, &len) || !mulib_jvm_take_ (&r, len, NULL)) goto done; continue; }
      if (tag == 3) { if (!mulib_jvm_read_ (&r, 4, &value)) goto done; constants[k] = mulib_jvm_signed_ (value); continue; }
      if (tag == 4 || tag == 9 || tag == 10 || tag == 11 || tag == 12 || tag == 17 || tag == 18) width = 4;
      else if (tag == 5 || tag == 6) { width = 8; if (k + 1 >= cp) goto done; ++k; }
      else if (tag == 7 || tag == 8 || tag == 16 || tag == 19 || tag == 20) width = 2;
      else if (tag == 15) width = 3;
      else { s = MULIB_MIR_UNSUPPORTED; goto done; }
      if (!mulib_jvm_take_ (&r, width, NULL)) goto done;
    }
  if (!mulib_jvm_take_ (&r, 6, NULL) || !mulib_jvm_read_ (&r, 2, &value)
      || !mulib_jvm_take_ (&r, (size_t)value * 2, NULL) || !mulib_jvm_read_ (&r, 2, &fields)) goto done;
  for (j = 0; j < fields; ++j) if (!mulib_jvm_take_ (&r, 6, NULL) || !mulib_jvm_attributes_ (&r)) goto done;
  if (!mulib_jvm_read_ (&r, 2, &methods)) goto done;
  for (j = 0; j < methods; ++j)
    {
      uint32_t flags, method_name, desc, attributes, a; int match, code_seen = 0;
      if (!mulib_jvm_read_ (&r, 2, &flags) || !mulib_jvm_read_ (&r, 2, &method_name)
          || !mulib_jvm_read_ (&r, 2, &desc) || !mulib_jvm_read_ (&r, 2, &attributes)) goto done;
      match = mulib_jvm_utf_ (r.p, offsets, tags, cp, method_name, name) && mulib_jvm_utf_ (r.p, offsets, tags, cp, desc, descriptor);
      if (match) { if (found) goto done; found = 1; if (!(flags & 8) || (flags & (0x100u | 0x400u | 0x20u))) { s = MULIB_MIR_UNSUPPORTED; goto done; } }
      for (a = 0; a < attributes; ++a)
        {
          uint32_t attr, len; const unsigned char *bytes;
          if (!mulib_jvm_read_ (&r, 2, &attr) || !mulib_jvm_read_ (&r, 4, &len) || !mulib_jvm_take_ (&r, len, &bytes)) goto done;
          if (match && mulib_jvm_utf_ (r.p, offsets, tags, cp, attr, "Code"))
            {
              mulib_jvm_reader_ cr; uint32_t stack, locals, size, exceptions;
              if (code_seen++) goto done;
              cr.p = bytes; cr.n = len; cr.at = 0;
              if (!mulib_jvm_read_ (&cr, 2, &stack) || !mulib_jvm_read_ (&cr, 2, &locals)
                  || !mulib_jvm_read_ (&cr, 4, &size) || !size || size >= 65536
                  || !mulib_jvm_take_ (&cr, size, &selected.bytes) || !mulib_jvm_read_ (&cr, 2, &exceptions)) goto done;
              if (exceptions) { s = MULIB_MIR_UNSUPPORTED; goto done; }
              if (!mulib_jvm_attributes_ (&cr) || cr.at != cr.n) goto done;
              selected.length = size; selected.max_stack = (uint16_t)stack; selected.max_locals = (uint16_t)locals; selected.parameters = (uint16_t)params;
            }
        }
      if (match && !code_seen) goto done;
    }
  if (!mulib_jvm_attributes_ (&r) || r.at != r.n) goto done;
  if (!found) { s = MULIB_MIR_UNSUPPORTED; goto done; }
  selected.integer_constants = constants; selected.constant_tags = tags; selected.constant_count = cp;
  s = mulib_jvm_load_code (&selected, limit, out, d);
done:
  free (offsets); free (tags); free (constants);
  if (s != MULIB_MIR_OK) return mulib_jvm_error_ (d, s, r.at);
  return s;
}
#ifdef __cplusplus
}
#endif
#endif
