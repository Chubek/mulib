/** @file mulib-cassowary-solver.h
 * @brief Cassowary slack/error tableau simplex for prioritized linear constraints.
 * Define MULIB_IMPL_CASSOWARY_SOLVER; link libm. Mutations are transactional.
 * Tables are rebuilt on changes; this is not the unrelated nonlinear geosolver.
 */
#ifndef MULIB_CASSOWARY_SOLVER_H
#define MULIB_CASSOWARY_SOLVER_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_CASSOWARY_REQUIRED 0.0
#define MULIB_CASSOWARY_WEAK 1.0
#define MULIB_CASSOWARY_MEDIUM 1000.0
#define MULIB_CASSOWARY_STRONG 1000000.0
#define MULIB_CASSOWARY_NONE ((size_t)-1)
typedef struct mulib_cassowary_solver mulib_cassowary_solver_t;
typedef struct mulib_cassowary_term { size_t variable; double coefficient; } mulib_cassowary_term_t;
typedef enum mulib_cassowary_relation { MULIB_CASSOWARY_EQ, MULIB_CASSOWARY_LE, MULIB_CASSOWARY_GE } mulib_cassowary_relation_t;
typedef enum mulib_cassowary_status {
  MULIB_CASSOWARY_OK, MULIB_CASSOWARY_INVALID = -1, MULIB_CASSOWARY_MEMORY = -2,
  MULIB_CASSOWARY_UNSATISFIABLE = -3, MULIB_CASSOWARY_LIMIT = -4, MULIB_CASSOWARY_NUMERIC = -5
} mulib_cassowary_status_t;
typedef struct mulib_cassowary_options {
  size_t max_variables, max_constraints, max_symbols, max_work;
  double epsilon;
} mulib_cassowary_options_t;
/** Zero options select 128 variables, 512 constraints, 2048 symbols,
 * 10000000 tableau work units and absolute epsilon 1e-8. */
mulib_cassowary_solver_t *mulib_cassowary_create(const mulib_cassowary_options_t *);
void mulib_cassowary_destroy(mulib_cassowary_solver_t *);
int mulib_cassowary_add_variable(mulib_cassowary_solver_t *, size_t *out_id);
/** sum(coefficient*x) relation rhs. Required strength=0 is hard; finite positive
 * strengths minimize weighted L1 error. Numeric strong/medium/weak weights are
 * not exact lexicographic levels. IDs stable and not reused. */
int mulib_cassowary_add(mulib_cassowary_solver_t *, const mulib_cassowary_term_t *, size_t,
    mulib_cassowary_relation_t, double rhs, double strength, size_t *out_constraint);
int mulib_cassowary_remove(mulib_cassowary_solver_t *, size_t constraint);
/** One soft edit equality per variable. Suggestions solve immediately. */
int mulib_cassowary_add_edit(mulib_cassowary_solver_t *, size_t variable, double strength);
int mulib_cassowary_suggest(mulib_cassowary_solver_t *, size_t variable, double value);
int mulib_cassowary_remove_edit(mulib_cassowary_solver_t *, size_t variable);
/** Rebuild feasible tableau using artificial variables, then primal optimize.
 * All failed operations preserve constraints, edit targets and published values. */
int mulib_cassowary_solve(mulib_cassowary_solver_t *);
int mulib_cassowary_value(const mulib_cassowary_solver_t *, size_t variable, double *out);
size_t mulib_cassowary_variable_count(const mulib_cassowary_solver_t *);
size_t mulib_cassowary_constraint_count(const mulib_cassowary_solver_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CASSOWARY_SOLVER) && !defined(MULIB_CASSOWARY_SOLVER_IMPLEMENTED)
#define MULIB_CASSOWARY_SOLVER_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_cassowary_constraint_ {
  mulib_cassowary_term_t *terms; size_t count; mulib_cassowary_relation_t relation;
  double rhs, strength; int active;
} mulib_cassowary_constraint_;
struct mulib_cassowary_solver {
  mulib_cassowary_options_t options; double *values; size_t *edits;
  size_t variables, count, active, capacity; mulib_cassowary_constraint_ *constraints;
};
typedef struct mulib_cassowary_row_ { double constant, *cells; } mulib_cassowary_row_;
enum { MULIB_CASS_EXT_, MULIB_CASS_SLACK_, MULIB_CASS_ERROR_, MULIB_CASS_DUMMY_ };
typedef struct mulib_cassowary_table_ {
  mulib_cassowary_options_t o; size_t count, work; unsigned char *types;
  mulib_cassowary_row_ **rows, *objective, *artificial; int error;
} mulib_cassowary_table_;
static int mulib_cassowary_tick_(mulib_cassowary_table_ *t, size_t n) {
  if (n > t->o.max_work - t->work) { t->error = MULIB_CASSOWARY_LIMIT; return 0; }
  t->work += n; return 1;
}
static mulib_cassowary_row_ *mulib_cassowary_row_new_(mulib_cassowary_table_ *t) {
  mulib_cassowary_row_ *r = (mulib_cassowary_row_ *)calloc(1, sizeof *r);
  if (r) r->cells = (double *)calloc(t->o.max_symbols, sizeof *r->cells);
  if (!r || !r->cells) { free(r); t->error = MULIB_CASSOWARY_MEMORY; return NULL; } return r;
}
static void mulib_cassowary_row_free_(mulib_cassowary_row_ *r) { if (r) { free(r->cells); free(r); } }
static mulib_cassowary_row_ *mulib_cassowary_row_copy_(mulib_cassowary_table_ *t, const mulib_cassowary_row_ *r) {
  mulib_cassowary_row_ *p = mulib_cassowary_row_new_(t);
  if (p) { p->constant = r->constant; memcpy(p->cells, r->cells, t->count * sizeof *p->cells); } return p;
}
static size_t mulib_cassowary_symbol_(mulib_cassowary_table_ *t, unsigned char type) {
  if (t->count == t->o.max_symbols) { t->error = MULIB_CASSOWARY_LIMIT; return MULIB_CASSOWARY_NONE; }
  t->types[t->count] = type; return t->count++;
}
static int mulib_cassowary_add_row_(mulib_cassowary_table_ *t, mulib_cassowary_row_ *a, const mulib_cassowary_row_ *b, double factor) {
  size_t i;
  if (!mulib_cassowary_tick_(t, t->count + 1)) return 0;
  if (!isfinite(a->constant += b->constant * factor)) goto numeric;
  for (i = 0; i < t->count; ++i) {
    if (!isfinite(a->cells[i] += b->cells[i] * factor)) goto numeric;
    if (fabs(a->cells[i]) < t->o.epsilon) a->cells[i] = 0;
  }
  return 1;
numeric:
  t->error = MULIB_CASSOWARY_NUMERIC; return 0;
}
static int mulib_cassowary_scale_(mulib_cassowary_table_ *t, mulib_cassowary_row_ *r, double f) {
  size_t i;
  if (!mulib_cassowary_tick_(t, t->count + 1)) return 0;
  if (!isfinite(r->constant *= f)) goto numeric;
  for (i = 0; i < t->count; ++i) if (!isfinite(r->cells[i] *= f)) goto numeric;
  return 1;
numeric:
  t->error = MULIB_CASSOWARY_NUMERIC; return 0;
}
static int mulib_cassowary_subject_(mulib_cassowary_table_ *t, mulib_cassowary_row_ *r, size_t symbol) {
  double f = -1 / r->cells[symbol]; r->cells[symbol] = 0;
  return mulib_cassowary_scale_(t, r, f);
}
static int mulib_cassowary_substitute_(mulib_cassowary_table_ *t, size_t symbol, const mulib_cassowary_row_ *r) {
  size_t i; double factor;
  for (i = 0; i < t->count; ++i) if (t->rows[i] && (factor = t->rows[i]->cells[symbol]) != 0) {
    t->rows[i]->cells[symbol] = 0; if (!mulib_cassowary_add_row_(t, t->rows[i], r, factor)) return 0;
  }
  factor = t->objective->cells[symbol]; t->objective->cells[symbol] = 0;
  if (factor && !mulib_cassowary_add_row_(t, t->objective, r, factor)) return 0;
  if (t->artificial) {
    factor = t->artificial->cells[symbol]; t->artificial->cells[symbol] = 0;
    if (factor && !mulib_cassowary_add_row_(t, t->artificial, r, factor)) return 0;
  }
  return 1;
}
static int mulib_cassowary_pivot_(mulib_cassowary_table_ *t, size_t enter, size_t leave) {
  mulib_cassowary_row_ *r = t->rows[leave]; t->rows[leave] = NULL;
  r->cells[leave] = -1;
  if (!mulib_cassowary_subject_(t, r, enter) || !mulib_cassowary_substitute_(t, enter, r)) { mulib_cassowary_row_free_(r); return 0; }
  t->rows[enter] = r; return 1;
}
static int mulib_cassowary_optimize_(mulib_cassowary_table_ *t, mulib_cassowary_row_ *objective) {
  size_t enter, leave, i; double ratio, best;
  for (;;) {
    if (!mulib_cassowary_tick_(t, t->count)) return 0;
    for (enter = 0; enter < t->count; ++enter)
      if (t->types[enter] != MULIB_CASS_DUMMY_ && objective->cells[enter] < -t->o.epsilon) break;
    if (enter == t->count) return 1;
    leave = MULIB_CASSOWARY_NONE; best = INFINITY;
    for (i = 0; i < t->count; ++i) if (t->types[i] != MULIB_CASS_EXT_ && t->rows[i] && t->rows[i]->cells[enter] < -t->o.epsilon) {
      ratio = -t->rows[i]->constant / t->rows[i]->cells[enter];
      if (ratio < best) { best = ratio; leave = i; }
    }
    if (leave == MULIB_CASSOWARY_NONE) { t->error = MULIB_CASSOWARY_NUMERIC; return 0; }
    if (!mulib_cassowary_pivot_(t, enter, leave)) return 0;
  }
}
static int mulib_cassowary_artificial_(mulib_cassowary_table_ *t, mulib_cassowary_row_ *r) {
  size_t symbol = mulib_cassowary_symbol_(t, MULIB_CASS_SLACK_), i, enter;
  if (symbol == MULIB_CASSOWARY_NONE) { mulib_cassowary_row_free_(r); return 0; }
  t->rows[symbol] = r; t->artificial = mulib_cassowary_row_copy_(t, r);
  if (!t->artificial || !mulib_cassowary_optimize_(t, t->artificial)) return 0;
  if (fabs(t->artificial->constant) > t->o.epsilon) { t->error = MULIB_CASSOWARY_UNSATISFIABLE; return 0; }
  mulib_cassowary_row_free_(t->artificial); t->artificial = NULL;
  if (t->rows[symbol]) {
    for (enter = 0; enter < t->count; ++enter) if (t->types[enter] != MULIB_CASS_DUMMY_ && fabs(t->rows[symbol]->cells[enter]) > t->o.epsilon) break;
    if (enter == t->count) { mulib_cassowary_row_free_(t->rows[symbol]); t->rows[symbol] = NULL; }
    else if (!mulib_cassowary_pivot_(t, enter, symbol)) return 0;
  }
  for (i = 0; i < t->count; ++i) if (t->rows[i]) t->rows[i]->cells[symbol] = 0;
  t->objective->cells[symbol] = 0; return 1;
}
static int mulib_cassowary_install_(mulib_cassowary_table_ *t, const mulib_cassowary_constraint_ *c) {
  mulib_cassowary_row_ *r = mulib_cassowary_row_new_(t);
  size_t i, marker, other = MULIB_CASSOWARY_NONE, subject;
  int dummy = 1;
  if (!r) return 0;
  r->constant = -c->rhs;
  for (i = 0; i < c->count; ++i) {
    size_t v = c->terms[i].variable; double coefficient = c->terms[i].coefficient;
    if (t->rows[v]) { if (!mulib_cassowary_add_row_(t, r, t->rows[v], coefficient)) goto fail; }
    else if (!isfinite(r->cells[v] += coefficient)) { t->error = MULIB_CASSOWARY_NUMERIC; goto fail; }
  }
  marker = mulib_cassowary_symbol_(t, c->relation == MULIB_CASSOWARY_EQ ?
      (c->strength ? MULIB_CASS_ERROR_ : MULIB_CASS_DUMMY_) : MULIB_CASS_SLACK_);
  if (marker == MULIB_CASSOWARY_NONE) goto fail;
  if (c->relation == MULIB_CASSOWARY_EQ) {
    r->cells[marker] = c->strength ? -1 : 1;
    if (c->strength) {
      other = mulib_cassowary_symbol_(t, MULIB_CASS_ERROR_); if (other == MULIB_CASSOWARY_NONE) goto fail;
      r->cells[other] = 1; t->objective->cells[marker] = c->strength; t->objective->cells[other] = c->strength;
    }
  } else {
    r->cells[marker] = c->relation == MULIB_CASSOWARY_LE ? 1 : -1;
    if (c->strength) {
      other = mulib_cassowary_symbol_(t, MULIB_CASS_ERROR_); if (other == MULIB_CASSOWARY_NONE) goto fail;
      r->cells[other] = -r->cells[marker]; t->objective->cells[other] = c->strength;
    }
  }
  if (r->constant < 0 && !mulib_cassowary_scale_(t, r, -1)) goto fail;
  subject = MULIB_CASSOWARY_NONE;
  for (i = 0; i < t->count; ++i) if (fabs(r->cells[i]) > t->o.epsilon) {
    if (t->types[i] == MULIB_CASS_EXT_) { subject = i; break; }
    if (t->types[i] != MULIB_CASS_DUMMY_) dummy = 0;
  }
  if (subject == MULIB_CASSOWARY_NONE && t->types[marker] != MULIB_CASS_DUMMY_ && r->cells[marker] < -t->o.epsilon) subject = marker;
  if (subject == MULIB_CASSOWARY_NONE && other != MULIB_CASSOWARY_NONE && r->cells[other] < -t->o.epsilon) subject = other;
  if (subject == MULIB_CASSOWARY_NONE && dummy) {
    if (fabs(r->constant) > t->o.epsilon) { t->error = MULIB_CASSOWARY_UNSATISFIABLE; goto fail; }
    subject = marker;
  }
  if (subject == MULIB_CASSOWARY_NONE) return mulib_cassowary_artificial_(t, r);
  if (!mulib_cassowary_subject_(t, r, subject) || !mulib_cassowary_substitute_(t, subject, r)) goto fail;
  t->rows[subject] = r; return 1;
fail:
  mulib_cassowary_row_free_(r); return 0;
}
mulib_cassowary_solver_t *mulib_cassowary_create(const mulib_cassowary_options_t *in) {
  mulib_cassowary_options_t o = {0}; mulib_cassowary_solver_t *s; size_t i;
  if (in) o = *in;
  if (!o.max_variables) o.max_variables = 128;
  if (!o.max_constraints) o.max_constraints = 512;
  if (!o.max_symbols) o.max_symbols = 2048;
  if (!o.max_work) o.max_work = 10000000;
  if (!o.epsilon) o.epsilon = 1e-8;
  if (o.max_variables > 1024 || o.max_constraints > 4096 || o.max_symbols > 8192 ||
      o.max_symbols < o.max_variables || !isfinite(o.epsilon) || o.epsilon <= 0 || o.epsilon >= 1) return NULL;
  s = (mulib_cassowary_solver_t *)calloc(1, sizeof *s); if (!s) return NULL;
  s->options = o; s->values = (double *)calloc(o.max_variables, sizeof *s->values);
  s->edits = (size_t *)malloc(o.max_variables * sizeof *s->edits);
  if (!s->values || !s->edits) { mulib_cassowary_destroy(s); return NULL; }
  for (i = 0; i < o.max_variables; ++i) s->edits[i] = MULIB_CASSOWARY_NONE;
  return s;
}
void mulib_cassowary_destroy(mulib_cassowary_solver_t *s) {
  size_t i; if (!s) return;
  for (i = 0; i < s->count; ++i) free(s->constraints[i].terms);
  free(s->constraints); free(s->values); free(s->edits); free(s);
}
int mulib_cassowary_add_variable(mulib_cassowary_solver_t *s, size_t *out) {
  if (!s || !out) return MULIB_CASSOWARY_INVALID;
  if (s->variables == s->options.max_variables) return MULIB_CASSOWARY_LIMIT;
  *out = s->variables++; return MULIB_CASSOWARY_OK;
}
int mulib_cassowary_solve(mulib_cassowary_solver_t *s) {
  mulib_cassowary_table_ t; size_t i; int rc;
  if (!s) return MULIB_CASSOWARY_INVALID;
  memset(&t, 0, sizeof t); t.o = s->options; t.count = s->variables;
  t.types = (unsigned char *)calloc(t.o.max_symbols, 1);
  t.rows = (mulib_cassowary_row_ **)calloc(t.o.max_symbols, sizeof *t.rows); t.objective = mulib_cassowary_row_new_(&t);
  if (!t.types || !t.rows || !t.objective) { t.error = MULIB_CASSOWARY_MEMORY; goto done; }
  for (i = 0; i < s->count; ++i) if (s->constraints[i].active && !mulib_cassowary_install_(&t, &s->constraints[i])) goto done;
  if (!mulib_cassowary_optimize_(&t, t.objective)) goto done;
  /* Independently check hard constraints before publishing numerical results. */
  for (i = 0; i < s->count; ++i) if (s->constraints[i].active && !s->constraints[i].strength) {
    const mulib_cassowary_constraint_ *c = &s->constraints[i]; size_t j;
    double residual = -c->rhs, scale = 1 + fabs(c->rhs), tolerance;
    if (!mulib_cassowary_tick_(&t, c->count + 1)) goto done;
    for (j = 0; j < c->count; ++j) {
      size_t v = c->terms[j].variable;
      double term = c->terms[j].coefficient * (t.rows[v] ? t.rows[v]->constant : 0);
      residual += term; scale += fabs(term);
    }
    tolerance = t.o.epsilon * scale;
    if (!isfinite(residual) || !isfinite(scale) ||
        (c->relation == MULIB_CASSOWARY_EQ && fabs(residual) > tolerance) ||
        (c->relation == MULIB_CASSOWARY_LE && residual > tolerance) ||
        (c->relation == MULIB_CASSOWARY_GE && residual < -tolerance)) {
      t.error = MULIB_CASSOWARY_NUMERIC; goto done;
    }
  }
  for (i = 0; i < s->variables; ++i) s->values[i] = t.rows[i] ? t.rows[i]->constant : 0;
done:
  rc = t.error;
  if (t.rows) for (i = 0; i < t.count; ++i) mulib_cassowary_row_free_(t.rows[i]);
  mulib_cassowary_row_free_(t.artificial); mulib_cassowary_row_free_(t.objective); free(t.rows); free(t.types); return rc;
}
int mulib_cassowary_add(mulib_cassowary_solver_t *s, const mulib_cassowary_term_t *terms, size_t n,
    mulib_cassowary_relation_t relation, double rhs, double strength, size_t *out) {
  mulib_cassowary_constraint_ c, *p; size_t i; int rc;
  if (!s || !out || (!terms && n) || n > 4096 || relation < MULIB_CASSOWARY_EQ || relation > MULIB_CASSOWARY_GE ||
      !isfinite(rhs) || !isfinite(strength) || strength < 0) return MULIB_CASSOWARY_INVALID;
  if (s->count == s->options.max_constraints) return MULIB_CASSOWARY_LIMIT;
  for (i = 0; i < n; ++i) if (terms[i].variable >= s->variables || !isfinite(terms[i].coefficient)) return MULIB_CASSOWARY_INVALID;
  memset(&c, 0, sizeof c); c.count = n; c.relation = relation; c.rhs = rhs; c.strength = strength; c.active = 1;
  c.terms = (mulib_cassowary_term_t *)malloc((n ? n : 1) * sizeof *terms); if (!c.terms) return MULIB_CASSOWARY_MEMORY;
  if (n) memcpy(c.terms, terms, n * sizeof *terms);
  if (s->count == s->capacity) {
    size_t capacity = s->capacity ? s->capacity * 2 : 8;
    if (capacity > s->options.max_constraints) capacity = s->options.max_constraints;
    p = (mulib_cassowary_constraint_ *)realloc(s->constraints, capacity * sizeof *p);
    if (!p) { free(c.terms); return MULIB_CASSOWARY_MEMORY; } s->constraints = p; s->capacity = capacity;
  }
  s->constraints[s->count++] = c; rc = mulib_cassowary_solve(s);
  if (rc) { --s->count; free(c.terms); return rc; }
  ++s->active; *out = s->count - 1; return MULIB_CASSOWARY_OK;
}
int mulib_cassowary_remove(mulib_cassowary_solver_t *s, size_t id) {
  size_t i; int rc;
  if (!s || id >= s->count || !s->constraints[id].active) return MULIB_CASSOWARY_INVALID;
  s->constraints[id].active = 0; rc = mulib_cassowary_solve(s);
  if (rc) s->constraints[id].active = 1;
  else { --s->active; for (i = 0; i < s->variables; ++i) if (s->edits[i] == id) s->edits[i] = MULIB_CASSOWARY_NONE; }
  return rc;
}
int mulib_cassowary_add_edit(mulib_cassowary_solver_t *s, size_t v, double strength) {
  mulib_cassowary_term_t term; size_t id; int rc;
  if (!s || v >= s->variables || s->edits[v] != MULIB_CASSOWARY_NONE || !isfinite(strength) || strength <= 0) return MULIB_CASSOWARY_INVALID;
  term.variable = v; term.coefficient = 1;
  rc = mulib_cassowary_add(s, &term, 1, MULIB_CASSOWARY_EQ, s->values[v], strength, &id);
  if (!rc) s->edits[v] = id;
  return rc;
}
int mulib_cassowary_suggest(mulib_cassowary_solver_t *s, size_t v, double value) {
  size_t id; double old; int rc;
  if (!s || v >= s->variables || !isfinite(value) || s->edits[v] == MULIB_CASSOWARY_NONE) return MULIB_CASSOWARY_INVALID;
  id = s->edits[v]; old = s->constraints[id].rhs; s->constraints[id].rhs = value; rc = mulib_cassowary_solve(s);
  if (rc) s->constraints[id].rhs = old;
  return rc;
}
int mulib_cassowary_remove_edit(mulib_cassowary_solver_t *s, size_t v) {
  if (!s || v >= s->variables || s->edits[v] == MULIB_CASSOWARY_NONE) return MULIB_CASSOWARY_INVALID;
  return mulib_cassowary_remove(s, s->edits[v]);
}
int mulib_cassowary_value(const mulib_cassowary_solver_t *s, size_t v, double *out) {
  if (!s || !out || v >= s->variables) return MULIB_CASSOWARY_INVALID;
  *out = s->values[v]; return MULIB_CASSOWARY_OK;
}
size_t mulib_cassowary_variable_count(const mulib_cassowary_solver_t *s) { return s ? s->variables : 0; }
size_t mulib_cassowary_constraint_count(const mulib_cassowary_solver_t *s) { return s ? s->active : 0; }
#ifdef __cplusplus
}
#endif
#endif
