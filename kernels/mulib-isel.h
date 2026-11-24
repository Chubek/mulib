/** @file mulib-isel.h
 * @brief Demand-driven memoized tree covering and native MIR to annotated LIR.
 * Define MULIB_IMPL_ISEL and the dependency switches listed in the manual.
 * BURS-style nonterminal goals and additive costs use sparse top-down memoization,
 * not MuBURG's eager bottom-up tables. No implementation macros are implicit.
 */
#ifndef MULIB_ISEL_H
#define MULIB_ISEL_H
#include "mulib-lir.h"
#include "mulib-isched.h"
#include "mulib-archml.h"
#include "mulib-lowopt.h"
#ifdef __cplusplus
extern "C" {
#endif
/** A demanded (node, nonterminal) state. rule=NONE denotes no finite cover. */
typedef struct mulib_isel_memo {
  size_t node, goal, rule;
  uint64_t cost;
  unsigned state;
} mulib_isel_memo_t;
typedef struct mulib_isel_cover {
  mulib_isel_memo_t *memo;
  size_t count, root;
  uint64_t cost;
} mulib_isel_cover_t;
typedef struct mulib_isel_limits {
  size_t nodes, rules, goals, cells, depth, work;
} mulib_isel_limits_t;
/** Minimum-cost tree cover using the MuBURG grammar representation. Goals are
 * demanded recursively and memoized; ties use rule order. Shared DAG children
 * are costed per tree occurrence. Only reachable demanded states are evaluated.
 * Entire node/rule arrays are structurally checked. Depth is capped at 128.
 * Cyclic demanded states, work exhaustion and overflowing covers are diagnosed.
 * Empty output required; failure leaves it unchanged. */
mulib_mir_status_t mulib_isel_cover (const mulib_muburg_node_t *, size_t nodes,
    const mulib_muburg_rule_t *, size_t rules, size_t goals, size_t root,
    size_t goal, mulib_isel_limits_t, mulib_isel_cover_t *, mulib_mir_diagnostic_t *);
/** Release the memoization cache and zero the cover. */
void mulib_isel_cover_free (mulib_isel_cover_t *);
typedef struct mulib_isel_options {
  const mulib_machine_t *machine;
  const mulib_archml_t *arch;
  size_t limit, edge_limit, cycle_limit, work_limit;
  int schedule;
} mulib_isel_options_t;
/** Defaults: AMD64, no external tuning, 65536 records, 262144 edges,
 * 1048576 cycles and 16777216 work units. Limits are literal, including zero. */
mulib_isel_options_t mulib_isel_defaults (void);
/** Select native wrapping-word MIR, annotate proven operand constants and
 * schedule each block. Built-in emitters select one operation plus operand
 * leaves; the general cover API also accepts arbitrary expression trees.
 * arch optionally supplies schedule classes/resources from library/archml.
 * output owns the program and all facts; only machine remains borrowed. */
mulib_mir_status_t mulib_isel_run (const mulib_mir_native_program_t *,
    const mulib_isel_options_t *, mulib_lir_t *, mulib_mir_diagnostic_t *);
/** Transactionally reselect/reschedule edited LIR. User constraints, explicit
 * schedule class overrides and source IDs survive; all derived facts refresh.
 * options.machine must match the LIR's borrowed target. */
mulib_mir_status_t mulib_isel_refresh (mulib_lir_t *, const mulib_isel_options_t *,
    mulib_mir_diagnostic_t *);
/** Transactionally annotate/schedule already selected LIR using ArchML classes,
 * latency, throughput, shared resources and barriers. RAW/WAR/WAW, all memory
 * pairs and CALL arguments constrain motion. Target lowering expands each LIR
 * operation into a bundle: cycle annotations model bundles, not silicon timing. */
mulib_mir_status_t mulib_isel_schedule (mulib_lir_t *, const mulib_isel_options_t *,
    mulib_mir_diagnostic_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ISEL) && !defined(MULIB_ISEL_IMPLEMENTED)
#define MULIB_ISEL_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_isel_search_ {
  const mulib_muburg_node_t *nodes;
  const mulib_muburg_rule_t *rules;
  size_t rule_count, capacity, maximum, work, node;
  mulib_isel_limits_t limits;
  mulib_isel_cover_t result;
  mulib_mir_status_t status;
} mulib_isel_search_;
static int mulib_isel_charge_ (size_t *remaining, size_t amount)
{
  if (amount > *remaining) return 0;
  *remaining -= amount; return 1;
}
static size_t mulib_isel_demand_ (mulib_isel_search_ *s, size_t node, size_t goal, size_t depth)
{
  size_t cell;
  int overflow = 0;
  if (s->status != MULIB_MIR_OK) return SIZE_MAX;
  s->node = node;
  if (depth >= s->limits.depth || depth >= 128 || !mulib_isel_charge_ (&s->work, 1)) {
    s->status = MULIB_MIR_RESOURCE_LIMIT; return SIZE_MAX;
  }
  for (cell = 0; cell < s->result.count; ++cell) {
    if (!mulib_isel_charge_ (&s->work, 1)) { s->status = MULIB_MIR_RESOURCE_LIMIT; return SIZE_MAX; }
    if (s->result.memo[cell].node == node && s->result.memo[cell].goal == goal) {
      if (s->result.memo[cell].state == 1) s->status = MULIB_MIR_BAD_REFERENCE;
      return cell;
    }
  }
  if (cell == s->maximum) {
    s->status = MULIB_MIR_RESOURCE_LIMIT; return SIZE_MAX;
  }
  if (cell == s->capacity) {
    size_t capacity = s->capacity ? s->capacity : 8;
    mulib_isel_memo_t *memo;
    if (s->capacity) capacity = s->capacity > s->maximum / 2 ? s->maximum : s->capacity * 2;
    if (capacity > s->maximum) capacity = s->maximum;
    memo = (mulib_isel_memo_t *)realloc (s->result.memo, capacity * sizeof (*memo));
    if (!memo) { s->status = MULIB_MIR_NO_MEMORY; return SIZE_MAX; }
    s->result.memo = memo; s->capacity = capacity;
  }
  ++s->result.count;
  memset (&s->result.memo[cell], 0, sizeof (s->result.memo[cell]));
  s->result.memo[cell].node = node; s->result.memo[cell].goal = goal;
  s->result.memo[cell].rule = SIZE_MAX; s->result.memo[cell].state = 1;
  for (size_t j = 0; j < s->rule_count; ++j) {
    const mulib_muburg_rule_t *r = &s->rules[j];
    uint64_t cost = r->cost;
    int valid = 1, exceeds = 0;
    s->node = node;
    if (!mulib_isel_charge_ (&s->work, 1)) { s->status = MULIB_MIR_RESOURCE_LIMIT; return cell; }
    if (r->lhs != goal || r->opcode != s->nodes[node].opcode || r->arity != s->nodes[node].arity) continue;
    for (unsigned a = 0; a < r->arity; ++a) {
      size_t child = mulib_isel_demand_ (s, s->nodes[node].child[a], r->child_goal[a], depth + 1);
      if (s->status != MULIB_MIR_OK) return cell;
      if (s->result.memo[child].rule == SIZE_MAX) {
        if (s->result.memo[child].state == 3) exceeds = 1;
        else valid = 0;
      } else if (!exceeds) {
        if (s->result.memo[child].cost > UINT64_MAX - cost) exceeds = 1;
        else cost += s->result.memo[child].cost;
      }
    }
    if (valid && exceeds) overflow = 1;
    if (valid && !exceeds && (s->result.memo[cell].rule == SIZE_MAX || cost < s->result.memo[cell].cost)) {
      s->result.memo[cell].rule = j; s->result.memo[cell].cost = cost;
    }
  }
  s->result.memo[cell].state = overflow && s->result.memo[cell].rule == SIZE_MAX ? 3 : 2;
  return cell;
}
void mulib_isel_cover_free (mulib_isel_cover_t *p)
{ if (p) { free (p->memo); memset (p, 0, sizeof (*p)); } }
static mulib_mir_status_t mulib_isel_cover_ (const mulib_muburg_node_t *nodes, size_t n,
    const mulib_muburg_rule_t *rules, size_t nr, size_t ng, size_t root, size_t goal,
    mulib_isel_limits_t limits, mulib_isel_cover_t *out, mulib_mir_diagnostic_t *d, size_t *remaining)
{
  mulib_isel_search_ s;
  size_t k = 0, cell;
  memset (&s, 0, sizeof (s)); s.status = MULIB_MIR_INVALID; s.work = limits.work;
  if (!nodes || !n || (!rules && nr) || !ng || root >= n || goal >= ng
      || !out || out->memo || out->count || out->root || out->cost) goto done;
  s.status = MULIB_MIR_RESOURCE_LIMIT;
  if (n > limits.nodes || nr > limits.rules || ng > limits.goals
      || n > SIZE_MAX / sizeof (*nodes)
      || nr > SIZE_MAX / sizeof (*rules)) goto done;
  s.maximum = n > SIZE_MAX / ng || n * ng > limits.cells ? limits.cells : n * ng;
  if (s.maximum > SIZE_MAX / sizeof (*s.result.memo)) s.maximum = SIZE_MAX / sizeof (*s.result.memo);
  s.status = MULIB_MIR_INVALID;
  for (k = 0; k < n; ++k) {
    if (!mulib_isel_charge_ (&s.work, 1)) { s.status = MULIB_MIR_RESOURCE_LIMIT; goto done; }
    if (nodes[k].arity > 2) goto done;
    for (unsigned a = 0; a < nodes[k].arity; ++a) if (nodes[k].child[a] >= n) goto done;
  }
  for (k = 0; k < nr; ++k) {
    if (!mulib_isel_charge_ (&s.work, 1)) { s.status = MULIB_MIR_RESOURCE_LIMIT; goto done; }
    if (rules[k].arity > 2 || rules[k].lhs >= ng) goto done;
    for (unsigned a = 0; a < rules[k].arity; ++a) if (rules[k].child_goal[a] >= ng) goto done;
  }
  s.nodes = nodes; s.rules = rules; s.rule_count = nr; s.limits = limits; s.status = MULIB_MIR_OK;
  cell = mulib_isel_demand_ (&s, root, goal, 0); k = root;
  if (s.status != MULIB_MIR_OK) { k = s.node; goto done; }
  if (s.result.memo[cell].rule == SIZE_MAX) {
    s.status = s.result.memo[cell].state == 3 ? MULIB_MIR_OVERFLOW : MULIB_MIR_UNSUPPORTED; goto done;
  }
  s.result.root = cell; s.result.cost = s.result.memo[cell].cost;
  *out = s.result; memset (&s.result, 0, sizeof (s.result));
done:
  if (remaining) *remaining = s.work;
  mulib_isel_cover_free (&s.result); if (d) { d->status = s.status; d->record = k; } return s.status;
}
mulib_mir_status_t mulib_isel_cover (const mulib_muburg_node_t *nodes, size_t n,
    const mulib_muburg_rule_t *rules, size_t nr, size_t ng, size_t root, size_t goal,
    mulib_isel_limits_t limits, mulib_isel_cover_t *out, mulib_mir_diagnostic_t *d)
{ return mulib_isel_cover_ (nodes, n, rules, nr, ng, root, goal, limits, out, d, NULL); }
mulib_isel_options_t mulib_isel_defaults (void)
{
  mulib_isel_options_t o;
  o.machine = mulib_machine_x86_64_sysv (); o.arch = NULL;
  o.limit = 65536; o.edge_limit = 262144; o.cycle_limit = 1048576;
  o.work_limit = 16777216; o.schedule = 1; return o;
}
static const char *mulib_isel_class_ (int op)
{
  if (op == MULIB_MIR_NATIVE_MUL) return "mul";
  if (op == MULIB_MIR_NATIVE_LOAD) return "load";
  if (op == MULIB_MIR_NATIVE_STORE) return "store";
  if (op == MULIB_MIR_NATIVE_CALL) return "serial";
  if (op >= MULIB_MIR_NATIVE_JUMP) return "branch";
  return "alu";
}
static mulib_mir_status_t mulib_isel_tuning_ (mulib_lir_t *r, const mulib_isel_options_t *o,
                                            uint64_t *resources, size_t *work, mulib_mir_diagnostic_t *d)
{
  size_t nr = o->arch ? mulib_archml_count (o->arch, "resource") : 0;
  if (nr > 64) return MULIB_MIR_RESOURCE_LIMIT;
  if (o->arch) {
    const char *name = mulib_archml_name (o->arch);
    if ((r->machine == mulib_machine_x86_64_sysv () && strcmp (name, "amd64"))
        || (r->machine == mulib_machine_rv64im () && strcmp (name, "riscv64"))) return MULIB_MIR_UNSUPPORTED;
  }
  for (size_t k = 0; k < r->body.count; ++k) {
    mulib_lir_annotation_t *a = &r->annotations[k];
    a->latency = r->selection[k].latency; a->throughput = 1; a->barrier = 0;
    if (d) d->record = k;
    if (!mulib_isel_charge_ (work, 1)) return MULIB_MIR_RESOURCE_LIMIT;
    resources[k] = a->resources = 0;
    if (o->arch) {
      const char *name = *a->schedule ? a->schedule : mulib_isel_class_ (r->body.code[k].opcode);
      const mulib_archml_node_t *n = mulib_archml_find (o->arch, "schedule", name), *v;
      uint64_t latency, throughput;
      if (!n || !mulib_archml_u64 (mulib_archml_child (mulib_archml_property (n, "latency"), 1), &latency)
          || !mulib_archml_u64 (mulib_archml_child (mulib_archml_property (n, "throughput-cycles"), 1), &throughput)
          || !latency || !throughput || latency > SIZE_MAX || throughput > SIZE_MAX) return MULIB_MIR_UNSUPPORTED;
      a->latency = (size_t)latency; a->throughput = (size_t)throughput;
      v = mulib_archml_child (mulib_archml_property (n, "barrier"), 1);
      a->barrier = v && v->text && !strcmp (v->text, "true");
      for (v = mulib_archml_child (mulib_archml_property (n, "uses"), 1); v; v = v->next) {
        const mulib_archml_node_t *ref = mulib_archml_child (v, 2);
        int found = 0;
        for (size_t j = 0; j < nr; ++j) {
          if (!mulib_isel_charge_ (work, 1)) return MULIB_MIR_RESOURCE_LIMIT;
          const mulib_archml_node_t *res = mulib_archml_child (mulib_archml_at (o->arch, "resource", j), 1);
          if (ref && ref->text && res && res->text && !strcmp (ref->text, res->text)) {
            resources[k] |= UINT64_C(1) << j; found = 1; break;
          }
        }
        if (!found) return MULIB_MIR_UNSUPPORTED;
      }
    }
    a->resources = resources[k];
  }
  return MULIB_MIR_OK;
}
static mulib_mir_status_t mulib_isel_schedule_ (mulib_lir_t *r, const mulib_isel_options_t *o,
                                              mulib_mir_diagnostic_t *d, size_t *work)
{
  uint64_t *resources = NULL;
  size_t total = 0;
  mulib_mir_status_t s = MULIB_MIR_RESOURCE_LIMIT;
  if (r->body.count > SIZE_MAX / sizeof (*resources)) return s;
  resources = (uint64_t *)calloc (r->body.count, sizeof (*resources));
  if (!resources) return MULIB_MIR_NO_MEMORY;
  s = mulib_isel_tuning_ (r, o, resources, work, d); if (s != MULIB_MIR_OK) goto done;
  for (size_t b = 0; b < r->body.block_count; ++b) {
    size_t first = r->body.blocks[b].first, n = r->body.blocks[b].count, count = 0;
    mulib_isched_node_t *nodes = NULL;
    mulib_isched_resources_t *units = NULL;
    mulib_isched_edge_t *edges = NULL;
    mulib_isched_schedule_t schedule = {NULL, NULL, 0, 0};
    mulib_isched_limits_t limits = {o->limit, o->edge_limit, o->cycle_limit};
    mulib_isched_status_t ss;
    mulib_isched_diagnostic_t sd;
    if (d) d->record = first;
    for (size_t a = 0; a < n; ++a) for (size_t c = a + 1; c < n; ++c) {
      if (!mulib_isel_charge_ (work, 2)) { s = MULIB_MIR_RESOURCE_LIMIT; goto block_done; }
      if (mulib_lir_dependency (r, first + a, first + c)) {
        if (total == o->edge_limit) { s = MULIB_MIR_RESOURCE_LIMIT; goto block_done; }
        ++total; ++count;
      }
    }
    /* Reserve an upper bound for the scheduler's scan-based implementation:
     * topological/priority scans, edge scans and 64-bit resource iteration.
     * Check by division before multiplying, so even SIZE_MAX limits are safe. */
    s = MULIB_MIR_RESOURCE_LIMIT;
    if (n && (n > *work / n || !mulib_isel_charge_ (work, n * n))) goto block_done;
    if (n && (n > *work / n || !mulib_isel_charge_ (work, n * n))) goto block_done;
    if (n && (n > *work / n || !mulib_isel_charge_ (work, n * n))) goto block_done;
    for (unsigned pass = 0; pass < 6; ++pass)
      if (n && (count > *work / n || !mulib_isel_charge_ (work, n * count))) goto block_done;
    if (n > *work / 140 || !mulib_isel_charge_ (work, n * 140)) goto block_done;
    s = MULIB_MIR_RESOURCE_LIMIT;
    if (count > SIZE_MAX / sizeof (*edges) || n > SIZE_MAX / sizeof (*nodes)
        || n > SIZE_MAX / sizeof (*units)) goto block_done;
    nodes = (mulib_isched_node_t *)malloc (n * sizeof (*nodes));
    units = (mulib_isched_resources_t *)malloc (n * sizeof (*units));
    if (count) edges = (mulib_isched_edge_t *)malloc (count * sizeof (*edges));
    s = MULIB_MIR_NO_MEMORY; if (!nodes || !units || (count && !edges)) goto block_done;
    count = 0;
    for (size_t a = 0; a < n; ++a) {
      nodes[a].latency = r->annotations[first + a].latency;
      units[a].mask = resources[first + a];
      units[a].throughput = r->annotations[first + a].throughput;
      for (size_t c = a + 1; c < n; ++c) if (mulib_lir_dependency (r, first + a, first + c)) {
        edges[count].before = a; edges[count++].after = c;
      }
    }
    ss = mulib_isched_run_resources (nodes, units, n, edges, count, !o->schedule, limits, &schedule, &sd);
    s = ss == MULIB_ISCHED_OK ? MULIB_MIR_OK : ss == MULIB_ISCHED_NO_MEMORY
        ? MULIB_MIR_NO_MEMORY : ss == MULIB_ISCHED_OVERFLOW ? MULIB_MIR_OVERFLOW
        : ss == MULIB_ISCHED_RESOURCE_LIMIT ? MULIB_MIR_RESOURCE_LIMIT : MULIB_MIR_BAD_REFERENCE;
    if (s != MULIB_MIR_OK && d) d->record = first + (sd.node < n ? sd.node : 0);
    if (s != MULIB_MIR_OK) goto block_done;
    for (size_t j = 0; j < n; ++j) {
      r->order[first + j] = first + schedule.order[j];
      r->annotations[first + j].cycle = schedule.start[j];
    }
    r->block_cycles[b] = schedule.makespan; s = MULIB_MIR_OK;
block_done:
    free (nodes); free (units); free (edges); mulib_isched_free (&schedule);
    if (s != MULIB_MIR_OK) goto done;
  }
  r->scheduled = 1;
done:
  free (resources); return s;
}
static mulib_mir_status_t mulib_isel_select_ (mulib_lir_t *r, const mulib_isel_options_t *o,
                                            mulib_mir_diagnostic_t *d)
{
  unsigned char *known = NULL;
  uint64_t *values = NULL;
  mulib_mir_status_t s = MULIB_MIR_RESOURCE_LIMIT;
  size_t nr = r->body.register_count, work = o->work_limit;
  if (nr > SIZE_MAX / sizeof (*values)) return s;
  if (nr) { known = (unsigned char *)calloc (nr, 1); values = (uint64_t *)calloc (nr, sizeof (*values)); }
  if (nr && (!known || !values)) { s = MULIB_MIR_NO_MEMORY; goto done; }
  r->cost = 0;
  for (size_t b = 0; b < r->body.block_count; ++b) {
    if (!mulib_isel_charge_ (&work, nr)) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
    if (nr) memset (known, 0, nr);
    for (size_t k = r->body.blocks[b].first; k < r->body.blocks[b].first + r->body.blocks[b].count; ++k) {
      const mulib_mir_native_instruction_t *i = &r->body.code[k];
      mulib_lir_annotation_t *a = &r->annotations[k];
      mulib_machine_selection_t q;
      mulib_muburg_node_t nodes[3];
      mulib_isel_cover_t cover = {NULL, 0, 0, 0};
      mulib_isel_limits_t limits = {3, o->limit, 2, 6, 4, work};
      int arity = mulib_mir_native_arity (i->opcode), constant = 0;
      uint64_t value = 0;
      memset (nodes, 0, sizeof (nodes)); memset (&q, 0, sizeof (q));
      if (d) d->record = k;
      a->known_mask = 0; a->values[0] = a->values[1] = 0;
      for (int j = 0; j < arity; ++j) {
        size_t reg = j ? i->b : i->a;
        nodes[j].opcode = MULIB_MACHINE_REGISTER;
        if (known[reg]) {
          uint64_t bound = UINT64_C(1) << (r->machine->signed_immediate_bits - 1);
          value = values[reg]; a->known_mask |= 1u << j; a->values[j] = value;
          nodes[j].opcode = value < bound || value >= UINT64_MAX - bound + 1
              ? MULIB_MACHINE_IMMEDIATE : MULIB_MACHINE_WIDE;
        }
      }
      nodes[arity].opcode = i->opcode; nodes[arity].arity = (unsigned)arity;
      nodes[arity].child[0] = 0; nodes[arity].child[1] = 1;
      s = mulib_isel_cover_ (nodes, (size_t)arity + 1, r->machine->rules,
          r->machine->rule_count, 2, (size_t)arity, 0, limits, &cover, NULL, &work);
      if (s != MULIB_MIR_OK) goto done;
      q.rule = cover.memo[cover.root].rule; q.cost = cover.cost;
      q.instruction = r->machine->rules[q.rule].instruction; q.latency = r->machine->latencies[q.rule];
      for (int j = 0; j < arity; ++j) if (r->machine->rules[q.rule].child_goal[j] == MULIB_MACHINE_IMM_GOAL) {
        if (!(a->known_mask & (1u << j))) { mulib_isel_cover_free (&cover); s = MULIB_MIR_INVALID; goto done; }
        q.immediate_mask |= 1u << j; q.operands[j] = a->values[j];
      }
      mulib_isel_cover_free (&cover);
      if (q.cost > UINT64_MAX - r->cost) { s = MULIB_MIR_OVERFLOW; goto done; }
      r->selection[k] = q; r->cost += q.cost;
      if (i->opcode == MULIB_MIR_NATIVE_CONST) { constant = 1; value = i->immediate; }
      else if (i->opcode == MULIB_MIR_NATIVE_COPY && a->known_mask) { constant = 1; value = a->values[0]; }
      else if (a->known_mask == 3) constant = mulib_lowopt_evaluate (i->opcode, a->values[0], a->values[1], &value);
      if (mulib_mir_native_defines (i->opcode)) {
        known[i->destination] = (unsigned char)constant; values[i->destination] = value;
      }
    }
  }
  r->selected = 1; r->scheduled = 0; s = mulib_isel_schedule_ (r, o, d, &work);
done:
  free (known); free (values); return s;
}
mulib_mir_status_t mulib_isel_run (const mulib_mir_native_program_t *p,
    const mulib_isel_options_t *options, mulib_lir_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_isel_options_t defaults = mulib_isel_defaults ();
  const mulib_isel_options_t *o = options ? options : &defaults;
  mulib_lir_t r;
  mulib_mir_status_t s;
  if (!mulib_lir_empty (out) || (o->schedule != 0 && o->schedule != 1)) {
    if (d) { d->status = MULIB_MIR_INVALID; d->record = 0; }
    return MULIB_MIR_INVALID;
  }
  memset (&r, 0, sizeof (r));
  s = mulib_lir_from_mir (p, o->machine, o->limit, &r, d);
  if (s == MULIB_MIR_OK) s = mulib_isel_select_ (&r, o, d);
  if (s == MULIB_MIR_OK) { *out = r; memset (&r, 0, sizeof (r)); }
  mulib_lir_free (&r); if (d) d->status = s; return s;
}
mulib_mir_status_t mulib_isel_refresh (mulib_lir_t *r, const mulib_isel_options_t *options,
                                      mulib_mir_diagnostic_t *d)
{
  mulib_isel_options_t defaults = mulib_isel_defaults ();
  const mulib_isel_options_t *o = options ? options : &defaults;
  mulib_lir_t copy, view;
  mulib_mir_status_t s;
  if (!r || r->machine != o->machine || (o->schedule != 0 && o->schedule != 1)) {
    if (d) { d->status = MULIB_MIR_INVALID; d->record = 0; }
    return MULIB_MIR_INVALID;
  }
  memset (&copy, 0, sizeof (copy)); view = *r; view.selected = view.scheduled = 0;
  s = mulib_lir_clone (&view, o->limit, &copy, d);
  if (s == MULIB_MIR_OK) s = mulib_isel_select_ (&copy, o, d);
  if (s == MULIB_MIR_OK) { mulib_lir_free (r); *r = copy; memset (&copy, 0, sizeof (copy)); }
  mulib_lir_free (&copy); if (d) d->status = s; return s;
}
mulib_mir_status_t mulib_isel_schedule (mulib_lir_t *r, const mulib_isel_options_t *options,
                                       mulib_mir_diagnostic_t *d)
{
  mulib_isel_options_t defaults = mulib_isel_defaults ();
  const mulib_isel_options_t *o = options ? options : &defaults;
  mulib_lir_t copy, view;
  size_t work = o->work_limit;
  mulib_mir_status_t s;
  if (!r || !r->selected || r->machine != o->machine || (o->schedule != 0 && o->schedule != 1)) {
    if (d) { d->status = MULIB_MIR_INVALID; d->record = 0; }
    return MULIB_MIR_INVALID;
  }
  memset (&copy, 0, sizeof (copy)); view = *r; view.scheduled = 0;
  s = mulib_lir_clone (&view, o->limit, &copy, d);
  if (s == MULIB_MIR_OK) s = mulib_isel_schedule_ (&copy, o, d, &work);
  if (s == MULIB_MIR_OK) { mulib_lir_free (r); *r = copy; memset (&copy, 0, sizeof (copy)); }
  mulib_lir_free (&copy); if (d) d->status = s; return s;
}
#ifdef __cplusplus
}
#endif
#endif
