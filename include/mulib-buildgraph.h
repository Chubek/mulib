/**
 * @file mulib-buildgraph.h
 * @brief Declared-input/output build DAG with execution and translation walkers.
 * Define MULIB_IMPL_BUILDGRAPH and MULIB_IMPL_ARTIFACT once. Actions are borrowed
 * opaque application objects. Hermeticity is a capability contract: executors
 * must use only declared inputs; OS sandboxing belongs to the execution backend.
 */
#ifndef MULIB_BUILDGRAPH_H
#define MULIB_BUILDGRAPH_H
#include "mulib-artifact.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct Action;
typedef struct mulib_buildgraph mulib_buildgraph_t;
typedef struct mulib_buildgraph_run mulib_buildgraph_run_t;
typedef struct mulib_buildgraph_step mulib_buildgraph_step_t;
typedef enum mulib_buildgraph_status {
  MULIB_BUILDGRAPH_OK = 0, MULIB_BUILDGRAPH_INVALID = -1,
  MULIB_BUILDGRAPH_MEMORY = -2, MULIB_BUILDGRAPH_FULL = -3,
  MULIB_BUILDGRAPH_CYCLE = -4, MULIB_BUILDGRAPH_FAILED = -5,
  MULIB_BUILDGRAPH_MISSING = -6, MULIB_BUILDGRAPH_BUSY = -7
} mulib_buildgraph_status_t;
typedef enum mulib_buildgraph_kind {
  MULIB_BUILDGRAPH_ARTIFACT, MULIB_BUILDGRAPH_ACTION
} mulib_buildgraph_kind_t;
typedef enum mulib_buildgraph_state {
  MULIB_BUILDGRAPH_PENDING, MULIB_BUILDGRAPH_SUCCEEDED,
  MULIB_BUILDGRAPH_ERRORED, MULIB_BUILDGRAPH_BLOCKED
} mulib_buildgraph_state_t;
typedef struct mulib_buildgraph_node {
  const char *name;
  mulib_buildgraph_kind_t kind;
  const struct Action *action;
  const mulib_artifact_t *source; /**< NULL for generated artifacts. */
} mulib_buildgraph_node_t;
typedef struct mulib_buildgraph_binding {
  size_t node;
  const char *name;
  const mulib_artifact_t *artifact;
} mulib_buildgraph_binding_t;
/** Execute one action; return zero on success. All output slots must be set. */
typedef int (*mulib_buildgraph_execute_fn) (void *, const struct Action *,
                                            mulib_buildgraph_step_t *);
/** Visit nodes in dependency order, then each incoming edge, without execution.
 * Nonzero callbacks stop translation and are returned unchanged. */
typedef struct mulib_buildgraph_translator {
  int (*node) (void *, size_t, const mulib_buildgraph_node_t *);
  int (*edge) (void *, size_t before, size_t after);
} mulib_buildgraph_translator_t;
/** Allocate bounded storage. Zero capacities permit an empty graph. */
mulib_buildgraph_t *mulib_buildgraph_create (size_t nodes, size_t edges);
/** Release names and retained sources; never destroy Actions. */
void mulib_buildgraph_destroy (mulib_buildgraph_t *graph);
/** Add a retained source or a NULL generated placeholder. Names are unique
 * canonical relative slash paths, at most 4095 bytes. out changes on success. */
int mulib_buildgraph_artifact (mulib_buildgraph_t *, const char *name,
                              mulib_artifact_t *source, size_t *out);
/** Borrow an opaque, non-NULL Action until graph destruction. */
int mulib_buildgraph_action (mulib_buildgraph_t *, const char *name,
                            const struct Action *, size_t *out);
/** Add artifact->action input or action->artifact output. Single producer,
 * no producer for a source, duplicate edges are idempotent. */
int mulib_buildgraph_depend (mulib_buildgraph_t *, size_t before, size_t after);
/** Borrow a node view. IDs remain stable for the graph lifetime. */
const mulib_buildgraph_node_t *mulib_buildgraph_node (const mulib_buildgraph_t *, size_t);
/** Number of registered nodes; zero for NULL. */
size_t mulib_buildgraph_count (const mulib_buildgraph_t *);
/** Validate all producers and cycles before any walker callbacks. */
int mulib_buildgraph_validate (const mulib_buildgraph_t *);
/** Walk target dependency closure; NULL/zero targets selects the entire graph.
 * Mutations and nested walks are rejected while either walker is active. */
int mulib_buildgraph_translate (mulib_buildgraph_t *, const size_t *targets,
    size_t count, const mulib_buildgraph_translator_t *, void *user);
/** Run serially in deterministic dependency order. Independent branches continue
 * after failure; descendants are blocked. *out must be NULL. A report is returned
 * on OK/FAILED, and owns all successful outputs independently of the graph. */
int mulib_buildgraph_execute (mulib_buildgraph_t *, const size_t *targets,
    size_t count, mulib_buildgraph_execute_fn, void *user, mulib_buildgraph_run_t **out);
/** Borrow declared inputs/outputs for the callback only, in edge insertion order. */
const mulib_buildgraph_binding_t *mulib_buildgraph_inputs (const mulib_buildgraph_step_t *, size_t *count);
const mulib_buildgraph_binding_t *mulib_buildgraph_outputs (const mulib_buildgraph_step_t *, size_t *count);
/** Retain/replace a staged output by slot. All staged values roll back on failure. */
int mulib_buildgraph_output (mulib_buildgraph_step_t *, size_t slot, mulib_artifact_t *);
/** Query report state and original callback code (MISSING for omitted outputs). */
int mulib_buildgraph_result (const mulib_buildgraph_run_t *, size_t,
                            mulib_buildgraph_state_t *, int *callback_code);
/** Borrow a successful source/output; NULL for failed, unselected or action nodes. */
const mulib_artifact_t *mulib_buildgraph_value (const mulib_buildgraph_run_t *, size_t);
/** Release report and retained artifacts; NULL safe. */
void mulib_buildgraph_run_destroy (mulib_buildgraph_run_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_BUILDGRAPH) && !defined(MULIB_BUILDGRAPH_IMPLEMENTED)
#define MULIB_BUILDGRAPH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
typedef struct mulib_buildgraph_edge_ { size_t before, after; } mulib_buildgraph_edge_;
struct mulib_buildgraph {
  mulib_buildgraph_node_t *nodes;
  mulib_buildgraph_edge_ *edges;
  size_t count, capacity, edge_count, edge_capacity;
  int walking;
};
typedef struct mulib_buildgraph_record_ {
  mulib_buildgraph_state_t state;
  int code;
  mulib_artifact_t *value;
} mulib_buildgraph_record_;
struct mulib_buildgraph_run { size_t count; mulib_buildgraph_record_ *records; };
struct mulib_buildgraph_step {
  mulib_buildgraph_binding_t *inputs, *outputs;
  size_t input_count, output_count;
};
mulib_buildgraph_t *
mulib_buildgraph_create (size_t n, size_t e)
{
  mulib_buildgraph_t *g;
  if (n > SIZE_MAX / sizeof (*g->nodes) || e > SIZE_MAX / sizeof (*g->edges)) return NULL;
  g = (mulib_buildgraph_t *)calloc (1, sizeof (*g));
  if (!g) return NULL;
  g->nodes = n ? (mulib_buildgraph_node_t *)calloc (n, sizeof (*g->nodes)) : NULL;
  g->edges = e ? (mulib_buildgraph_edge_ *)malloc (e * sizeof (*g->edges)) : NULL;
  if ((n && !g->nodes) || (e && !g->edges)) { mulib_buildgraph_destroy (g); return NULL; }
  g->capacity = n; g->edge_capacity = e;
  return g;
}
void
mulib_buildgraph_destroy (mulib_buildgraph_t *g)
{
  size_t i;
  if (!g || g->walking) return;
  for (i = 0; i < g->count; ++i) {
    free ((void *)g->nodes[i].name);
    mulib_artifact_release ((mulib_artifact_t *)g->nodes[i].source);
  }
  free (g->nodes); free (g->edges); free (g);
}
static int
mulib_buildgraph_name_ (const char *s)
{
  size_t i = 0, start = 0;
  if (!s || !*s) return 0;
  for (;;) {
    unsigned char c = (unsigned char)s[i];
    if (i > 4095 || (c && (c < 32 || c == 127 || c == '\\' || c == ':'))) return 0;
    if (!c || c == '/') {
      size_t n = i - start;
      if (!n || (n == 1 && s[start] == '.') || (n == 2 && s[start] == '.' && s[start+1] == '.')) return 0;
      if (!c) return 1;
      start = i + 1;
    }
    ++i;
  }
}
static int
mulib_buildgraph_add_ (mulib_buildgraph_t *g, const char *name,
    const struct Action *action, mulib_artifact_t *source, size_t *out)
{
  size_t i, n;
  char *copy;
  if (!g || !out || !mulib_buildgraph_name_ (name)) return MULIB_BUILDGRAPH_INVALID;
  if (g->walking) return MULIB_BUILDGRAPH_BUSY;
  for (i = 0; i < g->count; ++i)
    if (!strcmp (g->nodes[i].name, name)) return MULIB_BUILDGRAPH_INVALID;
  if (g->count == g->capacity) return MULIB_BUILDGRAPH_FULL;
  n = strlen (name) + 1; copy = (char *)malloc (n);
  if (!copy) return MULIB_BUILDGRAPH_MEMORY;
  if (source && !mulib_artifact_retain (source)) { free (copy); return MULIB_BUILDGRAPH_MEMORY; }
  memcpy (copy, name, n);
  g->nodes[g->count].name = copy;
  g->nodes[g->count].kind = action ? MULIB_BUILDGRAPH_ACTION : MULIB_BUILDGRAPH_ARTIFACT;
  g->nodes[g->count].action = action; g->nodes[g->count].source = source;
  *out = g->count++;
  return 0;
}
int mulib_buildgraph_artifact (mulib_buildgraph_t *g, const char *n, mulib_artifact_t *a, size_t *id)
{ return mulib_buildgraph_add_ (g, n, NULL, a, id); }
int mulib_buildgraph_action (mulib_buildgraph_t *g, const char *n, const struct Action *a, size_t *id)
{ return a ? mulib_buildgraph_add_ (g, n, a, NULL, id) : MULIB_BUILDGRAPH_INVALID; }
int
mulib_buildgraph_depend (mulib_buildgraph_t *g, size_t a, size_t b)
{
  size_t i;
  if (!g || a >= g->count || b >= g->count || g->nodes[a].kind == g->nodes[b].kind)
    return MULIB_BUILDGRAPH_INVALID;
  if (g->walking) return MULIB_BUILDGRAPH_BUSY;
  if (g->nodes[b].source) return MULIB_BUILDGRAPH_INVALID;
  for (i = 0; i < g->edge_count; ++i) {
    if (g->edges[i].before == a && g->edges[i].after == b) return 0;
    if (g->nodes[b].kind == MULIB_BUILDGRAPH_ARTIFACT && g->edges[i].after == b)
      return MULIB_BUILDGRAPH_INVALID;
  }
  if (g->edge_count == g->edge_capacity) return MULIB_BUILDGRAPH_FULL;
  g->edges[g->edge_count].before = a; g->edges[g->edge_count++].after = b;
  return 0;
}
const mulib_buildgraph_node_t *mulib_buildgraph_node (const mulib_buildgraph_t *g, size_t id)
{ return g && id < g->count ? g->nodes + id : NULL; }
size_t mulib_buildgraph_count (const mulib_buildgraph_t *g) { return g ? g->count : 0; }
static int
mulib_buildgraph_order_ (const mulib_buildgraph_t *g, size_t **out)
{
  size_t *order, *degree, i, head = 0, tail = 0;
  if (!g) return MULIB_BUILDGRAPH_INVALID;
  *out = NULL;
  if (!g->count) return 0;
  order = (size_t *)malloc (g->count * sizeof (*order));
  degree = (size_t *)calloc (g->count, sizeof (*degree));
  if (!order || !degree) { free (order); free (degree); return MULIB_BUILDGRAPH_MEMORY; }
  for (i = 0; i < g->edge_count; ++i) ++degree[g->edges[i].after];
  for (i = 0; i < g->count; ++i) {
    if (g->nodes[i].kind == MULIB_BUILDGRAPH_ARTIFACT && !g->nodes[i].source && !degree[i]) {
      free (order); free (degree); return MULIB_BUILDGRAPH_MISSING;
    }
    if (!degree[i]) order[tail++] = i;
  }
  while (head < tail) {
    size_t id = order[head++];
    for (i = 0; i < g->edge_count; ++i)
      if (g->edges[i].before == id && !--degree[g->edges[i].after]) order[tail++] = g->edges[i].after;
  }
  free (degree);
  if (tail != g->count) { free (order); return MULIB_BUILDGRAPH_CYCLE; }
  *out = order;
  return 0;
}
int mulib_buildgraph_validate (const mulib_buildgraph_t *g)
{ size_t *order; int rc = mulib_buildgraph_order_ (g, &order); if (g) free (order); return rc; }
static int
mulib_buildgraph_select_ (mulib_buildgraph_t *g, const size_t *targets, size_t n,
                          size_t **order, unsigned char **selected)
{
  size_t i, e;
  int rc;
  if (!g || (!targets && n)) return MULIB_BUILDGRAPH_INVALID;
  if (g->walking) return MULIB_BUILDGRAPH_BUSY;
  for (i = 0; i < n; ++i) if (targets[i] >= g->count) return MULIB_BUILDGRAPH_INVALID;
  rc = mulib_buildgraph_order_ (g, order);
  if (rc) return rc;
  *selected = (unsigned char *)calloc (g->count ? g->count : 1, 1);
  if (!*selected) { free (*order); return MULIB_BUILDGRAPH_MEMORY; }
  if (!n) memset (*selected, 1, g->count);
  else {
    for (i = 0; i < n; ++i) (*selected)[targets[i]] = 1;
    for (i = g->count; i; --i) {
      size_t id = (*order)[i-1];
      if ((*selected)[id])
        for (e = 0; e < g->edge_count; ++e)
          if (g->edges[e].after == id) (*selected)[g->edges[e].before] = 1;
    }
    /* Selecting an action also produces every declared output. */
    for (e = 0; e < g->edge_count; ++e)
      if (g->nodes[g->edges[e].before].kind == MULIB_BUILDGRAPH_ACTION
          && (*selected)[g->edges[e].before]) (*selected)[g->edges[e].after] = 1;
  }
  return 0;
}
int
mulib_buildgraph_translate (mulib_buildgraph_t *g, const size_t *targets, size_t n,
    const mulib_buildgraph_translator_t *t, void *user)
{
  size_t *order, i, e;
  unsigned char *selected;
  int rc;
  if (!t || !t->node) return MULIB_BUILDGRAPH_INVALID;
  rc = mulib_buildgraph_select_ (g, targets, n, &order, &selected);
  if (rc) return rc;
  g->walking = 1;
  for (i = 0; i < g->count && !rc; ++i) {
    size_t id = order[i];
    if (!selected[id]) continue;
    rc = t->node (user, id, g->nodes + id);
    if (t->edge)
      for (e = 0; e < g->edge_count && !rc; ++e)
        if (g->edges[e].after == id) rc = t->edge (user, g->edges[e].before, id);
  }
  g->walking = 0; free (order); free (selected);
  return rc;
}
const mulib_buildgraph_binding_t *mulib_buildgraph_inputs (const mulib_buildgraph_step_t *s, size_t *n)
{ if (n) *n = s ? s->input_count : 0; return s ? s->inputs : NULL; }
const mulib_buildgraph_binding_t *mulib_buildgraph_outputs (const mulib_buildgraph_step_t *s, size_t *n)
{ if (n) *n = s ? s->output_count : 0; return s ? s->outputs : NULL; }
int
mulib_buildgraph_output (mulib_buildgraph_step_t *s, size_t slot, mulib_artifact_t *a)
{
  if (!s || slot >= s->output_count || !a) return MULIB_BUILDGRAPH_INVALID;
  if (!mulib_artifact_retain (a)) return MULIB_BUILDGRAPH_MEMORY;
  mulib_artifact_release ((mulib_artifact_t *)s->outputs[slot].artifact);
  s->outputs[slot].artifact = a;
  return 0;
}
int
mulib_buildgraph_execute (mulib_buildgraph_t *g, const size_t *targets, size_t n,
    mulib_buildgraph_execute_fn fn, void *user, mulib_buildgraph_run_t **out)
{
  size_t *order, i, e;
  unsigned char *selected;
  mulib_buildgraph_run_t *r;
  mulib_buildgraph_step_t s;
  int rc, overall = 0;
  if (!fn || !out || *out) return MULIB_BUILDGRAPH_INVALID;
  rc = mulib_buildgraph_select_ (g, targets, n, &order, &selected);
  if (rc) return rc;
  r = (mulib_buildgraph_run_t *)calloc (1, sizeof (*r));
  memset (&s, 0, sizeof (s));
  if (r) {
    r->count = g->count;
    r->records = (mulib_buildgraph_record_ *)calloc (g->count ? g->count : 1, sizeof (*r->records));
  }
  if (g->edge_count <= SIZE_MAX / sizeof (*s.inputs)) {
    s.inputs = (mulib_buildgraph_binding_t *)calloc (g->edge_count ? g->edge_count : 1, sizeof (*s.inputs));
    s.outputs = (mulib_buildgraph_binding_t *)calloc (g->edge_count ? g->edge_count : 1, sizeof (*s.outputs));
  }
  if (!r || !r->records || !s.inputs || !s.outputs) {
    mulib_buildgraph_run_destroy (r); free (s.inputs); free (s.outputs);
    free (order); free (selected); return MULIB_BUILDGRAPH_MEMORY;
  }
  g->walking = 1;
  for (i = 0; i < g->count; ++i) {
    size_t id = order[i];
    mulib_buildgraph_record_ *record = r->records + id;
    if (!selected[id]) continue;
    for (e = 0; e < g->edge_count; ++e)
      if (g->edges[e].after == id && r->records[g->edges[e].before].state != MULIB_BUILDGRAPH_SUCCEEDED)
        record->state = MULIB_BUILDGRAPH_BLOCKED;
    if (record->state == MULIB_BUILDGRAPH_BLOCKED) { overall = MULIB_BUILDGRAPH_FAILED; continue; }
    if (g->nodes[id].kind == MULIB_BUILDGRAPH_ARTIFACT) {
      if (g->nodes[id].source) record->value = mulib_artifact_retain ((mulib_artifact_t *)g->nodes[id].source);
      record->state = record->value ? MULIB_BUILDGRAPH_SUCCEEDED : MULIB_BUILDGRAPH_ERRORED;
      if (!record->value) { record->code = MULIB_BUILDGRAPH_MEMORY; overall = MULIB_BUILDGRAPH_FAILED; }
      continue;
    }
    s.input_count = s.output_count = 0;
    for (e = 0; e < g->edge_count; ++e) {
      size_t node;
      mulib_buildgraph_binding_t *b;
      if (g->edges[e].after == id) {
        node = g->edges[e].before; b = s.inputs + s.input_count++;
        b->artifact = r->records[node].value;
      } else if (g->edges[e].before == id) {
        node = g->edges[e].after; b = s.outputs + s.output_count++; b->artifact = NULL;
      } else continue;
      b->node = node; b->name = g->nodes[node].name;
    }
    record->code = fn (user, g->nodes[id].action, &s);
    if (!record->code)
      for (e = 0; e < s.output_count; ++e)
        if (!s.outputs[e].artifact) { record->code = MULIB_BUILDGRAPH_MISSING; break; }
    record->state = record->code ? MULIB_BUILDGRAPH_ERRORED : MULIB_BUILDGRAPH_SUCCEEDED;
    if (record->code) overall = MULIB_BUILDGRAPH_FAILED;
    for (e = 0; e < s.output_count; ++e) {
      mulib_artifact_t *a = (mulib_artifact_t *)s.outputs[e].artifact;
      if (record->code) mulib_artifact_release (a);
      else r->records[s.outputs[e].node].value = a;
    }
  }
  g->walking = 0; free (order); free (selected); free (s.inputs); free (s.outputs);
  *out = r;
  return overall;
}
int
mulib_buildgraph_result (const mulib_buildgraph_run_t *r, size_t id, mulib_buildgraph_state_t *s, int *code)
{
  if (!r || id >= r->count || (!s && !code)) return MULIB_BUILDGRAPH_INVALID;
  if (s) *s = r->records[id].state;
  if (code) *code = r->records[id].code;
  return 0;
}
const mulib_artifact_t *mulib_buildgraph_value (const mulib_buildgraph_run_t *r, size_t id)
{ return r && id < r->count ? r->records[id].value : NULL; }
void
mulib_buildgraph_run_destroy (mulib_buildgraph_run_t *r)
{
  size_t i;
  if (!r) return;
  if (r->records) for (i = 0; i < r->count; ++i) mulib_artifact_release (r->records[i].value);
  free (r->records); free (r);
}
#endif
