/** @file mulib-vmweave.h
 * @brief MIR optimization and a resumable typed register virtual machine.
 * Define MULIB_IMPL_VMWEAVE; explicitly implement MIR, EGRAPH and REWRITE.
 */
#ifndef MULIB_VMWEAVE_H
#define MULIB_VMWEAVE_H
#include "mulib-mir.h"
#include "mulib-rewrite.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Optimize valid MIR into an empty owned buffer. record_limit bounds input
   * and scratch. Dead potentially trapping arithmetic and PARAM Boolean checks
   * are retained. Faulting constant expressions remain operations;
   * optimization itself does not execute them. Failure leaves output empty.
   * Input/output must not alias.
   */
  mulib_mir_status_t
  mulib_vmweave_optimize (const mulib_mir_program_t *program,
                          size_t record_limit, mulib_mir_buffer_t *output,
                           mulib_mir_diagnostic_t *diagnostic);
  typedef struct mulib_vmweave_program mulib_vmweave_program_t;
  /** Validate and deep-copy a register/control-flow program. Optional local
   * constant folding respects block boundaries and preserves faulting records
   * and host calls. No input arrays are retained. *output must be NULL.
   */
  mulib_mir_status_t mulib_vmweave_compile (
      const mulib_mir_flow_program_t *program, size_t limit, int optimize,
      mulib_vmweave_program_t **output, mulib_mir_diagnostic_t *diagnostic);
  /** Borrow an immutable image; it must outlive all frames initialized with it. */
  const mulib_mir_flow_program_t *
  mulib_vmweave_program_view (const mulib_vmweave_program_t *program);
  void mulib_vmweave_program_destroy (mulib_vmweave_program_t *program);
  /** Run/resume a frame for at most budget additional instructions. Returns
   * FINISHED, SUSPENDED, STEP_LIMIT, or a fault. Run after FINISHED is idempotent.
   * Complete a suspended call with mulib_mir_frame_resume before running again.
   * A zero budget executes nothing. Faults and host side effects persist.
   */
  mulib_mir_status_t mulib_vmweave_run (mulib_mir_frame_t *frame, size_t budget);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VMWEAVE) && !defined(MULIB_VMWEAVE_IMPLEMENTED)
#define MULIB_VMWEAVE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  struct mulib_vmweave_program
  {
    mulib_mir_flow_program_t view;
    mulib_mir_flow_instruction_t *code;
    mulib_mir_type_t *types, *parameters;
    mulib_mir_signature_t *hosts;
  };
  const mulib_mir_flow_program_t *
  mulib_vmweave_program_view (const mulib_vmweave_program_t *p)
  {
    return p ? &p->view : NULL;
  }
  void
  mulib_vmweave_program_destroy (mulib_vmweave_program_t *p)
  {
    if (p)
      { free (p->code); free (p->types); free (p->parameters); free (p->hosts); free (p); }
  }
  /* Saturate typed integer identities only. Operands have already executed,
   * so eliminating this scalar operation cannot eliminate their effects.
   * Dynamic WORD handles are opaque: only explicitly wrapping/bitwise MIR
   * operations are eligible. Floating and host operations never enter here. */
  static mulib_mir_status_t
  mulib_vmweave_identity_ (mulib_mir_flow_instruction_t *i,
                          const mulib_mir_flow_program_t *p,
                          const unsigned char *known,
                          const mulib_mir_value_t *values)
  {
    static const mulib_mir_flow_opcode_t operations[] = {
      MULIB_MIR_FLOW_ADD, MULIB_MIR_FLOW_SUB, MULIB_MIR_FLOW_MUL,
      MULIB_MIR_FLOW_WRAP_ADD, MULIB_MIR_FLOW_WRAP_SUB, MULIB_MIR_FLOW_WRAP_MUL,
      MULIB_MIR_FLOW_OR, MULIB_MIR_FLOW_XOR, MULIB_MIR_FLOW_SHL, MULIB_MIR_FLOW_SHR
    };
    mulib_rewrite_pattern_t lhs[10][3], rhs;
    mulib_rewrite_rule_t rules[10];
    mulib_rewrite_limits_t limits = { 10, 3, 100, 10000 };
    mulib_rewrite_result_t result;
    mulib_egraph_extract_limits_t extraction = { 256, 8 };
    mulib_egraph_tree_t tree;
    mulib_egraph_t *graph = NULL;
    mulib_egraph_node_t node;
    mulib_egraph_status_t es;
    size_t k, a, b, root, matched = SIZE_MAX;
    int64_t literal;
    mulib_mir_type_t type;
    if (i->destination == MULIB_MIR_NONE || i->b == MULIB_MIR_NONE) return MULIB_MIR_OK;
    type = p->register_types[i->destination];
    if (type != MULIB_MIR_I64 && type != MULIB_MIR_WORD) return MULIB_MIR_OK;
    for (k = 0; k < 10; ++k) if (i->opcode == operations[k]) matched = k;
    if (matched == SIZE_MAX || !known[i->b]) return MULIB_MIR_OK;
    /* Constants other than zero/one cannot match these rules. */
    if (type == MULIB_MIR_WORD)
      {
        if (values[i->b].word > 1) return MULIB_MIR_OK;
        literal = (int64_t)values[i->b].word;
      }
    else
      {
        literal = values[i->b].integer;
        if (literal != 0 && literal != 1) return MULIB_MIR_OK;
      }
    memset (lhs, 0, sizeof (lhs)); memset (&rhs, 0, sizeof (rhs));
    memset (&tree, 0, sizeof (tree));
    rhs.kind = MULIB_REWRITE_VARIABLE;
    for (k = 0; k < 10; ++k)
      {
        lhs[k][0].kind = MULIB_REWRITE_VARIABLE;
        lhs[k][1].kind = lhs[k][2].kind = MULIB_REWRITE_NODE;
        lhs[k][1].opcode = 1;
        lhs[k][1].literal = operations[k] == MULIB_MIR_FLOW_MUL
                              || operations[k] == MULIB_MIR_FLOW_WRAP_MUL;
        lhs[k][2].opcode = 256 + (unsigned)operations[k]; lhs[k][2].arity = 2;
        lhs[k][2].child[0] = 0; lhs[k][2].child[1] = 1;
        rules[k].lhs = lhs[k]; rules[k].lhs_count = 3;
        rules[k].rhs = &rhs; rules[k].rhs_count = 1;
      }
    es = mulib_egraph_create (8, &graph);
    if (es != MULIB_EGRAPH_OK) goto done;
    memset (&node, 0, sizeof (node)); node.opcode = 2;
    es = mulib_egraph_add (graph, node, &a); if (es != MULIB_EGRAPH_OK) goto done;
    node.opcode = 1; node.literal = literal;
    es = mulib_egraph_add (graph, node, &b); if (es != MULIB_EGRAPH_OK) goto done;
    node.opcode = 256 + (unsigned)i->opcode; node.arity = 2; node.literal = 0;
    node.child[0] = a; node.child[1] = b;
    es = mulib_egraph_add (graph, node, &root); if (es != MULIB_EGRAPH_OK) goto done;
    if (mulib_rewrite_run (graph, rules, 10, limits, &result) != MULIB_REWRITE_OK)
      { es = result.graph_status == MULIB_EGRAPH_NO_MEMORY ? MULIB_EGRAPH_NO_MEMORY : MULIB_EGRAPH_INVALID; goto done; }
    es = mulib_egraph_extract (graph, root, NULL, NULL, extraction, &tree);
    if (es == MULIB_EGRAPH_OK && tree.count == 1 && tree.nodes[0].opcode == 2)
      { i->opcode = MULIB_MIR_FLOW_COPY; i->b = MULIB_MIR_NONE; }
  done:
    mulib_egraph_tree_free (&tree); mulib_egraph_destroy (graph);
    return es == MULIB_EGRAPH_OK ? MULIB_MIR_OK
           : es == MULIB_EGRAPH_NO_MEMORY ? MULIB_MIR_NO_MEMORY : MULIB_MIR_INVALID;
  }
  static mulib_mir_status_t
  mulib_vmweave_fold_flow_ (mulib_vmweave_program_t *p)
  {
    const mulib_mir_flow_program_t *v = &p->view;
    unsigned char *known = NULL, *leader = NULL;
    mulib_mir_value_t *values = NULL;
    size_t k;
    mulib_mir_status_t status = MULIB_MIR_OK;
    known = (unsigned char *)calloc (v->register_count, 1);
    leader = (unsigned char *)calloc (v->count, 1);
    values = (mulib_mir_value_t *)calloc (v->register_count, sizeof (*values));
    if (!known || !leader || !values)
      { free (known); free (leader); free (values); return MULIB_MIR_NO_MEMORY; }
    leader[0] = 1;
    for (k = 0; k < v->count; ++k)
      if (p->code[k].opcode == MULIB_MIR_FLOW_JUMP
          || p->code[k].opcode == MULIB_MIR_FLOW_BRANCH)
        {
          leader[p->code[k].target] = 1;
          if (k + 1 < v->count) leader[k + 1] = 1;
        }
      else if (p->code[k].opcode == MULIB_MIR_FLOW_RETURN && k + 1 < v->count)
        leader[k + 1] = 1;
    for (k = 0; k < v->count; ++k)
      {
        mulib_mir_flow_instruction_t *i = &p->code[k];
        int fold = 0;
        if (leader[k]) memset (known, 0, v->register_count);
        if (i->opcode == MULIB_MIR_FLOW_CONST)
          { values[i->destination] = i->immediate; known[i->destination] = 1; continue; }
        if (i->opcode == MULIB_MIR_FLOW_BRANCH && known[i->a])
          {
            i->target = values[i->a].integer ? i->target : k + 1;
            i->opcode = MULIB_MIR_FLOW_JUMP; i->a = MULIB_MIR_NONE;
            continue;
          }
        if (i->opcode >= MULIB_MIR_FLOW_COPY && i->opcode <= MULIB_MIR_FLOW_F64_I64)
          fold = known[i->a] && (i->b == MULIB_MIR_NONE || known[i->b])
                 && (i->opcode == MULIB_MIR_FLOW_COPY
                     || (v->register_types[i->a] != MULIB_MIR_F64
                         && v->register_types[i->destination] != MULIB_MIR_F64));
        if (fold)
          {
            mulib_mir_frame_t f;
            memset (&f, 0, sizeof (f));
            f.program = v; f.registers = values; f.pc = k; f.remaining = 1;
            if (mulib_mir_frame_step (&f) == MULIB_MIR_OK)
              {
                i->opcode = MULIB_MIR_FLOW_CONST;
                i->a = i->b = i->target = MULIB_MIR_NONE;
                i->immediate = values[i->destination]; known[i->destination] = 1;
                continue;
              }
          }
        status = mulib_vmweave_identity_ (i, v, known, values);
        if (status != MULIB_MIR_OK) break;
        if (i->destination != MULIB_MIR_NONE) known[i->destination] = 0;
      }
    free (known); free (leader); free (values);
    return status;
  }
  mulib_mir_status_t
  mulib_vmweave_compile (const mulib_mir_flow_program_t *source, size_t limit,
                          int optimize, mulib_vmweave_program_t **out,
                          mulib_mir_diagnostic_t *d)
  {
    mulib_vmweave_program_t *p = NULL;
    mulib_mir_status_t s;
    size_t count, k, at;
    if (!out || *out) { s = MULIB_MIR_INVALID; goto fail; }
    s = mulib_mir_flow_validate (source, limit, d);
    if (s != MULIB_MIR_OK) return s;
    count = source->signature.parameter_count;
    for (k = 0; k < source->host_count; ++k)
      {
        if (source->hosts[k].parameter_count > SIZE_MAX - count)
          { s = MULIB_MIR_RESOURCE_LIMIT; goto fail; }
        count += source->hosts[k].parameter_count;
      }
    if (count > SIZE_MAX / sizeof (mulib_mir_type_t))
      { s = MULIB_MIR_RESOURCE_LIMIT; goto fail; }
    p = (mulib_vmweave_program_t *)calloc (1, sizeof (*p));
    if (!p) { s = MULIB_MIR_NO_MEMORY; goto fail; }
    p->code = (mulib_mir_flow_instruction_t *)malloc (source->count * sizeof (*p->code));
    p->types = (mulib_mir_type_t *)malloc (source->register_count * sizeof (*p->types));
    if (count) p->parameters = (mulib_mir_type_t *)malloc (count * sizeof (*p->parameters));
    if (source->host_count) p->hosts = (mulib_mir_signature_t *)malloc (source->host_count * sizeof (*p->hosts));
    if (!p->code || !p->types || (count && !p->parameters) || (source->host_count && !p->hosts))
      { s = MULIB_MIR_NO_MEMORY; goto fail; }
    memcpy (p->code, source->code, source->count * sizeof (*p->code));
    memcpy (p->types, source->register_types, source->register_count * sizeof (*p->types));
    p->view = *source; p->view.code = p->code; p->view.register_types = p->types;
    p->view.hosts = p->hosts; p->view.signature.parameters = p->parameters;
    at = source->signature.parameter_count;
    if (at) memcpy (p->parameters, source->signature.parameters, at * sizeof (*p->parameters));
    for (k = 0; k < source->host_count; ++k)
      {
        size_t n = source->hosts[k].parameter_count;
        p->hosts[k] = source->hosts[k];
        p->hosts[k].parameters = n ? p->parameters + at : NULL;
        if (n) memcpy (p->parameters + at, source->hosts[k].parameters, n * sizeof (*p->parameters));
        at += n;
      }
    if (optimize)
      {
        s = mulib_vmweave_fold_flow_ (p);
        if (s != MULIB_MIR_OK) goto fail;
      }
    *out = p;
    return MULIB_MIR_OK;
  fail:
    mulib_vmweave_program_destroy (p);
    if (d) { d->status = s; d->record = 0; }
    return s;
  }
  mulib_mir_status_t
  mulib_vmweave_run (mulib_mir_frame_t *f, size_t budget)
  {
    mulib_mir_status_t s;
    if (!f || !f->program || f->running) return MULIB_MIR_INVALID;
    f->remaining = budget;
    do { s = mulib_mir_frame_step (f); } while (s == MULIB_MIR_OK);
    return s;
  }
  mulib_mir_status_t
  mulib_vmweave_optimize (const mulib_mir_program_t *p, size_t limit,
                          mulib_mir_buffer_t *out, mulib_mir_diagnostic_t *d)
  {
    mulib_mir_buffer_t temp = { NULL, 0, 0, 0 }, result = { NULL, 0, 0, 0 };
    size_t *map = NULL, k = 0;
    unsigned char *live = NULL;
    mulib_mir_status_t s;
    if (!out || out->code || out->count || out->capacity)
      {
        s = MULIB_MIR_INVALID;
        goto done;
      }
    s = mulib_mir_validate (p, limit, d);
    if (s != MULIB_MIR_OK)
      return s;
    if (p->count > SIZE_MAX / sizeof (*map))
      {
        s = MULIB_MIR_RESOURCE_LIMIT;
        goto done;
      }
    s = mulib_mir_buffer_init (&temp, p->count, p->parameter_count);
    if (s != MULIB_MIR_OK)
      goto done;
    map = (size_t *)malloc (p->count * sizeof (*map));
    live = (unsigned char *)calloc (p->count, 1);
    if (!map || !live)
      {
        s = MULIB_MIR_NO_MEMORY;
        goto done;
      }
    for (k = 0; k < p->count; ++k)
      {
        mulib_mir_instruction_t i = p->code[k];
        int arity = mulib_mir_arity (i.opcode);
        int64_t value = 0;
        if (arity)
          i.a = map[i.a];
        if (arity == 2)
          i.b = map[i.b];
        if (i.opcode == MULIB_MIR_COPY)
          {
            map[k] = i.a;
            continue;
          }
        if (arity && i.opcode != MULIB_MIR_RETURN
            && temp.code[i.a].opcode == MULIB_MIR_CONST
            && (arity == 1 || temp.code[i.b].opcode == MULIB_MIR_CONST)
            && mulib_mir_evaluate (i.opcode, temp.code[i.a].immediate,
                                   arity == 2 ? temp.code[i.b].immediate : 0,
                                   0, &value)
                   == MULIB_MIR_OK)
          {
            i.opcode = MULIB_MIR_CONST;
            i.a = i.b = MULIB_MIR_NONE;
            i.immediate = value;
          }
        map[k] = temp.count;
        temp.code[temp.count++] = i;
      }
    for (k = temp.count; k-- > 0;)
      {
        mulib_mir_instruction_t *i = &temp.code[k];
        int arity = mulib_mir_arity (i->opcode);
        int trapping
            = i->opcode >= MULIB_MIR_ADD && i->opcode <= MULIB_MIR_NEG;
        if (i->opcode == MULIB_MIR_RETURN || i->opcode == MULIB_MIR_PARAM
            || trapping)
          live[k] = 1;
        if (live[k])
          {
            if (arity)
              live[i->a] = 1;
            if (arity == 2)
              live[i->b] = 1;
          }
      }
    s = mulib_mir_buffer_init (&result, temp.count, p->parameter_count);
    if (s != MULIB_MIR_OK)
      goto done;
    for (k = 0; k < temp.count; ++k)
      if (live[k])
        {
          mulib_mir_instruction_t i = temp.code[k];
          int arity = mulib_mir_arity (i.opcode);
          if (arity)
            i.a = map[i.a];
          if (arity == 2)
            i.b = map[i.b];
          map[k] = result.count;
          result.code[result.count++] = i;
        }
    *out = result;
    memset (&result, 0, sizeof (result));
    s = MULIB_MIR_OK;
  done:
    free (map);
    free (live);
    mulib_mir_buffer_free (&temp);
    mulib_mir_buffer_free (&result);
    if (d)
      {
        d->status = s;
        d->record = k;
      }
    return s;
  }
#ifdef __cplusplus
}
#endif
#endif
