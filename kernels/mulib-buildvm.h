/** @file mulib-buildvm.h
 * @brief Hermetic, bounded artifact-register virtual machine for build DAG tasks.
 * Define MULIB_IMPL_BUILDVM plus BUILDGRAPH, ARTIFACT and VCDIFF separately.
 */
#ifndef MULIB_BUILDVM_H
#define MULIB_BUILDVM_H
#include "mulib-buildgraph.h"
#include "mulib-vcdiff.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_buildvm_opcode {
  MULIB_BUILDVM_INPUT, MULIB_BUILDVM_LITERAL, MULIB_BUILDVM_CONCAT,
  MULIB_BUILDVM_SLICE, MULIB_BUILDVM_DIFF, MULIB_BUILDVM_PATCH,
  MULIB_BUILDVM_EMIT
} mulib_buildvm_opcode_t;
enum { MULIB_BUILDVM_OK = 0, MULIB_BUILDVM_INVALID = -1,
       MULIB_BUILDVM_MEMORY = -2, MULIB_BUILDVM_LIMIT = -3 };
/** INPUT: dst <- input[a]. LITERAL: dst <- data[0:length].
 * CONCAT: dst <- reg[a] + reg[b]. SLICE: dst <- reg[a][offset:length].
 * DIFF: dst <- VCDIFF(reg[a],reg[b]); PATCH: dst <- apply(reg[a],reg[b]),
 * with length specifying PATCH output capacity. EMIT: output[b] <- reg[a].
 * id/type label newly created values (NULL defaults); data and strings borrowed.
 * Instructions are inert data; unknown opcodes always fail. */
typedef struct mulib_buildvm_instruction {
  mulib_buildvm_opcode_t opcode;
  size_t dst, a, b, offset, length;
  const void *data;
  const char *id, *type;
} mulib_buildvm_instruction_t;
typedef struct mulib_buildvm_program {
  const mulib_buildvm_instruction_t *code;
  size_t count, registers;
  size_t max_bytes; /**< Cumulative loaded/created bytes; zero defaults to 64 MiB.
                        DIFF/PATCH charge their capacity, including scratch. */
} mulib_buildvm_program_t;
/** Check all structural operands before execution (input/output slots checked
 * against actual bindings during execute). 0 success, INVALID otherwise. */
int mulib_buildvm_validate (const mulib_buildvm_program_t *);
/** BuildGraph task callback. Deterministic, no host file/environment/process,
 * clock or network capabilities. Only BYTES inputs are accessible. All outputs
 * are staged in step; graph commits only when the whole program succeeds. */
int mulib_buildvm_execute (void *program, mulib_buildgraph_step_t *step);
/** Register a validated borrowed program as a graph task. Program, code and
 * literal data must remain immutable and alive through every graph run. */
int mulib_buildvm_task (mulib_buildgraph_t *, const char *name,
                       const mulib_buildvm_program_t *, size_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BUILDVM) && !defined(MULIB_BUILDVM_IMPLEMENTED)
#define MULIB_BUILDVM_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
int mulib_buildvm_validate (const mulib_buildvm_program_t *p)
{
  size_t i;
  if (!p || !p->registers || p->registers > 65536 || p->count > 65536
      || (!p->code && p->count)) return MULIB_BUILDVM_INVALID;
  for (i = 0; i < p->count; ++i) {
    const mulib_buildvm_instruction_t *c = p->code + i;
    if (c->opcode < MULIB_BUILDVM_INPUT || c->opcode > MULIB_BUILDVM_EMIT
        || (c->opcode != MULIB_BUILDVM_EMIT && c->dst >= p->registers)
        || (c->opcode >= MULIB_BUILDVM_CONCAT && c->a >= p->registers)
        || ((c->opcode == MULIB_BUILDVM_CONCAT || c->opcode == MULIB_BUILDVM_DIFF
             || c->opcode == MULIB_BUILDVM_PATCH) && c->b >= p->registers)
        || (c->opcode == MULIB_BUILDVM_LITERAL && !c->data && c->length)
        || (c->id && !*c->id) || (c->type && !*c->type)) return MULIB_BUILDVM_INVALID;
  }
  return 0;
}
int mulib_buildvm_task (mulib_buildgraph_t *g, const char *name,
                       const mulib_buildvm_program_t *p, size_t *out)
{
  if (mulib_buildvm_validate (p)) return MULIB_BUILDVM_INVALID;
  return mulib_buildgraph_task (g, name, mulib_buildvm_execute, (void *)p, out);
}
int mulib_buildvm_execute (void *program, mulib_buildgraph_step_t *step)
{
  const mulib_buildvm_program_t *p = (const mulib_buildvm_program_t *)program;
  mulib_artifact_t **regs;
  const mulib_buildgraph_binding_t *inputs;
  size_t input_count, output_count, i, charged = 0, limit;
  int rc = mulib_buildvm_validate (p);
  if (rc || !step) return MULIB_BUILDVM_INVALID;
  inputs = mulib_buildgraph_inputs (step, &input_count);
  (void)mulib_buildgraph_outputs (step, &output_count);
  for (i = 0; i < p->count; ++i)
    if ((p->code[i].opcode == MULIB_BUILDVM_INPUT && p->code[i].a >= input_count)
        || (p->code[i].opcode == MULIB_BUILDVM_EMIT && p->code[i].b >= output_count)) return MULIB_BUILDVM_INVALID;
  regs = (mulib_artifact_t **)calloc (p->registers, sizeof (*regs));
  if (!regs) return MULIB_BUILDVM_MEMORY;
  limit = p->max_bytes ? p->max_bytes : 64 * 1024 * 1024;
  for (i = 0; i < p->count; ++i) {
    const mulib_buildvm_instruction_t *c = p->code + i;
    const mulib_artifact_desc_t *a = NULL, *b = NULL;
    mulib_artifact_desc_t d = {0};
    mulib_artifact_t *value = NULL;
    void *buffer = NULL;
    size_t n = c->length, used = 0, cost;
    if (c->opcode >= MULIB_BUILDVM_CONCAT) {
      a = mulib_artifact_view (regs[c->a]);
      if (!a || a->kind != MULIB_ARTIFACT_BYTES) { rc = MULIB_BUILDVM_INVALID; break; }
    }
    if (c->opcode == MULIB_BUILDVM_EMIT) {
      if (mulib_buildgraph_output (step, c->b, regs[c->a])) { rc = MULIB_BUILDVM_MEMORY; break; }
      continue;
    }
    d.id = c->id ? c->id : "buildvm:value"; d.type = c->type ? c->type : "file";
    if (c->opcode == MULIB_BUILDVM_INPUT) {
      a = mulib_artifact_view (inputs[c->a].artifact);
      if (!a || a->kind != MULIB_ARTIFACT_BYTES) { rc = MULIB_BUILDVM_INVALID; break; }
      n = a->length;
    } else if (c->opcode == MULIB_BUILDVM_LITERAL) d.data = c->data;
    else if (c->opcode == MULIB_BUILDVM_SLICE) {
      if (c->offset > a->length || n > a->length - c->offset) { rc = MULIB_BUILDVM_INVALID; break; }
      d.data = n ? (const unsigned char *)a->data + c->offset : NULL;
    } else {
      b = mulib_artifact_view (regs[c->b]);
      if (!b || b->kind != MULIB_ARTIFACT_BYTES) { rc = MULIB_BUILDVM_INVALID; break; }
      if (c->opcode == MULIB_BUILDVM_CONCAT) {
        if (b->length > SIZE_MAX - a->length) { rc = MULIB_BUILDVM_LIMIT; break; }
        n = a->length + b->length;
      } else if (c->opcode == MULIB_BUILDVM_DIFF) {
        if (b->length > SIZE_MAX - 128) { rc = MULIB_BUILDVM_LIMIT; break; }
        n = b->length + 128; d.type = c->type ? c->type : "diff/vcdiff";
      }
    }
    if (n > limit - charged || n > (size_t)PTRDIFF_MAX) { rc = MULIB_BUILDVM_LIMIT; break; }
    cost = n;
    if (c->opcode == MULIB_BUILDVM_INPUT) value = mulib_artifact_retain ((mulib_artifact_t *)inputs[c->a].artifact);
    else {
      if (c->opcode == MULIB_BUILDVM_CONCAT || c->opcode == MULIB_BUILDVM_DIFF || c->opcode == MULIB_BUILDVM_PATCH) {
        buffer = malloc (n ? n : 1);
        if (!buffer) { rc = MULIB_BUILDVM_MEMORY; break; }
        if (c->opcode == MULIB_BUILDVM_CONCAT) {
          if (a->length) memcpy (buffer, a->data, a->length);
          if (b->length) memcpy ((unsigned char *)buffer + a->length, b->data, b->length);
        } else if (c->opcode == MULIB_BUILDVM_DIFF) {
          if (!mulib_vcdiff_encode (a->data, a->length, b->data, b->length, buffer, n, &used)) rc = MULIB_BUILDVM_INVALID;
          n = used;
        } else {
          int status = mulib_vcdiff_decode (a->data, a->length, b->data, b->length, buffer, n, &used);
          if (status != 1) rc = status == -2 ? MULIB_BUILDVM_LIMIT : MULIB_BUILDVM_INVALID;
          n = used;
        }
        d.data = buffer;
      }
      if (!rc) { d.length = n; value = mulib_artifact_create (&d); }
      free (buffer);
    }
    if (rc) break;
    if (!value) { rc = MULIB_BUILDVM_MEMORY; break; }
    mulib_artifact_release (regs[c->dst]); regs[c->dst] = value; charged += cost;
  }
  for (i = 0; i < p->registers; ++i) mulib_artifact_release (regs[i]);
  free (regs); return rc;
}
#endif
