/** @file mulib-optml.h
 * @brief Annotation-driven LIR tree rewrites, reselection and assembly output.
 * Define MULIB_IMPL_OPTML and all dependencies listed in manual/mulib-optml.md.
 */
#ifndef MULIB_OPTML_H
#define MULIB_OPTML_H
#include "mulib-isel.h"
#include "mulib-peephole.h"
#include "mulib-superinstr.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Owned ordered subset of the trusted peephole catalogue. Zero initialize. */
typedef struct mulib_optml_rules {
  mulib_peephole_rule_t *rules;
  size_t count;
} mulib_optml_rules_t;
/** Parse a length-delimited OptML policy: one built-in rule name per line;
 * blank lines, ASCII whitespace and # comments are allowed. Unknown/duplicate
 * names fail. limit bounds source bytes and rules. Empty policies are valid. */
mulib_mir_status_t mulib_optml_parse (const char *, size_t length, size_t limit,
    mulib_optml_rules_t *, size_t *error_offset);
/** Release a parsed rule policy. Names remain borrowed from the static catalogue. */
void mulib_optml_rules_free (mulib_optml_rules_t *);
typedef struct mulib_optml_options {
  mulib_isel_options_t selection;
  const mulib_peephole_rule_t *rules;
  size_t rule_count, passes, text_limit;
  int dead_writes, superinstructions;
  mulib_superinstr_options_t super;
} mulib_optml_options_t;
typedef struct mulib_optml_stats {
  size_t passes, rewrites, dead_writes, macros, macro_uses;
} mulib_optml_stats_t;
typedef struct mulib_optml_result {
  mulib_lir_t lir;
  char *assembly;
  size_t length;
  mulib_optml_stats_t stats;
} mulib_optml_result_t;
/** Default trusted catalogue, eight bounded passes, dead-write removal, macro
 * factoring, 1 MiB assembly. Zero passes selects/schedules without rewriting. */
mulib_optml_options_t mulib_optml_defaults (void);
/** Optimize an immutable LIR into an empty owned output. Each pass consumes
 * proven constants and constraints, matches only stable reaching-definition
 * trees, removes pure dead writes, then reselects/reschedules. Memory, calls and
 * KEEP/VOLATILE/BARRIER records are never removed. Failure preserves output/stats.
 * Hitting the pass bound returns a valid partially optimized program. */
mulib_mir_status_t mulib_optml_run (const mulib_lir_t *, const mulib_optml_options_t *,
    mulib_lir_t *, mulib_optml_stats_t *, mulib_mir_diagnostic_t *);
/** Emit selected LIR through its target's ABI-aware assembly backend. */
mulib_mir_status_t mulib_optml_emit (const mulib_lir_t *, const char *symbol,
    size_t limit, mulib_mir_write_fn, void *user, mulib_mir_diagnostic_t *);
/** Complete native MIR -> selection -> annotated LIR -> optimization -> assembly
 * pipeline. Transactional owned output; no file or executable mapping is created. */
mulib_mir_status_t mulib_optml_compile (const mulib_mir_native_program_t *, const char *symbol,
    const mulib_optml_options_t *, mulib_optml_result_t *, mulib_mir_diagnostic_t *);
/** Write final assembly to a caller sink (file, buffer or writable mapping). */
mulib_mir_status_t mulib_optml_write (const mulib_optml_result_t *, mulib_mir_write_fn, void *user);
/** Release the complete compilation artifact. */
void mulib_optml_free (mulib_optml_result_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OPTML) && !defined(MULIB_OPTML_IMPLEMENTED)
#define MULIB_OPTML_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_optml_rules_free (mulib_optml_rules_t *r)
{ if (r) { free (r->rules); memset (r, 0, sizeof (*r)); } }
static int mulib_optml_space_ (unsigned char c)
{ return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
mulib_mir_status_t mulib_optml_parse (const char *text, size_t length, size_t limit,
    mulib_optml_rules_t *out, size_t *error_offset)
{
  mulib_optml_rules_t r = {NULL, 0};
  size_t count, pos = 0;
  const mulib_peephole_rule_t *all = mulib_peephole_rules (&count);
  mulib_mir_status_t s = MULIB_MIR_INVALID;
  if (!out || out->rules || out->count || (!text && length)) goto done;
  s = MULIB_MIR_RESOURCE_LIMIT; if (length > limit || count > SIZE_MAX / sizeof (*r.rules)) goto done;
  r.rules = (mulib_peephole_rule_t *)calloc (count, sizeof (*r.rules));
  s = MULIB_MIR_NO_MEMORY; if (!r.rules) goto done;
  s = MULIB_MIR_INVALID;
  while (pos < length) {
    size_t first, found = count;
    while (pos < length && mulib_optml_space_ ((unsigned char)text[pos])) ++pos;
    if (pos == length) break;
    if (text[pos] == '#') { while (pos < length && text[pos] != '\n') ++pos; continue; }
    first = pos;
    while (pos < length && !mulib_optml_space_ ((unsigned char)text[pos]) && text[pos] != '#') ++pos;
    for (size_t j = 0; j < count; ++j)
      if (strlen (all[j].name) == pos - first && !memcmp (all[j].name, text + first, pos - first)) { found = j; break; }
    if (found == count) { pos = first; goto done; }
    for (size_t j = 0; j < r.count; ++j) if (r.rules[j].name == all[found].name) { pos = first; goto done; }
    if (r.count == limit) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
    r.rules[r.count++] = all[found];
    while (pos < length && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r')) ++pos;
    if (pos < length && text[pos] != '\n' && text[pos] != '#') goto done;
  }
  *out = r; memset (&r, 0, sizeof (r)); s = MULIB_MIR_OK;
done:
  mulib_optml_rules_free (&r); if (error_offset) *error_offset = pos; return s;
}
mulib_optml_options_t mulib_optml_defaults (void)
{
  mulib_optml_options_t o;
  o.selection = mulib_isel_defaults (); o.rules = mulib_peephole_rules (&o.rule_count);
  o.passes = 8; o.text_limit = 1048576; o.dead_writes = o.superinstructions = 1;
  o.super = mulib_superinstr_defaults (); return o;
}
static const mulib_mir_native_instruction_t *mulib_optml_tree_ (
    const mulib_lir_t *r, const size_t *last, size_t reg)
{
  size_t def = last[reg];
  const mulib_mir_native_instruction_t *i;
  if (def == SIZE_MAX || r->annotations[def].constraints) return NULL;
  i = &r->body.code[def];
  if (i->opcode < MULIB_MIR_NATIVE_ADD || i->opcode > MULIB_MIR_NATIVE_ULT) return NULL;
  /* Include the definition itself: x=x+y does not leave its old x available. */
  if ((last[i->a] != SIZE_MAX && last[i->a] >= def)
      || (last[i->b] != SIZE_MAX && last[i->b] >= def)) return NULL;
  return i;
}
static int mulib_optml_charge_ (size_t *remaining, size_t amount)
{
  if (amount > *remaining) return 0;
  *remaining -= amount; return 1;
}
static mulib_mir_status_t mulib_optml_pass_ (mulib_lir_t *r, const mulib_optml_options_t *o,
    mulib_optml_stats_t *stats, int *changed)
{
  size_t *last = NULL, n = r->body.register_count, work = o->selection.work_limit;
  unsigned char *used = NULL, *live = NULL;
  mulib_mir_status_t s = MULIB_MIR_RESOURCE_LIMIT;
  mulib_mir_native_program_t p = mulib_mir_native_view (&r->body);
  if (n > SIZE_MAX / sizeof (*last)) return s;
  if (n) {
    last = (size_t *)malloc (n * sizeof (*last)); used = (unsigned char *)calloc (n, 1);
    live = (unsigned char *)calloc (n, 1);
    if (!last || !used || !live) { s = MULIB_MIR_NO_MEMORY; goto done; }
  }
  for (size_t b = 0; b < p.block_count; ++b) {
    if (!mulib_optml_charge_ (&work, n)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
    for (size_t j = 0; j < n; ++j) last[j] = SIZE_MAX;
    for (size_t k = p.blocks[b].first; k < p.blocks[b].first + p.blocks[b].count; ++k) {
      mulib_mir_native_instruction_t *i = &r->body.code[k];
      mulib_lir_annotation_t *a = &r->annotations[k];
      mulib_peephole_facts_t f;
      int arity = mulib_mir_native_arity (i->opcode), matched;
      if (!mulib_optml_charge_ (&work, 1) || o->rule_count > work / 2
          || !mulib_optml_charge_ (&work, o->rule_count * 2)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
      f.known = a->known_mask; f.values[0] = a->values[0]; f.values[1] = a->values[1];
      f.left = arity > 0 ? mulib_optml_tree_ (r, last, i->a) : NULL;
      f.right = arity == 2 ? mulib_optml_tree_ (r, last, i->b) : NULL;
      matched = a->constraints ? 0 : mulib_peephole_apply (i, &f, o->rules, o->rule_count, NULL);
      if (matched < 0) { s = MULIB_MIR_INVALID; goto done; }
      if (matched) {
        if (stats->rewrites == SIZE_MAX) { s = MULIB_MIR_OVERFLOW; goto done; }
        ++stats->rewrites; *changed = 1;
      }
      if (mulib_mir_native_defines (i->opcode)) last[i->destination] = k;
      if (a->constraints & (MULIB_LIR_BARRIER | MULIB_LIR_VOLATILE)) {
        if (!mulib_optml_charge_ (&work, n)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
        for (size_t j = 0; j < n; ++j) last[j] = SIZE_MAX;
      }
    }
  }
  if (o->dead_writes) {
    for (size_t k = 0; k < p.count; ++k) {
      const mulib_mir_native_instruction_t *i = &r->body.code[k];
      int arity = mulib_mir_native_arity (i->opcode);
      if (!mulib_optml_charge_ (&work, 1)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
      if (arity > 0) used[i->a] = 1;
      if (arity == 2) used[i->b] = 1;
      if (i->opcode == MULIB_MIR_NATIVE_CALL) {
        if (!mulib_optml_charge_ (&work, i->b)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
        for (size_t j = 0; j < i->b; ++j) used[p.arguments[i->a + j]] = 1;
      }
    }
    for (size_t b = 0; b < p.block_count; ++b) {
      if (!mulib_optml_charge_ (&work, n)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
      if (n) memcpy (live, used, n); /* Conservative live-out: all global reads. */
      for (size_t k = p.blocks[b].first + p.blocks[b].count; k-- > p.blocks[b].first;) {
        mulib_mir_native_instruction_t *i = &r->body.code[k];
        int arity;
        if (!mulib_optml_charge_ (&work, 1)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
        if (mulib_mir_native_defines (i->opcode)) {
          if (!live[i->destination] && !mulib_mir_native_effect (i->opcode) && !r->annotations[k].constraints) {
            *i = mulib_mir_native_instruction (MULIB_MIR_NATIVE_NOP);
            if (stats->dead_writes == SIZE_MAX) { s = MULIB_MIR_OVERFLOW; goto done; }
            ++stats->dead_writes; *changed = 1; continue;
          }
          live[i->destination] = 0;
        }
        arity = mulib_mir_native_arity (i->opcode);
        if (arity > 0) live[i->a] = 1;
        if (arity == 2) live[i->b] = 1;
        if (i->opcode == MULIB_MIR_NATIVE_CALL) {
          if (!mulib_optml_charge_ (&work, i->b)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
          for (size_t j = 0; j < i->b; ++j) live[p.arguments[i->a + j]] = 1;
        }
      }
    }
  }
  r->selected = r->scheduled = 0; s = MULIB_MIR_OK;
done:
  free (last); free (used); free (live); return s;
}
mulib_mir_status_t mulib_optml_run (const mulib_lir_t *input, const mulib_optml_options_t *options,
    mulib_lir_t *out, mulib_optml_stats_t *stats, mulib_mir_diagnostic_t *d)
{
  mulib_optml_options_t defaults = mulib_optml_defaults ();
  const mulib_optml_options_t *o = options ? options : &defaults;
  mulib_lir_t r, view;
  mulib_optml_stats_t st = {0,0,0,0,0};
  mulib_mir_status_t s;
  if (!mulib_lir_empty (out) || !input || input->machine != o->selection.machine
      || (!o->rules && o->rule_count) || (o->dead_writes != 0 && o->dead_writes != 1)
      || (o->superinstructions != 0 && o->superinstructions != 1)) {
    if (d) { d->status = MULIB_MIR_INVALID; d->record = 0; }
    return MULIB_MIR_INVALID;
  }
  if (o->rule_count > o->selection.limit || o->passes > o->selection.limit) {
    if (d) { d->status = MULIB_MIR_RESOURCE_LIMIT; d->record = 0; }
    return MULIB_MIR_RESOURCE_LIMIT;
  }
  if (!mulib_peephole_validate (o->rules, o->rule_count)) {
    if (d) { d->status = MULIB_MIR_INVALID; d->record = 0; }
    return MULIB_MIR_INVALID;
  }
  memset (&r, 0, sizeof (r)); view = *input; view.selected = view.scheduled = 0;
  s = mulib_lir_clone (&view, o->selection.limit, &r, d);
  if (s != MULIB_MIR_OK) goto done;
  /* Facts are refreshed even when the input was edited or came from a custom pass. */
  s = mulib_isel_refresh (&r, &o->selection, d); if (s != MULIB_MIR_OK) goto done;
  for (size_t pass = 0; pass < o->passes; ++pass) {
    int changed = 0;
    s = mulib_optml_pass_ (&r, o, &st, &changed); if (s != MULIB_MIR_OK) goto done;
    ++st.passes;
    s = mulib_isel_refresh (&r, &o->selection, d); if (s != MULIB_MIR_OK) goto done;
    if (!changed) break;
  }
  *out = r; memset (&r, 0, sizeof (r)); if (stats) *stats = st;
done:
  mulib_lir_free (&r); if (d) d->status = s; return s;
}
mulib_mir_status_t mulib_optml_emit (const mulib_lir_t *r, const char *symbol, size_t limit,
    mulib_mir_write_fn writer, void *user, mulib_mir_diagnostic_t *d)
{
  mulib_mir_native_program_t p;
  mulib_mir_status_t s = mulib_lir_validate (r, limit, d);
  if (s != MULIB_MIR_OK) return s;
  if (!r->selected || !r->scheduled) {
    if (d) { d->status = MULIB_MIR_INVALID; d->record = 0; }
    return MULIB_MIR_INVALID;
  }
  p = mulib_mir_native_view (&r->body);
  return mulib_machine_emit (r->machine, &p, r->selection, r->order, symbol, limit, writer, user, d);
}
typedef struct mulib_optml_text_ { mulib_strbuf_t buffer; size_t limit; mulib_mir_status_t status; }
  mulib_optml_text_;
static int mulib_optml_writer_ (void *user, const char *text, size_t length)
{
  mulib_optml_text_ *t = (mulib_optml_text_ *)user;
  if (t->status != MULIB_MIR_OK) return 0;
  if (t->buffer.length > t->limit || length > t->limit - t->buffer.length
      || length == SIZE_MAX - t->buffer.length) t->status = MULIB_MIR_RESOURCE_LIMIT;
  else if (!mulib_strbuf_append_n (&t->buffer, text, length)) t->status = MULIB_MIR_NO_MEMORY;
  return t->status == MULIB_MIR_OK;
}
void mulib_optml_free (mulib_optml_result_t *r)
{ if (r) { mulib_lir_free (&r->lir); free (r->assembly); memset (r, 0, sizeof (*r)); } }
mulib_mir_status_t mulib_optml_compile (const mulib_mir_native_program_t *p, const char *symbol,
    const mulib_optml_options_t *options, mulib_optml_result_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_optml_options_t defaults = mulib_optml_defaults ();
  const mulib_optml_options_t *o = options ? options : &defaults;
  mulib_optml_result_t r;
  mulib_lir_t selected;
  mulib_optml_text_ text;
  mulib_mir_status_t s;
  if (!out || !mulib_lir_empty (&out->lir) || out->assembly || out->length
      || out->stats.passes || out->stats.rewrites || out->stats.dead_writes
      || out->stats.macros || out->stats.macro_uses || !mulib_mir_native_name (symbol)) {
    if (d) { d->status = MULIB_MIR_INVALID; d->record = 0; }
    return MULIB_MIR_INVALID;
  }
  memset (&r, 0, sizeof (r)); memset (&selected, 0, sizeof (selected)); memset (&text, 0, sizeof (text));
  text.limit = o->text_limit;
  s = mulib_isel_run (p, &o->selection, &selected, d); if (s != MULIB_MIR_OK) goto done;
  s = mulib_optml_run (&selected, o, &r.lir, &r.stats, d); if (s != MULIB_MIR_OK) goto done;
  s = mulib_optml_emit (&r.lir, symbol, o->selection.limit, mulib_optml_writer_, &text, d);
  if (text.status != MULIB_MIR_OK) s = text.status;
  if (s != MULIB_MIR_OK) goto done;
  if (o->superinstructions) {
    mulib_superinstr_result_t packed = {NULL,0,0,0};
    mulib_superinstr_options_t so = o->super;
    if (so.text_limit > o->text_limit) so.text_limit = o->text_limit;
    s = mulib_superinstr_pack (text.buffer.data, text.buffer.length, symbol, &so, &packed);
    if (s != MULIB_MIR_OK) goto done;
    r.assembly = packed.text; r.length = packed.length;
    r.stats.macros = packed.macros; r.stats.macro_uses = packed.uses;
  } else {
    r.assembly = text.buffer.data; r.length = text.buffer.length; memset (&text.buffer, 0, sizeof (text.buffer));
  }
  *out = r; memset (&r, 0, sizeof (r));
done:
  mulib_strbuf_free (&text.buffer); mulib_lir_free (&selected); mulib_optml_free (&r);
  if (d) d->status = s;
  return s;
}
mulib_mir_status_t mulib_optml_write (const mulib_optml_result_t *r, mulib_mir_write_fn writer, void *user)
{
  if (!r || !r->assembly || !writer) return MULIB_MIR_INVALID;
  return writer (user, r->assembly, r->length) ? MULIB_MIR_OK : MULIB_MIR_IO_ERROR;
}
#ifdef __cplusplus
}
#endif
#endif
