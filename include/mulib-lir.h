/** @file mulib-lir.h
 * @brief Owned terse register LIR with selection, semantic and scheduling annotations.
 * Define MULIB_IMPL_LIR, MULIB_IMPL_MIR, MULIB_IMPL_MACHINE and
 * MULIB_IMPL_MUBURG, MULIB_IMPL_LOWOPT. See manual/mulib-lir.md for ownership and invalidation.
 */
#ifndef MULIB_LIR_H
#define MULIB_LIR_H
#include "mulib-machine.h"
#include "mulib-lowopt.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
  MULIB_LIR_KEEP = 1, MULIB_LIR_BARRIER = 2, MULIB_LIR_VOLATILE = 4
};
/** User constraints survive rewrites. Other fields are derived facts, refreshed
 * by selection/scheduling. A schedule name is an ArchML schedule record name;
 * an empty name requests the opcode's default class. cycles are block-relative. */
typedef struct mulib_lir_annotation {
  unsigned constraints, known_mask;
  uint64_t values[2];
  uint64_t resources; /* Shared unit-capacity resources used during issue. */
  size_t source, latency, throughput, cycle;
  char schedule[64];
  int barrier; /* Derived ArchML scheduling barrier. */
} mulib_lir_annotation_t;
/** Native MIR's terse operations are LIR's semantic core. Selection describes
 * the target realization (including literal operands); order is a block-local
 * permutation. All arrays/strings are owned; machine is borrowed and immutable.
 * Initialize to zero, never shallow-copy a live object. Edits invalidate selected
 * and scheduled; use isel_refresh before emission. No hidden implementation macros. */
typedef struct mulib_lir {
  mulib_mir_native_buffer_t body;
  mulib_machine_selection_t *selection;
  mulib_lir_annotation_t *annotations;
  size_t *order, *block_cycles;
  const mulib_machine_t *machine;
  uint64_t cost;
  int selected, scheduled;
} mulib_lir_t;
/** Check the zero-owner precondition (also accepts NULL, returning false). */
int mulib_lir_empty (const mulib_lir_t *);
/** Deep-copy a validated native program into an empty LIR, identity order. */
mulib_mir_status_t mulib_lir_from_mir (const mulib_mir_native_program_t *,
    const mulib_machine_t *, size_t limit, mulib_lir_t *, mulib_mir_diagnostic_t *);
/** Deep-copy all annotations and selection state, borrowing the same target. */
mulib_mir_status_t mulib_lir_clone (const mulib_lir_t *, size_t limit,
    mulib_lir_t *, mulib_mir_diagnostic_t *);
/** Validate semantic records, constant facts, selected operands/costs and, when
 * scheduled, dependency order, resource occupancy and cycle annotations.
 * Unselected/unscheduled objects may contain stale derived facts/order. */
mulib_mir_status_t mulib_lir_validate (const mulib_lir_t *, size_t limit,
    mulib_mir_diagnostic_t *);
/** Test data-register reads, including CALL's argument slice. */
int mulib_lir_reads (const mulib_mir_native_program_t *, size_t record, size_t reg);
/** RAW/WAR/WAW, ordered memory effects, user barriers and terminator edges.
 * before/after are increasing records in the same validated basic block. */
int mulib_lir_dependency (const mulib_lir_t *, size_t before, size_t after);
/** Stable opcode name, or NULL for an invalid opcode. */
const char *mulib_lir_opcode_name (int opcode);
/** Emit a deterministic diagnostic LIR listing with annotations. Partial output
 * is possible on callback failure. This listing is not target assembly. */
mulib_mir_status_t mulib_lir_write (const mulib_lir_t *, size_t limit,
    mulib_mir_write_fn, void *user);
/** Release owned storage and zero the owner; NULL is harmless. */
void mulib_lir_free (mulib_lir_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LIR) && !defined(MULIB_LIR_IMPLEMENTED)
#define MULIB_LIR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_lir_empty (const mulib_lir_t *r)
{
  return r && !r->body.code && !r->body.blocks && !r->body.arguments && !r->body.symbols
      && !r->body.count && !r->body.block_count && !r->body.register_count
      && !r->body.parameter_count && !r->body.argument_count && !r->body.symbol_count
      && !r->selection && !r->annotations && !r->order && !r->block_cycles
      && !r->machine && !r->cost && !r->selected && !r->scheduled;
}
void mulib_lir_free (mulib_lir_t *r)
{
  if (!r) return;
  mulib_mir_native_free (&r->body); free (r->selection); free (r->annotations);
  free (r->order); free (r->block_cycles); memset (r, 0, sizeof (*r));
}
const char *mulib_lir_opcode_name (int op)
{
  static const char *const names[] = {"nop","const","param","copy","add","sub",
    "mul","and","or","xor","shl","shr","sar","eq","slt","ult","load",
    "store","call","jump","branch","return"};
  return op >= 0 && op < MULIB_MIR_NATIVE_OPCODE_COUNT ? names[op] : NULL;
}
mulib_mir_status_t mulib_lir_from_mir (const mulib_mir_native_program_t *p,
    const mulib_machine_t *m, size_t limit, mulib_lir_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_lir_t r;
  mulib_mir_status_t s = MULIB_MIR_INVALID;
  memset (&r, 0, sizeof (r));
  if (!mulib_lir_empty (out)) goto done;
  s = mulib_machine_validate (m, limit); if (s != MULIB_MIR_OK) goto done;
  s = mulib_mir_native_clone (p, limit, &r.body, d); if (s != MULIB_MIR_OK) return s;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (p->count > SIZE_MAX / sizeof (*r.selection)
      || p->count > SIZE_MAX / sizeof (*r.annotations)
      || p->count > SIZE_MAX / sizeof (*r.order)
      || p->block_count > SIZE_MAX / sizeof (*r.block_cycles)) goto done;
  r.selection = (mulib_machine_selection_t *)calloc (p->count, sizeof (*r.selection));
  r.annotations = (mulib_lir_annotation_t *)calloc (p->count, sizeof (*r.annotations));
  r.order = (size_t *)malloc (p->count * sizeof (*r.order));
  r.block_cycles = (size_t *)calloc (p->block_count, sizeof (*r.block_cycles));
  s = MULIB_MIR_NO_MEMORY;
  if (!r.selection || !r.annotations || !r.order || !r.block_cycles) goto done;
  for (size_t k = 0; k < p->count; ++k) {
    r.order[k] = k; r.annotations[k].source = k;
    r.annotations[k].latency = r.annotations[k].throughput = 1;
  }
  r.machine = m; *out = r; memset (&r, 0, sizeof (r)); s = MULIB_MIR_OK;
done:
  mulib_lir_free (&r); if (d) { d->status = s; d->record = 0; } return s;
}
int mulib_lir_reads (const mulib_mir_native_program_t *p, size_t record, size_t reg)
{
  const mulib_mir_native_instruction_t *i;
  int arity;
  if (!p || record >= p->count || !p->code) return 0;
  i = &p->code[record]; arity = mulib_mir_native_arity (i->opcode);
  if ((arity > 0 && i->a == reg) || (arity == 2 && i->b == reg)) return 1;
  if (i->opcode == MULIB_MIR_NATIVE_CALL && p->arguments
      && i->a <= p->argument_count && i->b <= p->argument_count - i->a)
    for (size_t k = 0; k < i->b; ++k) if (p->arguments[i->a + k] == reg) return 1;
  return 0;
}
int mulib_lir_dependency (const mulib_lir_t *r, size_t a, size_t b)
{
  mulib_mir_native_program_t p = mulib_mir_native_view (&r->body);
  const mulib_mir_native_instruction_t *x = &p.code[a], *y = &p.code[b];
  int xd = mulib_mir_native_defines (x->opcode), yd = mulib_mir_native_defines (y->opcode);
  return y->opcode >= MULIB_MIR_NATIVE_JUMP
      || r->annotations[a].barrier || r->annotations[b].barrier
      || ((r->annotations[a].constraints | r->annotations[b].constraints)
          & (MULIB_LIR_BARRIER | MULIB_LIR_VOLATILE))
      || x->opcode == MULIB_MIR_NATIVE_CALL || y->opcode == MULIB_MIR_NATIVE_CALL
      || (mulib_mir_native_effect (x->opcode) && mulib_mir_native_effect (y->opcode))
      || (xd && mulib_lir_reads (&p, b, x->destination))
      || (yd && mulib_lir_reads (&p, a, y->destination))
      || (xd && yd && x->destination == y->destination);
}
typedef struct mulib_lir_state_ {
  size_t write_pos, read_pos, write_end, read_end, block;
  uint64_t value;
  int known;
} mulib_lir_state_;
static mulib_lir_state_ *mulib_lir_value_ (mulib_lir_state_ *states, size_t reg, size_t block)
{
  mulib_lir_state_ *s = &states[reg];
  if (s->block != block + 1) { memset (s, 0, sizeof (*s)); s->block = block + 1; }
  return s;
}
static int mulib_lir_before_ (size_t position, size_t finish, size_t pos, size_t cycle)
{ return (!position || position < pos) && finish <= cycle; }
static int mulib_lir_schedule_name_ (const char *s, size_t capacity)
{
  for (size_t k = 0; k < capacity; ++k) {
    unsigned char c = (unsigned char)s[k];
    if (!c) return 1;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '_' || c == '-' || c == '.')) return 0;
  }
  return 0;
}
mulib_mir_status_t mulib_lir_validate (const mulib_lir_t *r, size_t limit,
                                     mulib_mir_diagnostic_t *d)
{
  mulib_mir_native_program_t p;
  mulib_mir_status_t s = MULIB_MIR_INVALID;
  size_t *positions = NULL;
  mulib_lir_state_ *state = NULL;
  uint64_t leaf_cost[3][2] = {{0}};
  unsigned char leaf_found[3][2] = {{0}};
  uint64_t total = 0;
  size_t k = 0;
  if (!r || !r->selection || !r->annotations || !r->order || !r->block_cycles
      || (r->selected != 0 && r->selected != 1) || (r->scheduled != 0 && r->scheduled != 1)
      || (r->scheduled && !r->selected)) goto done;
  p = mulib_mir_native_view (&r->body);
  s = mulib_mir_native_validate (&p, limit, d); if (s != MULIB_MIR_OK) return s;
  s = mulib_machine_validate (r->machine, limit); if (s != MULIB_MIR_OK) goto done;
  if (r->selected) for (k = 0; k < r->machine->rule_count; ++k) {
    const mulib_muburg_rule_t *rule = &r->machine->rules[k];
    if (rule->opcode >= MULIB_MACHINE_REGISTER && rule->opcode <= MULIB_MACHINE_WIDE) {
      size_t terminal = (size_t)(rule->opcode - MULIB_MACHINE_REGISTER), goal = rule->lhs;
      if (!leaf_found[terminal][goal] || rule->cost < leaf_cost[terminal][goal])
        leaf_cost[terminal][goal] = rule->cost;
      leaf_found[terminal][goal] = 1;
    }
  }
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (p.count > SIZE_MAX / sizeof (*positions) || p.register_count > SIZE_MAX / sizeof (*state)) goto done;
  positions = (size_t *)calloc (p.count, sizeof (*positions));
  if (p.register_count) state = (mulib_lir_state_ *)calloc (p.register_count, sizeof (*state));
  s = MULIB_MIR_NO_MEMORY; if (!positions || (p.register_count && !state)) goto done;
  s = MULIB_MIR_INVALID;
  for (size_t b = 0; b < p.block_count; ++b) {
    size_t first = p.blocks[b].first, end = first + p.blocks[b].count;
    size_t ready[64] = {0}, makespan = 0, previous = 0;
    size_t all_pos = 0, all_end = 0, barrier_pos = 0, barrier_end = 0, memory_pos = 0, memory_end = 0;
    for (k = first; k < end; ++k) {
      size_t n = r->scheduled ? r->order[k] : k;
      if (n < first || n >= end || positions[n] || ((n == end - 1) != (k == end - 1))) goto done;
      positions[n] = k + 1;
      if (r->scheduled) {
        const mulib_lir_annotation_t *a = &r->annotations[n];
        size_t finish;
        if (!a->latency || !a->throughput || a->latency > SIZE_MAX - a->cycle
            || (k != first && a->cycle <= previous)
            || (a->resources && a->throughput > SIZE_MAX - a->cycle)) goto done;
        finish = a->cycle + a->latency; previous = a->cycle;
        if (finish > makespan) makespan = finish;
        for (size_t q = 0; q < 64; ++q) if (a->resources & (UINT64_C(1) << q)) {
          if (a->cycle < ready[q]) goto done;
          ready[q] = a->cycle + a->throughput;
        }
      }
    }
    if (r->scheduled && r->block_cycles[b] != makespan) goto done;
    for (k = p.blocks[b].first; k < end; ++k) {
      const mulib_mir_native_instruction_t *i = &p.code[k];
      const mulib_lir_annotation_t *a = &r->annotations[k];
      size_t pos = positions[k], finish = r->scheduled ? a->cycle + a->latency : 0;
      uint64_t value = 0;
      int arity = mulib_mir_native_arity (i->opcode), constant = 0;
      if ((a->constraints & ~(unsigned)(MULIB_LIR_KEEP | MULIB_LIR_BARRIER | MULIB_LIR_VOLATILE))
          || !mulib_lir_schedule_name_ (a->schedule, sizeof (a->schedule))) goto done;
      if (r->selected) {
        const mulib_machine_selection_t *q = &r->selection[k];
        const mulib_muburg_rule_t *rule;
        unsigned mask = 0, immediate = 0;
        uint64_t cost;
        if (q->rule >= r->machine->rule_count
            || r->machine->rules[q->rule].opcode != (int)i->opcode
            || q->instruction != r->machine->rules[q->rule].instruction
            || q->latency != r->machine->latencies[q->rule]
            || !a->latency || !a->throughput || (a->barrier != 0 && a->barrier != 1)) goto done;
        rule = &r->machine->rules[q->rule]; cost = rule->cost;
        for (int j = 0; j < 2; ++j) {
          int terminal = MULIB_MACHINE_REGISTER;
          uint64_t expected = 0;
          if (j < arity) {
            size_t reg = j ? i->b : i->a;
            mulib_lir_state_ *v = mulib_lir_value_ (state, reg, b);
            if (v->known) {
              uint64_t bound = UINT64_C(1) << (r->machine->signed_immediate_bits - 1);
              expected = v->value; mask |= 1u << j;
              terminal = expected < bound || expected >= UINT64_MAX - bound + 1
                  ? MULIB_MACHINE_IMMEDIATE : MULIB_MACHINE_WIDE;
            }
            if (rule->child_goal[j] == MULIB_MACHINE_IMM_GOAL) {
              if (!(mask & (1u << j))) goto done;
              immediate |= 1u << j;
            }
            terminal -= MULIB_MACHINE_REGISTER;
            if (!leaf_found[terminal][rule->child_goal[j]]
                || leaf_cost[terminal][rule->child_goal[j]] > UINT64_MAX - cost) goto done;
            cost += leaf_cost[terminal][rule->child_goal[j]];
          }
          if (a->values[j] != expected || q->operands[j] != ((immediate & (1u << j)) ? expected : 0)) goto done;
        }
        if (a->known_mask != mask || q->immediate_mask != immediate || q->cost != cost
            || cost > UINT64_MAX - total) goto done;
        total += cost;
        if (i->opcode == MULIB_MIR_NATIVE_CONST) { value = i->immediate; constant = 1; }
        else if (i->opcode == MULIB_MIR_NATIVE_COPY && (mask & 1)) { value = a->values[0]; constant = 1; }
        else if (mask == 3) constant = mulib_lowopt_evaluate (i->opcode, a->values[0], a->values[1], &value);
      }
      if (r->scheduled) {
        int barrier = a->barrier || (a->constraints & (MULIB_LIR_BARRIER | MULIB_LIR_VOLATILE))
            || i->opcode == MULIB_MIR_NATIVE_CALL;
        if (!mulib_lir_before_ (barrier_pos, barrier_end, pos, a->cycle)) goto done;
        if ((barrier || i->opcode >= MULIB_MIR_NATIVE_JUMP)
            && !mulib_lir_before_ (all_pos, all_end, pos, a->cycle)) goto done;
        if (barrier) { barrier_pos = pos; barrier_end = finish; }
        if (mulib_mir_native_effect (i->opcode)) {
          if (!mulib_lir_before_ (memory_pos, memory_end, pos, a->cycle)) goto done;
          memory_pos = pos; memory_end = finish;
        }
        if (mulib_mir_native_defines (i->opcode)) {
          mulib_lir_state_ *v = mulib_lir_value_ (state, i->destination, b);
          if (!mulib_lir_before_ (v->write_pos, v->write_end, pos, a->cycle)
              || !mulib_lir_before_ (v->read_pos, v->read_end, pos, a->cycle)) goto done;
        }
        for (size_t j = 0; j < (i->opcode == MULIB_MIR_NATIVE_CALL ? i->b : (size_t)arity); ++j) {
          size_t reg = i->opcode == MULIB_MIR_NATIVE_CALL ? p.arguments[i->a + j] : j ? i->b : i->a;
          mulib_lir_state_ *v = mulib_lir_value_ (state, reg, b);
          if (!mulib_lir_before_ (v->write_pos, v->write_end, pos, a->cycle)) goto done;
          if (pos > v->read_pos) v->read_pos = pos;
          if (finish > v->read_end) v->read_end = finish;
        }
        if (pos > all_pos) all_pos = pos;
        if (finish > all_end) all_end = finish;
      }
      if (mulib_mir_native_defines (i->opcode)) {
        mulib_lir_state_ *v = mulib_lir_value_ (state, i->destination, b);
        v->known = constant; v->value = value;
        v->write_pos = pos; v->write_end = finish; v->read_pos = v->read_end = 0;
      }
    }
  }
  if (r->selected && r->cost != total) goto done;
  s = MULIB_MIR_OK;
done:
  free (positions); free (state); if (d) { d->status = s; d->record = k; } return s;
}
mulib_mir_status_t mulib_lir_clone (const mulib_lir_t *p, size_t limit,
                                  mulib_lir_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_lir_t r;
  mulib_mir_native_program_t v;
  mulib_mir_status_t s;
  if (!mulib_lir_empty (out)) {
    if (d) { d->status = MULIB_MIR_INVALID; d->record = 0; }
    return MULIB_MIR_INVALID;
  }
  s = mulib_lir_validate (p, limit, d); if (s != MULIB_MIR_OK) return s;
  memset (&r, 0, sizeof (r)); v = mulib_mir_native_view (&p->body);
  s = mulib_lir_from_mir (&v, p->machine, limit, &r, d); if (s != MULIB_MIR_OK) return s;
  memcpy (r.annotations, p->annotations, v.count * sizeof (*r.annotations));
  memcpy (r.selection, p->selection, v.count * sizeof (*r.selection));
  memcpy (r.order, p->order, v.count * sizeof (*r.order));
  memcpy (r.block_cycles, p->block_cycles, v.block_count * sizeof (*r.block_cycles));
  r.cost = p->cost; r.selected = p->selected; r.scheduled = p->scheduled; *out = r;
  return MULIB_MIR_OK;
}
mulib_mir_status_t mulib_lir_write (const mulib_lir_t *r, size_t limit,
                                  mulib_mir_write_fn writer, void *user)
{
  mulib_mir_status_t s = mulib_lir_validate (r, limit, NULL);
  char text[512];
  if (s != MULIB_MIR_OK) return s;
  if (!writer) return MULIB_MIR_INVALID;
  for (size_t b = 0; b < r->body.block_count; ++b) {
    int n = snprintf (text, sizeof (text), "block %zu @cycles=%zu\n", b, r->block_cycles[b]);
    if (n < 0 || (size_t)n >= sizeof (text) || !writer (user, text, (size_t)n)) return MULIB_MIR_IO_ERROR;
    for (size_t j = r->body.blocks[b].first; j < r->body.blocks[b].first + r->body.blocks[b].count; ++j) {
      size_t k = r->scheduled ? r->order[j] : j;
      const mulib_mir_native_instruction_t *i = &r->body.code[k];
      const mulib_lir_annotation_t *a = &r->annotations[k];
      n = snprintf (text, sizeof (text),
          "  %zu: %s %zu %zu %zu %zu 0x%016" PRIx64
          " @source=%zu @constraints=%u @known=%u @cost=%" PRIu64
          " @latency=%zu @throughput=%zu @cycle=%zu @schedule=%s\n",
          k, mulib_lir_opcode_name (i->opcode), i->destination, i->a, i->b, i->target,
          i->immediate, a->source, a->constraints, a->known_mask, r->selection[k].cost,
          a->latency, a->throughput, a->cycle, a->schedule);
      if (n < 0 || (size_t)n >= sizeof (text) || !writer (user, text, (size_t)n)) return MULIB_MIR_IO_ERROR;
    }
  }
  return MULIB_MIR_OK;
}
#ifdef __cplusplus
}
#endif
#endif
