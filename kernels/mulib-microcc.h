/** @file mulib-microcc.h
 * @brief Retargetable high-IR to native MIR to scheduled assembly compiler.
 *
 * Pipeline: ClickIR (Sea of Nodes), ANFIR, gated/memory SSA, DepIR or
 * Superblocks -> validated native MIR -> LowOpt -> MuBURG target selection ->
 * latency-aware block scheduling -> ABI-aware assembly emission. Native MIR
 * may also be supplied directly. Optimizing before selection keeps covers and
 * dependence information consistent. PHIs are lowered with edge-local parallel
 * copies. Memory, RAW, WAR, WAW and terminator dependencies constrain scheduling.
 *
 * Built-in targets are ELF x86-64 SysV and RV64IM LP64. They spill virtual
 * registers to stable frame slots, reserving scratch/ABI registers. Target
 * callbacks can implement other allocation strategies and assembly dialects.
 * Assembly is returned as owned text for an external assembler/linker. MuLIB's
 * current typed subset assembler does not parse this textual assembly.
 * Define MULIB_IMPL_MICROCC and all documented dependency switches explicitly.
 */
#ifndef MULIB_MICROCC_H
#define MULIB_MICROCC_H
#include "mulib-depir.h"
#include "mulib-machine.h"
#include "mulib-isched.h"
#include "mulib-lowopt.h"
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_microcc_ir {
  MULIB_MICROCC_NATIVE, MULIB_MICROCC_CLICKIR, MULIB_MICROCC_ANFIR,
  MULIB_MICROCC_GSSAIR, MULIB_MICROCC_DEPIR, MULIB_MICROCC_SUPERBLOCKS
} mulib_microcc_ir_t;
/** program points to the corresponding immutable *_program_t descriptor. */
typedef struct mulib_microcc_source { mulib_microcc_ir_t kind; const void *program; }
  mulib_microcc_source_t;
enum { MULIB_MICROCC_OPTIMIZE = 1, MULIB_MICROCC_SCHEDULE = 2 };
typedef struct mulib_microcc_options {
  const mulib_machine_t *machine;
  size_t limit, edge_limit, cycle_limit, text_limit;
  unsigned flags;
} mulib_microcc_options_t;
typedef enum mulib_microcc_stage {
  MULIB_MICROCC_INPUT, MULIB_MICROCC_LOWER, MULIB_MICROCC_LOWOPT,
  MULIB_MICROCC_SELECT, MULIB_MICROCC_SCHEDULER, MULIB_MICROCC_ASSEMBLY
} mulib_microcc_stage_t;
typedef struct mulib_microcc_diagnostic {
  mulib_microcc_stage_t stage;
  mulib_mir_diagnostic_t mir;
} mulib_microcc_diagnostic_t;
/** Owned compilation artifact. order[position] is an original MIR record ID;
 * starts[record] is its issue cycle relative to its block. Latencies are a
 * scheduling model, not cycle-accurate hardware simulation or emitted delays.
 * Zero initialize, do not copy/edit live owners, free after use. */
typedef struct mulib_microcc_result {
  mulib_mir_native_buffer_t mir;
  mulib_machine_selection_t *selected;
  size_t *order, *starts, *block_cycles;
  char *assembly;
  size_t assembly_length;
  uint64_t cost;
  mulib_lowopt_stats_t optimization;
} mulib_microcc_result_t;
/** Defaults: x86-64, 65536 array/analysis cells, 262144 dependency edges,
 * 1048576 cycles per block and assembly bytes, both passes enabled.
 * Zero limits are literal bounds. */
mulib_microcc_options_t mulib_microcc_defaults (void);
/** Compile atomically. options=NULL uses defaults. symbol is an ASCII C
 * identifier up to 127 bytes. All source arrays remain unchanged; successful
 * output owns everything it needs. Diagnostics identify the failing stage
 * and that stage's node/record, not a source-language location. */
mulib_mir_status_t mulib_microcc_compile (const mulib_microcc_source_t *source,
    const char *symbol, const mulib_microcc_options_t *options,
    mulib_microcc_result_t *out, mulib_microcc_diagnostic_t *diagnostic);
/** Write the already-compiled assembly in one callback. Writer failure can
 * have partial external effects; the compilation artifact remains usable. */
mulib_mir_status_t mulib_microcc_write (const mulib_microcc_result_t *result,
    mulib_mir_write_fn writer, void *user);
/** Release the entire artifact and reset it; NULL is harmless. */
void mulib_microcc_free (mulib_microcc_result_t *result);
/** Static stage name for diagnostics. */
const char *mulib_microcc_stage_string (mulib_microcc_stage_t stage);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MICROCC) && !defined(MULIB_MICROCC_IMPLEMENTED)
#define MULIB_MICROCC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_microcc_options_t mulib_microcc_defaults (void)
{
  mulib_microcc_options_t o;
  o.machine = mulib_machine_x86_64_sysv (); o.limit = 65536;
  o.edge_limit = 262144; o.cycle_limit = 1048576; o.text_limit = 1048576;
  o.flags = MULIB_MICROCC_OPTIMIZE | MULIB_MICROCC_SCHEDULE; return o;
}
void mulib_microcc_free (mulib_microcc_result_t *r)
{
  if (!r) return;
  mulib_mir_native_free (&r->mir); free (r->selected); free (r->order);
  free (r->starts); free (r->block_cycles); free (r->assembly); memset (r, 0, sizeof (*r));
}
const char *mulib_microcc_stage_string (mulib_microcc_stage_t stage)
{
  switch (stage)
    {
    case MULIB_MICROCC_INPUT: return "input/target";
    case MULIB_MICROCC_LOWER: return "IR lowering";
    case MULIB_MICROCC_LOWOPT: return "low-level optimization";
    case MULIB_MICROCC_SELECT: return "instruction selection";
    case MULIB_MICROCC_SCHEDULER: return "instruction scheduling";
    case MULIB_MICROCC_ASSEMBLY: return "assembly emission";
    default: return "unknown stage";
    }
}
static int
mulib_microcc_reads_ (const mulib_mir_native_program_t *p,
                     const mulib_mir_native_instruction_t *i, size_t reg)
{
  int arity = mulib_mir_native_arity (i->opcode);
  size_t k;
  if ((arity > 0 && i->a == reg) || (arity == 2 && i->b == reg)) return 1;
  if (i->opcode == MULIB_MIR_NATIVE_CALL)
    for (k = 0; k < i->b; ++k) if (p->arguments[i->a + k] == reg) return 1;
  return 0;
}
static int
mulib_microcc_dependency_ (const mulib_mir_native_program_t *p, size_t a, size_t b)
{
  const mulib_mir_native_instruction_t *x = &p->code[a], *y = &p->code[b];
  int xd = mulib_mir_native_defines (x->opcode), yd = mulib_mir_native_defines (y->opcode);
  return y->opcode >= MULIB_MIR_NATIVE_JUMP
      || (mulib_mir_native_effect (x->opcode) && mulib_mir_native_effect (y->opcode))
      || (xd && mulib_microcc_reads_ (p, y, x->destination))
      || (yd && mulib_microcc_reads_ (p, x, y->destination))
      || (xd && yd && x->destination == y->destination);
}
static mulib_mir_status_t
mulib_microcc_schedule_ (const mulib_mir_native_program_t *p, const mulib_microcc_options_t *o,
                        mulib_microcc_result_t *r, mulib_mir_diagnostic_t *d)
{
  size_t b, a, c, k, edge_total = 0;
  for (b = 0; b < p->block_count; ++b)
    {
      size_t first = p->blocks[b].first, n = p->blocks[b].count, count = 0;
      mulib_isched_node_t *nodes = NULL;
      mulib_isched_edge_t *edges = NULL;
      mulib_isched_schedule_t schedule;
      mulib_isched_limits_t limits;
      mulib_isched_diagnostic_t sd;
      mulib_isched_status_t ss;
      mulib_mir_status_t s = MULIB_MIR_OK;
      memset (&schedule, 0, sizeof (schedule));
      if (!(o->flags & MULIB_MICROCC_SCHEDULE))
        {
          size_t time = 0;
          for (k = first; k < first + n; ++k)
            {
              r->order[k] = k; r->starts[k] = time;
              if (r->selected[k].latency > o->cycle_limit - time)
                { d->record = k; return MULIB_MIR_RESOURCE_LIMIT; }
              time += r->selected[k].latency;
            }
          r->block_cycles[b] = time; continue;
        }
      for (a = 0; a < n; ++a)
        for (c = a + 1; c < n; ++c)
          if (mulib_microcc_dependency_ (p, first + a, first + c))
            {
              if (count == o->edge_limit - edge_total)
                { d->record = first + c; return MULIB_MIR_RESOURCE_LIMIT; }
              ++count;
            }
      if (count > SIZE_MAX / sizeof (*edges)) return MULIB_MIR_RESOURCE_LIMIT;
      edge_total += count;
      nodes = (mulib_isched_node_t *)malloc (n * sizeof (*nodes));
      if (count) edges = (mulib_isched_edge_t *)malloc (count * sizeof (*edges));
      if (!nodes || (count && !edges)) { free (nodes); free (edges); return MULIB_MIR_NO_MEMORY; }
      k = 0;
      for (a = 0; a < n; ++a)
        {
          nodes[a].latency = r->selected[first + a].latency;
          for (c = a + 1; c < n; ++c)
            if (mulib_microcc_dependency_ (p, first + a, first + c))
              { edges[k].before = a; edges[k++].after = c; }
        }
      limits.nodes = o->limit; limits.edges = o->edge_limit; limits.cycles = o->cycle_limit;
      ss = mulib_isched_run (nodes, n, edges, count, limits, &schedule, &sd);
      if (ss != MULIB_ISCHED_OK)
        {
          d->record = first + (sd.node < n ? sd.node : 0);
          s = ss == MULIB_ISCHED_NO_MEMORY ? MULIB_MIR_NO_MEMORY
              : ss == MULIB_ISCHED_RESOURCE_LIMIT || ss == MULIB_ISCHED_OVERFLOW
                ? MULIB_MIR_RESOURCE_LIMIT : MULIB_MIR_BAD_REFERENCE;
        }
      else
        {
          for (k = 0; k < n; ++k)
            { r->order[first + k] = first + schedule.order[k]; r->starts[first + k] = schedule.start[k]; }
          r->block_cycles[b] = schedule.makespan;
        }
      free (nodes); free (edges); mulib_isched_free (&schedule);
      if (s != MULIB_MIR_OK) return s;
    }
  return MULIB_MIR_OK;
}
typedef struct mulib_microcc_text_ {
  mulib_strbuf_t buffer;
  size_t limit;
  mulib_mir_status_t status;
} mulib_microcc_text_;
static int mulib_microcc_writer_ (void *user, const char *bytes, size_t count)
{
  mulib_microcc_text_ *t = (mulib_microcc_text_ *)user;
  if (t->status != MULIB_MIR_OK) return 0;
  if (count > t->limit - t->buffer.length || count == SIZE_MAX - t->buffer.length)
    { t->status = MULIB_MIR_RESOURCE_LIMIT; return 0; }
  if (!mulib_strbuf_append_n (&t->buffer, bytes, count))
    { t->status = MULIB_MIR_NO_MEMORY; return 0; }
  return 1;
}
mulib_mir_status_t
mulib_microcc_compile (const mulib_microcc_source_t *source, const char *symbol,
                      const mulib_microcc_options_t *options, mulib_microcc_result_t *out,
                      mulib_microcc_diagnostic_t *diagnostic)
{
  mulib_microcc_options_t defaults = mulib_microcc_defaults ();
  const mulib_microcc_options_t *o = options ? options : &defaults;
  mulib_microcc_result_t r;
  mulib_microcc_diagnostic_t d;
  mulib_microcc_text_ text;
  mulib_mir_native_program_t p;
  unsigned char *known = NULL;
  uint64_t *values = NULL;
  size_t b, k;
  mulib_mir_status_t s = MULIB_MIR_INVALID;
  memset (&r, 0, sizeof (r)); memset (&d, 0, sizeof (d)); memset (&text, 0, sizeof (text));
  if (!source || !source->program || !out || out->assembly || out->selected || out->order
      || out->starts || out->block_cycles || out->mir.code || out->mir.blocks || out->mir.symbols
      || out->mir.arguments || out->mir.count || out->mir.block_count || out->mir.register_count
      || out->mir.parameter_count || out->mir.argument_count || out->mir.symbol_count
      || out->assembly_length || out->cost || !mulib_mir_native_name (symbol)
      || (o->flags & ~(unsigned)(MULIB_MICROCC_OPTIMIZE | MULIB_MICROCC_SCHEDULE))) goto done;
  s = mulib_machine_validate (o->machine, o->limit);
  if (s != MULIB_MIR_OK) goto done;
  d.stage = MULIB_MICROCC_LOWER;
  switch (source->kind)
    {
    case MULIB_MICROCC_NATIVE: s = mulib_mir_native_clone ((const mulib_mir_native_program_t *)source->program, o->limit, &r.mir, &d.mir); break;
    case MULIB_MICROCC_CLICKIR: s = mulib_clickir_lower ((const mulib_clickir_program_t *)source->program, o->limit, &r.mir, &d.mir); break;
    case MULIB_MICROCC_ANFIR: s = mulib_anfir_lower ((const mulib_anfir_program_t *)source->program, o->limit, &r.mir, &d.mir); break;
    case MULIB_MICROCC_GSSAIR: s = mulib_gssair_lower ((const mulib_gssair_program_t *)source->program, o->limit, &r.mir, &d.mir); break;
    case MULIB_MICROCC_DEPIR: s = mulib_depir_lower ((const mulib_depir_program_t *)source->program, o->limit, &r.mir, &d.mir); break;
    case MULIB_MICROCC_SUPERBLOCKS: s = mulib_superblocks_lower ((const mulib_superblocks_program_t *)source->program, o->limit, &r.mir, &d.mir); break;
    default: s = MULIB_MIR_INVALID; break;
    }
  if (s != MULIB_MIR_OK) goto done;
  p = mulib_mir_native_view (&r.mir);
  if (o->flags & MULIB_MICROCC_OPTIMIZE)
    {
      mulib_mir_native_buffer_t optimized;
      memset (&optimized, 0, sizeof (optimized)); d.stage = MULIB_MICROCC_LOWOPT;
      s = mulib_lowopt_run (&p, o->limit, &optimized, &r.optimization, &d.mir);
      if (s != MULIB_MIR_OK) goto done;
      mulib_mir_native_free (&r.mir); r.mir = optimized; p = mulib_mir_native_view (&r.mir);
    }
  d.stage = MULIB_MICROCC_SELECT; d.mir.record = 0;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (p.count > SIZE_MAX / sizeof (*r.selected)) goto done;
  r.selected = (mulib_machine_selection_t *)calloc (p.count, sizeof (*r.selected));
  r.order = (size_t *)malloc (p.count * sizeof (*r.order));
  r.starts = (size_t *)malloc (p.count * sizeof (*r.starts));
  r.block_cycles = (size_t *)malloc (p.block_count * sizeof (*r.block_cycles));
  if (p.register_count)
    { known = (unsigned char *)calloc (p.register_count, 1); values = (uint64_t *)calloc (p.register_count, sizeof (*values)); }
  s = MULIB_MIR_NO_MEMORY;
  if (!r.selected || !r.order || !r.starts || !r.block_cycles || (p.register_count && (!known || !values))) goto done;
  for (b = 0; b < p.block_count; ++b)
    {
      if (p.register_count) memset (known, 0, p.register_count);
      for (k = p.blocks[b].first; k < p.blocks[b].first + p.blocks[b].count; ++k)
        {
          const mulib_mir_native_instruction_t *i = &p.code[k];
          uint64_t operands[2] = {0,0}, value = 0;
          unsigned mask = 0;
          int arity = mulib_mir_native_arity (i->opcode), is_constant = 0;
          d.mir.record = k;
          if (arity > 0 && known[i->a]) { mask |= 1; operands[0] = values[i->a]; }
          if (arity == 2 && known[i->b]) { mask |= 2; operands[1] = values[i->b]; }
          s = mulib_machine_select (o->machine, i, mask, operands, o->limit, &r.selected[k]);
          if (s != MULIB_MIR_OK) goto done;
          if (r.selected[k].cost > UINT64_MAX - r.cost) { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
          r.cost += r.selected[k].cost;
          if (i->opcode == MULIB_MIR_NATIVE_CONST) { value = i->immediate; is_constant = 1; }
          else if (i->opcode == MULIB_MIR_NATIVE_COPY && (mask & 1)) { value = operands[0]; is_constant = 1; }
          else if (mask == 3) is_constant = mulib_lowopt_evaluate (i->opcode, operands[0], operands[1], &value);
          if (mulib_mir_native_defines (i->opcode))
            { known[i->destination] = (unsigned char)is_constant; if (is_constant) values[i->destination] = value; }
        }
    }
  d.stage = MULIB_MICROCC_SCHEDULER; d.mir.record = 0;
  s = mulib_microcc_schedule_ (&p, o, &r, &d.mir);
  if (s != MULIB_MIR_OK) goto done;
  d.stage = MULIB_MICROCC_ASSEMBLY; d.mir.record = 0; text.limit = o->text_limit;
  s = mulib_machine_emit (o->machine, &p, r.selected, r.order, symbol, o->limit,
      mulib_microcc_writer_, &text, &d.mir);
  if (text.status != MULIB_MIR_OK) s = text.status;
  if (s != MULIB_MIR_OK) goto done;
  if (!text.buffer.data && !mulib_microcc_writer_ (&text, "", 0)) { s = text.status; goto done; }
  r.assembly = text.buffer.data; r.assembly_length = text.buffer.length;
  memset (&text.buffer, 0, sizeof (text.buffer)); *out = r; memset (&r, 0, sizeof (r));
done:
  free (known); free (values); mulib_strbuf_free (&text.buffer); mulib_microcc_free (&r);
  d.mir.status = s; if (diagnostic) *diagnostic = d; return s;
}
mulib_mir_status_t
mulib_microcc_write (const mulib_microcc_result_t *r, mulib_mir_write_fn writer, void *user)
{
  if (!r || !r->assembly || !writer) return MULIB_MIR_INVALID;
  return writer (user, r->assembly, r->assembly_length) ? MULIB_MIR_OK : MULIB_MIR_IO_ERROR;
}
#ifdef __cplusplus
}
#endif
#endif
