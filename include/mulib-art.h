/** @file mulib-art.h
 * @brief Small verified DEX integer-method runtime through ClickIR, with JIT/AOT.
 * Define MULIB_IMPL_ART and the explicit dependency switches in the manual.
 * This is not AOSP ART, dex2oat, an Android framework or an APK runtime. */
#ifndef MULIB_ART_H
#define MULIB_ART_H
#include "mulib-dexcc.h"
#include "mulib-clickir.h"
#include "mulib-jit.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_art_method mulib_art_method_t;
/** Compile a verified static int method to owned ClickIR and executable MIR.
 * All inputs are copied; *out must be NULL. limit bounds all intermediate
 * arrays, including SSA PHIs, CFG-squared cells and register/block products. */
mulib_mir_status_t mulib_art_compile (const mulib_dexcc_method_t *method,
    size_t limit, mulib_art_method_t **out, mulib_mir_diagnostic_t *diagnostic);
/** Load raw little-endian insns for a static (I...I)I method. */
mulib_mir_status_t mulib_art_load_code (const void *bytes, size_t length,
    uint16_t registers, uint16_t parameters, size_t limit,
    mulib_art_method_t **out, mulib_mir_diagnostic_t *diagnostic);
/** Select a static integer method from a DEX035..040 file. Checks envelope
 * integrity and all metadata used by this method; unrelated methods are not
 * verified. Rejects handlers, native/abstract methods and unsupported bytecode. */
mulib_mir_status_t mulib_art_load_dex (const void *bytes, size_t length,
    const char *class_descriptor, const char *method_name, uint16_t parameters,
    size_t limit, mulib_art_method_t **out, mulib_mir_diagnostic_t *diagnostic);
/** Borrow immutable compiler graph, valid until destroy. */
const mulib_clickir_program_t *mulib_art_graph (const mulib_art_method_t *method);
/** Explicitly compile native baseline JIT (Linux AMD64); no interpreted fallback
 * on unsupported hosts or denied executable memory. Existing frames remain valid.
 * Do not compile/destroy concurrently with execution. */
mulib_mir_status_t mulib_art_jit (mulib_art_method_t *method,
    mulib_mir_diagnostic_t *diagnostic);
/** Image for newly initialized frames; chooses JIT if explicitly compiled. */
const mulib_mir_flow_program_t *mulib_art_program (const mulib_art_method_t *method);
/** Copy Java int arguments into a zero-initialized resumable frame. The method
 * must outlive it; release with mulib_mir_frame_free. */
mulib_mir_status_t mulib_art_frame_init (const mulib_art_method_t *method,
    const int32_t *arguments, size_t count, mulib_mir_frame_t *frame);
/** Resume for budget MIR operations (not bytecodes). FINISHED, STEP_LIMIT or
 * arithmetic fault; zero budget has no effects. Independent frames may share. */
mulib_mir_status_t mulib_art_run (const mulib_art_method_t *method,
    mulib_mir_frame_t *frame, size_t budget);
/** Convenience single invocation. *result unchanged except on FINISHED. */
mulib_mir_status_t mulib_art_invoke (const mulib_art_method_t *method,
    const int32_t *arguments, size_t count, size_t budget, int32_t *result);
/** Emit portable C11 baseline AOT using the same validated ClickIR lowering.
 * symbol begins mulib_generated_; generated symbol_program() initializes frames.
 * Writer failures may leave partial text. No external compiler is spawned. */
mulib_mir_status_t mulib_art_emit_c (const mulib_art_method_t *method,
    const char *symbol, mulib_mir_write_fn writer, void *user,
    mulib_mir_diagnostic_t *diagnostic);
/** Release all graphs/images; zero *method. On OS unmap failure retain owner.
 * All frames must already be released. NULL is harmless. */
mulib_mir_status_t mulib_art_destroy (mulib_art_method_t **method);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ART) && !defined(MULIB_ART_IMPLEMENTED)
#define MULIB_ART_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_art_method {
  mulib_clickir_buffer_t graph;
  mulib_clickir_flow_t flow;
  mulib_mir_type_t *parameters;
  mulib_jit_flow_t *jit;
  size_t limit;
};
static mulib_mir_status_t mulib_art_error_ (mulib_mir_diagnostic_t *d,
    mulib_mir_status_t s, size_t k)
{ if (d) { d->status = s; d->record = k; } return s; }
static mulib_mir_status_t mulib_art_dex_status_ (mulib_dexcc_status_t s)
{
  switch (s) {
    case MULIB_DEXCC_OK: return MULIB_MIR_OK;
    case MULIB_DEXCC_UNSUPPORTED: return MULIB_MIR_UNSUPPORTED;
    case MULIB_DEXCC_LIMIT: return MULIB_MIR_RESOURCE_LIMIT;
    case MULIB_DEXCC_NOMEM: return MULIB_MIR_NO_MEMORY;
    case MULIB_DEXCC_UNINITIALIZED: case MULIB_DEXCC_BAD_TARGET: return MULIB_MIR_BAD_REFERENCE;
    default: return MULIB_MIR_INVALID;
  }
}
const mulib_clickir_program_t *mulib_art_graph (const mulib_art_method_t *m)
{ return m ? &m->graph.program : NULL; }
const mulib_mir_flow_program_t *mulib_art_program (const mulib_art_method_t *m)
{ return !m ? NULL : m->jit ? mulib_jit_flow_program (m->jit) : &m->flow.program; }
mulib_mir_status_t mulib_art_destroy (mulib_art_method_t **p)
{
  mulib_art_method_t *m;
  if (!p || !*p) return MULIB_MIR_OK;
  m = *p;
  if (mulib_jit_flow_destroy (m->jit) != MULIB_JIT_OK) return MULIB_MIR_HOST_ERROR;
  mulib_clickir_free (&m->graph); mulib_clickir_flow_free (&m->flow);
  free (m->parameters); free (m); *p = NULL; return MULIB_MIR_OK;
}
static void mulib_art_native_ (mulib_mir_native_buffer_t *b, size_t *pc,
    mulib_mir_native_opcode_t op, size_t dest, size_t a, size_t c, uint64_t imm)
{
  mulib_mir_native_instruction_t i = mulib_mir_native_instruction (op);
  i.destination = dest; i.a = a; i.b = c; i.immediate = imm; b->code[(*pc)++] = i;
}
static void mulib_art_int_ (mulib_mir_native_buffer_t *b, size_t *pc,
    size_t reg, size_t mask, size_t sign)
{
  mulib_art_native_ (b, pc, MULIB_MIR_NATIVE_AND, reg, reg, mask, 0);
  mulib_art_native_ (b, pc, MULIB_MIR_NATIVE_XOR, reg, reg, sign, 0);
  mulib_art_native_ (b, pc, MULIB_MIR_NATIVE_SUB, reg, reg, sign, 0);
}
mulib_mir_status_t mulib_art_compile (const mulib_dexcc_method_t *source,
    size_t limit, mulib_art_method_t **out, mulib_mir_diagnostic_t *d)
{
  mulib_art_method_t *m = NULL;
  mulib_mir_native_buffer_t n; mulib_mir_native_program_t p;
  mulib_mir_type_t *types = NULL; mulib_mir_signature_t sig;
  mulib_dexcc_diagnostic_t dd;
  mulib_mir_status_t s; size_t k = 0, pc = 0, nr, mask, sign, zero, shift, temp, boolean, max;
  memset (&n, 0, sizeof (n));
  if (!out || *out) return mulib_art_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_art_dex_status_ (mulib_dexcc_validate (source, limit, &dd));
  if (s != MULIB_MIR_OK) return mulib_art_error_ (d, s, dd.instruction);
  s = MULIB_MIR_RESOURCE_LIMIT;
  nr = (size_t)source->registers + 6;
  if (nr > limit || source->count >= limit || source->parameters > (limit - 5) / 4
      || source->count > (limit - 5 - (size_t)source->parameters * 4) / 8) goto done;
  max = 5 + (size_t)source->parameters * 4 + source->count * 8;
  if (nr > SIZE_MAX / sizeof (*types)) goto done;
  m = (mulib_art_method_t *)calloc (1, sizeof (*m));
  types = (mulib_mir_type_t *)malloc (nr * sizeof (*types));
  s = MULIB_MIR_NO_MEMORY; if (!m || !types) goto done;
  if (source->parameters) m->parameters = (mulib_mir_type_t *)malloc ((size_t)source->parameters * sizeof (*m->parameters));
  if (source->parameters && !m->parameters) goto done;
  m->limit = limit;
  for (k = 0; k < source->parameters; ++k) m->parameters[k] = MULIB_MIR_I64;
  for (k = 0; k < nr; ++k) types[k] = MULIB_MIR_I64;
  mask = source->registers; sign = mask + 1; zero = mask + 2; shift = mask + 3; temp = mask + 4; boolean = mask + 5;
  types[boolean] = MULIB_MIR_BOOL;
  s = mulib_mir_native_init (&n, max, source->count + 1, nr, source->parameters, 0, 0, limit);
  if (s != MULIB_MIR_OK) goto done;
  mulib_art_native_ (&n, &pc, MULIB_MIR_NATIVE_CONST, mask, MULIB_MIR_NONE, MULIB_MIR_NONE, UINT32_MAX);
  mulib_art_native_ (&n, &pc, MULIB_MIR_NATIVE_CONST, sign, MULIB_MIR_NONE, MULIB_MIR_NONE, UINT64_C (0x80000000));
  mulib_art_native_ (&n, &pc, MULIB_MIR_NATIVE_CONST, zero, MULIB_MIR_NONE, MULIB_MIR_NONE, 0);
  mulib_art_native_ (&n, &pc, MULIB_MIR_NATIVE_CONST, shift, MULIB_MIR_NONE, MULIB_MIR_NONE, 31);
  for (k = 0; k < source->parameters; ++k)
    {
      size_t reg = source->registers - source->parameters + k;
      mulib_art_native_ (&n, &pc, MULIB_MIR_NATIVE_PARAM, reg, k, MULIB_MIR_NONE, 0);
      mulib_art_int_ (&n, &pc, reg, mask, sign);
    }
  n.code[pc] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_JUMP); n.code[pc++].target = 1;
  n.blocks[0].first = 0; n.blocks[0].count = pc;
  for (k = 0; k < source->count; ++k)
    {
      const mulib_dexcc_instruction_t *i = &source->code[k];
      int op = i->opcode, normalize = 0;
      mulib_mir_native_instruction_t term = mulib_mir_native_instruction (MULIB_MIR_NATIVE_JUMP);
      term.target = k + 2; n.blocks[k + 1].first = pc;
      if (op == MULIB_DEXCC_CONST)
        mulib_art_native_ (&n, &pc, MULIB_MIR_NATIVE_CONST, i->a, MULIB_MIR_NONE, MULIB_MIR_NONE, (uint64_t)(int64_t)i->literal);
      else if (op == MULIB_DEXCC_MOVE)
        mulib_art_native_ (&n, &pc, MULIB_MIR_NATIVE_COPY, i->a, i->b, MULIB_MIR_NONE, 0);
      else if (op == MULIB_DEXCC_NEG || op == MULIB_DEXCC_NOT)
        {
          mulib_art_native_ (&n, &pc, op == MULIB_DEXCC_NEG ? MULIB_MIR_NATIVE_SUB : MULIB_MIR_NATIVE_XOR,
              i->a, op == MULIB_DEXCC_NEG ? zero : i->b, op == MULIB_DEXCC_NEG ? i->b : mask, 0);
          normalize = 1;
        }
      else if (op >= MULIB_DEXCC_ADD && op <= MULIB_DEXCC_USHR)
        {
          static const mulib_mir_native_opcode_t ops[] = {
            MULIB_MIR_NATIVE_ADD, MULIB_MIR_NATIVE_SUB, MULIB_MIR_NATIVE_MUL,
            MULIB_MIR_NATIVE_SDIV, MULIB_MIR_NATIVE_SREM, MULIB_MIR_NATIVE_AND,
            MULIB_MIR_NATIVE_OR, MULIB_MIR_NATIVE_XOR, MULIB_MIR_NATIVE_SHL,
            MULIB_MIR_NATIVE_SAR, MULIB_MIR_NATIVE_SHR
          };
          size_t a = i->b, c = i->c;
          if (op >= MULIB_DEXCC_SHL)
            { mulib_art_native_ (&n, &pc, MULIB_MIR_NATIVE_AND, temp, c, shift, 0); c = temp; }
          if (op == MULIB_DEXCC_USHR)
            { mulib_art_native_ (&n, &pc, MULIB_MIR_NATIVE_AND, i->a, a, mask, 0); a = i->a; }
          mulib_art_native_ (&n, &pc, ops[op - MULIB_DEXCC_ADD], i->a, a, c, 0); normalize = 1;
        }
      else if (op == MULIB_DEXCC_GOTO) term.target = i->target + 1;
      else if (op >= MULIB_DEXCC_IF_EQ && op <= MULIB_DEXCC_IF_LE)
        {
          int invert = op == MULIB_DEXCC_IF_NE || op == MULIB_DEXCC_IF_GE || op == MULIB_DEXCC_IF_LE;
          size_t a = i->a, c = i->b;
          if (op == MULIB_DEXCC_IF_GT || op == MULIB_DEXCC_IF_LE) { a = i->b; c = i->a; }
          mulib_art_native_ (&n, &pc, op <= MULIB_DEXCC_IF_NE ? MULIB_MIR_NATIVE_EQ : MULIB_MIR_NATIVE_SLT, boolean, a, c, 0);
          term.opcode = MULIB_MIR_NATIVE_BRANCH; term.a = boolean;
          term.target = invert ? k + 2 : i->target + 1; term.b = invert ? i->target + 1 : k + 2;
        }
      else if (op == MULIB_DEXCC_RETURN)
        { term = mulib_mir_native_instruction (MULIB_MIR_NATIVE_RETURN); term.a = i->a; }
      if (normalize) mulib_art_int_ (&n, &pc, i->a, mask, sign);
      n.code[pc++] = term; n.blocks[k + 1].count = pc - n.blocks[k + 1].first;
    }
  n.count = pc; p = mulib_mir_native_view (&n);
  sig.parameters = m->parameters; sig.parameter_count = source->parameters; sig.result = MULIB_MIR_I64;
  s = mulib_clickir_from_native (&p, types, sig, limit, &m->graph, d);
  if (s == MULIB_MIR_OK) s = mulib_clickir_to_flow (&m->graph.program, limit, &m->flow, d);
  if (s == MULIB_MIR_OK) { *out = m; m = NULL; }
done:
  free (types); mulib_mir_native_free (&n); (void)mulib_art_destroy (&m);
  return mulib_art_error_ (d, s, k);
}
mulib_mir_status_t mulib_art_load_code (const void *bytes, size_t length,
    uint16_t regs, uint16_t params, size_t limit, mulib_art_method_t **out,
    mulib_mir_diagnostic_t *d)
{
  mulib_dexcc_buffer_t b = {0}; mulib_dexcc_diagnostic_t dd; mulib_mir_status_t s;
  if (!out || *out) return mulib_art_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_art_dex_status_ (mulib_dexcc_decode (bytes, length, regs, params, limit, &b, &dd));
  if (s == MULIB_MIR_OK) s = mulib_art_compile (&b.method, limit, out, d);
  else mulib_art_error_ (d, s, dd.instruction);
  mulib_dexcc_free (&b); return s;
}
mulib_mir_status_t mulib_art_jit (mulib_art_method_t *m, mulib_mir_diagnostic_t *d)
{
  int s;
  mulib_mir_diagnostic_t local = {MULIB_MIR_OK, 0};
  if (!d) d = &local;
  if (!m) return mulib_art_error_ (d, MULIB_MIR_INVALID, 0);
  if (m->jit) return mulib_art_error_ (d, MULIB_MIR_OK, 0);
  s = mulib_jit_flow_compile (&m->flow.program, m->limit, 0, &m->jit, d);
  if (s == MULIB_JIT_EMIR) return d->status;
  return mulib_art_error_ (d, s == MULIB_JIT_OK ? MULIB_MIR_OK : s == MULIB_JIT_ENOTSUP
      ? MULIB_MIR_UNSUPPORTED : s == MULIB_JIT_ENOMEM ? MULIB_MIR_NO_MEMORY : MULIB_MIR_HOST_ERROR, 0);
}
mulib_mir_status_t mulib_art_frame_init (const mulib_art_method_t *m,
    const int32_t *args, size_t count, mulib_mir_frame_t *f)
{
  mulib_mir_value_t *values = NULL; size_t k; mulib_mir_status_t s;
  const mulib_mir_flow_program_t *p = mulib_art_program (m);
  if (!p || count != p->signature.parameter_count || (count && !args)) return MULIB_MIR_INVALID;
  if (count) { values = (mulib_mir_value_t *)calloc (count, sizeof (*values)); if (!values) return MULIB_MIR_NO_MEMORY; }
  for (k = 0; k < count; ++k) values[k].integer = args[k];
  s = mulib_mir_frame_init (f, p, values, count, NULL, NULL, m->limit, NULL); free (values); return s;
}
mulib_mir_status_t mulib_art_run (const mulib_art_method_t *m, mulib_mir_frame_t *f, size_t budget)
{
  if (!m || !f || !f->program) return MULIB_MIR_INVALID;
  if (m->jit && f->program == mulib_jit_flow_program (m->jit)) return mulib_jit_flow_run (m->jit, f, budget);
  if (f->program != &m->flow.program) return MULIB_MIR_INVALID;
  return mulib_vmweave_run (f, budget);
}
mulib_mir_status_t mulib_art_invoke (const mulib_art_method_t *m,
    const int32_t *args, size_t count, size_t budget, int32_t *result)
{
  mulib_mir_frame_t f = {0}; mulib_mir_status_t s;
  if (!result) return MULIB_MIR_INVALID;
  s = mulib_art_frame_init (m, args, count, &f);
  if (s == MULIB_MIR_OK) s = mulib_art_run (m, &f, budget);
  if (s == MULIB_MIR_FINISHED) *result = (int32_t)f.result.integer;
  mulib_mir_frame_free (&f); return s;
}
mulib_mir_status_t mulib_art_emit_c (const mulib_art_method_t *m, const char *name,
    mulib_mir_write_fn writer, void *user, mulib_mir_diagnostic_t *d)
{
  if (!m) return mulib_art_error_ (d, MULIB_MIR_INVALID, 0);
  return mulib_mir_flow_emit_c (&m->flow.program, name, m->limit, writer, user, d);
}
static uint32_t mulib_art_u_ (const unsigned char *p, unsigned n)
{ uint32_t v = 0; unsigned k; for (k = 0; k < n; ++k) v |= (uint32_t)p[k] << (k * 8); return v; }
static int mulib_art_leb_ (const unsigned char *p, size_t n, size_t *at, uint32_t *v)
{
  unsigned k; uint32_t value = 0;
  for (k = 0; k < 5; ++k)
    { unsigned c; if (*at >= n) return 0; c = p[(*at)++]; if (k == 4 && (c & 240)) return 0;
      value |= (uint32_t)(c & 127) << (k * 7); if (!(c & 128)) { *v = value; return 1; } }
  return 0;
}
static int mulib_art_string_ (const unsigned char *p, size_t n, uint32_t id, const char *text)
{
  uint32_t count = mulib_art_u_ (p + 56, 4), off = mulib_art_u_ (p + 60, 4), len;
  size_t at, bytes;
  if (id >= count) return 0;
  at = mulib_art_u_ (p + off + (size_t)id * 4, 4);
  if (at < mulib_art_u_ (p + 108, 4) || !mulib_art_leb_ (p, n, &at, &len)) return 0;
  bytes = strlen (text);
  return bytes == len && bytes < n - at && !memcmp (p + at, text, bytes) && !p[at + bytes];
}
static int mulib_art_type_ (const unsigned char *p, size_t n, uint32_t id, const char *text)
{
  uint32_t count = mulib_art_u_ (p + 64, 4), off = mulib_art_u_ (p + 68, 4);
  return id < count && mulib_art_string_ (p, n, mulib_art_u_ (p + off + (size_t)id * 4, 4), text);
}
static int mulib_art_shorty_ (const unsigned char *p, size_t n, uint32_t id, uint16_t params)
{
  uint32_t count = mulib_art_u_ (p + 56, 4), off = mulib_art_u_ (p + 60, 4), len;
  size_t at, k;
  if (id >= count) return 0;
  at = mulib_art_u_ (p + off + (size_t)id * 4, 4);
  if (at < mulib_art_u_ (p + 108, 4) || !mulib_art_leb_ (p, n, &at, &len)
      || len != (uint32_t)params + 1 || len >= n - at || p[at + len]) return 0;
  for (k = 0; k < len; ++k) if (p[at + k] != 'I') return 0;
  return 1;
}
mulib_mir_status_t mulib_art_load_dex (const void *data, size_t n, const char *cls,
    const char *name, uint16_t params, size_t limit, mulib_art_method_t **out,
    mulib_mir_diagnostic_t *d)
{
  const unsigned char *p = (const unsigned char *)data;
  uint32_t methods, moff, classes, coff, found = UINT32_MAX, class_id = 0, data_off;
  size_t k, at; mulib_dexcc_diagnostic_t dd; mulib_mir_status_t s;
  if (!out || *out || !cls || !name) return mulib_art_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_art_dex_status_ (mulib_dexcc_check_file (data, n, limit, &dd));
  if (s != MULIB_MIR_OK) return mulib_art_error_ (d, s, dd.instruction);
  methods = mulib_art_u_ (p + 88, 4); moff = mulib_art_u_ (p + 92, 4);
  classes = mulib_art_u_ (p + 96, 4); coff = mulib_art_u_ (p + 100, 4); data_off = mulib_art_u_ (p + 108, 4);
  for (k = 0; k < methods; ++k)
    {
      const unsigned char *id = p + moff + k * 8;
      uint32_t proto, poff, list, j;
      if (!mulib_art_type_ (p, n, mulib_art_u_ (id, 2), cls) || !mulib_art_string_ (p, n, mulib_art_u_ (id + 4, 4), name)) continue;
      proto = mulib_art_u_ (id + 2, 2); if (proto >= mulib_art_u_ (p + 72, 4)) return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, k);
      poff = mulib_art_u_ (p + 76, 4) + proto * 12;
      if (!mulib_art_type_ (p, n, mulib_art_u_ (p + poff + 4, 4), "I")) continue;
      list = mulib_art_u_ (p + poff + 8, 4);
      if (!list) { if (params) continue; }
      else {
        if (list < data_off || list % 4 || list > n - 4 || mulib_art_u_ (p + list, 4) > (n - list - 4) / 2)
          return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, list);
        if (mulib_art_u_ (p + list, 4) != params) continue;
        for (j = 0; j < params; ++j) if (!mulib_art_type_ (p, n, mulib_art_u_ (p + list + 4 + (size_t)j * 2, 2), "I")) break;
        if (j != params) continue;
      }
      if (!mulib_art_shorty_ (p, n, mulib_art_u_ (p + poff, 4), params))
        return mulib_art_error_ (d, MULIB_MIR_BAD_TYPE, poff);
      found = (uint32_t)k; class_id = mulib_art_u_ (id, 2); break;
    }
  if (found == UINT32_MAX) return mulib_art_error_ (d, MULIB_MIR_UNSUPPORTED, 0);
  for (k = 0; k < classes; ++k) if (mulib_art_u_ (p + coff + k * 32, 4) == class_id) break;
  if (k == classes) return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, found);
  at = mulib_art_u_ (p + coff + k * 32 + 24, 4);
  if (at < data_off || at >= n) return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, at);
  {
    uint32_t counts[4], j, group;
    for (j = 0; j < 4; ++j) if (!mulib_art_leb_ (p, n, &at, &counts[j]) || counts[j] > limit) return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, at);
    for (group = 0; group < 4; ++group)
      {
        uint32_t index = 0;
        for (j = 0; j < counts[group]; ++j)
          {
            uint32_t delta, flags, code;
            if (!mulib_art_leb_ (p, n, &at, &delta) || delta > UINT32_MAX - index
                || (j && !delta) || !mulib_art_leb_ (p, n, &at, &flags)) return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, at);
            index += delta;
            if (group < 2) continue;
            if (index >= methods || !mulib_art_leb_ (p, n, &at, &code)) return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, at);
            if (index == found)
              {
                uint32_t units;
                if (!(flags & 8) || (flags & (0x20u | 0x100u | 0x400u)) || group != 2)
                  return mulib_art_error_ (d, MULIB_MIR_UNSUPPORTED, at);
                if (code < data_off || code % 4 || code > n - 16) return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, code);
                if (mulib_art_u_ (p + code + 6, 2)) return mulib_art_error_ (d, MULIB_MIR_UNSUPPORTED, code);
                if (mulib_art_u_ (p + code + 2, 2) != params) return mulib_art_error_ (d, MULIB_MIR_BAD_TYPE, code);
                units = mulib_art_u_ (p + code + 12, 4);
                if (units > (n - code - 16) / 2) return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, code);
                return mulib_art_load_code (p + code + 16, (size_t)units * 2, (uint16_t)mulib_art_u_ (p + code, 2), params, limit, out, d);
              }
          }
      }
  }
  return mulib_art_error_ (d, MULIB_MIR_BAD_REFERENCE, found);
}
#ifdef __cplusplus
}
#endif
#endif
