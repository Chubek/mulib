/** @file mulib-term_rewriting.h
 * @brief Bounded ordered first-order term rewriting on shared MuLIB ASTs.
 * Define MULIB_IMPL_TERM_REWRITING, MULIB_IMPL_AST and MULIB_IMPL_STRBUF.
 */
#ifndef MULIB_TERM_REWRITING_H
#define MULIB_TERM_REWRITING_H
#include "mulib-ast.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_term_rule { const mulib_ast_t *lhs, *rhs; } mulib_term_rule_t;
/** Leftmost outermost, rule-order rewriting. ?name leaf productions in patterns
 * bind subtrees; repeated names require structural equality. At most 32 distinct
 * variables per rule, RHS variables must occur on LHS. Productions and text match
 * exactly; action/offset metadata is ignored. Input/rules are borrowed proper AST
 * roots (depth<=128, <=65536 nodes each), as returned by mulib_tree_parse.
 * Return 1 normal form, 2 step limit, 0 invalid/OOM/resource limit. On 1/2, output
 * receives a new owned tree and optional steps; on 0 outputs remain unchanged.
 * Destroy output with mulib_ast_destroy. No termination/confluence guarantee.
 */
int mulib_term_rewrite(const mulib_ast_t *input, const mulib_term_rule_t *rules,
                       size_t rule_count, size_t step_limit, mulib_ast_t **output, size_t *steps);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TERM_REWRITING) && !defined(MULIB_TERM_REWRITING_IMPLEMENTED)
#define MULIB_TERM_REWRITING_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_term_bindings_ { const char *names[32]; const mulib_ast_t *values[32]; size_t n; } mulib_term_bindings_;
static int mulib_term_variable_(const mulib_ast_t *n)
{ return n->production[0] == '?' && n->production[1] && !n->child && !n->length; }
static int mulib_term_validate_(const mulib_ast_t *n, unsigned depth, size_t *budget, mulib_term_bindings_ *vars, int rhs)
{
  if (!n || depth > 128 || !*budget || !n->production || (!n->text && n->length)) return 0;
  --*budget;
  if (n->production[0] == '?') {
    size_t i;
    if (!mulib_term_variable_(n)) return 0;
    if (vars) {
      for (i = 0; i < vars->n && strcmp(vars->names[i], n->production); ++i) {}
      if (i == vars->n) { if (rhs || i == 32) return 0; vars->names[vars->n++] = n->production; }
    }
  }
  for (const mulib_ast_t *c = n->child; c; c = c->next)
    if (c->parent != n || !mulib_term_validate_(c, depth + 1, budget, vars, rhs)) return 0;
  return 1;
}
static int mulib_term_equal_(const mulib_ast_t *a, const mulib_ast_t *b)
{
  if (strcmp(a->production, b->production) || a->length != b->length
      || (a->length && memcmp(a->text, b->text, a->length))) return 0;
  a = a->child; b = b->child;
  while (a && b) { if (!mulib_term_equal_(a, b)) return 0; a = a->next; b = b->next; }
  return !a && !b;
}
static int mulib_term_match_(const mulib_ast_t *p, const mulib_ast_t *n, mulib_term_bindings_ *v)
{
  if (mulib_term_variable_(p)) {
    size_t i;
    for (i = 0; i < v->n && strcmp(v->names[i], p->production); ++i) {}
    if (i < v->n) return mulib_term_equal_(v->values[i], n);
    if (i == 32) return 0;
    v->names[i] = p->production; v->values[i] = n; ++v->n; return 1;
  }
  if (strcmp(p->production, n->production) || p->length != n->length
      || (p->length && memcmp(p->text, n->text, p->length))) return 0;
  p = p->child; n = n->child;
  while (p && n) { if (!mulib_term_match_(p, n, v)) return 0; p = p->next; n = n->next; }
  return !p && !n;
}
static mulib_ast_t *mulib_term_clone_(const mulib_ast_t *n, const mulib_term_bindings_ *v, unsigned depth, size_t *budget)
{
  mulib_ast_t *copy;
  if (depth > 128 || !*budget) return NULL;
  if (v && mulib_term_variable_(n)) {
    for (size_t i = 0; i < v->n; ++i) if (!strcmp(n->production, v->names[i]))
      return mulib_term_clone_(v->values[i], NULL, depth, budget);
    return NULL;
  }
  --*budget;
  copy = mulib_ast_create(n->production, n->text, n->length);
  if (!copy) return NULL;
  for (n = n->child; n; n = n->next) {
    mulib_ast_t *child = mulib_term_clone_(n, v, depth + 1, budget);
    if (!child || !mulib_ast_append(copy, child)) { mulib_ast_destroy(child); mulib_ast_destroy(copy); return NULL; }
  }
  return copy;
}
/* 0 no match, 1 replaced, 2 matching but limit exhausted, -1 allocation/limit. */
static int mulib_term_step_(mulib_ast_t **slot, const mulib_term_rule_t *rules, size_t count, int allowed, unsigned depth)
{
  mulib_ast_t *n = *slot;
  for (size_t i = 0; i < count; ++i) {
    mulib_term_bindings_ bindings = {{0}, {0}, 0};
    if (mulib_term_match_(rules[i].lhs, n, &bindings)) {
      mulib_ast_t *replacement;
      size_t budget = 65536;
      if (!allowed) return 2;
      replacement = mulib_term_clone_(rules[i].rhs, &bindings, depth, &budget);
      if (!replacement) return -1;
      replacement->parent = n->parent; replacement->next = n->next;
      if (n->parent && n->parent->tail == n) n->parent->tail = replacement;
      n->parent = NULL; n->next = NULL; *slot = replacement; mulib_ast_destroy(n); return 1;
    }
  }
  for (mulib_ast_t **child = &n->child; *child; child = &(*child)->next) {
    int result = mulib_term_step_(child, rules, count, allowed, depth + 1);
    if (result) return result;
  }
  return 0;
}
int mulib_term_rewrite(const mulib_ast_t *input, const mulib_term_rule_t *rules,
                       size_t count, size_t limit, mulib_ast_t **out, size_t *steps)
{
  mulib_ast_t *root;
  size_t budget = 65536, done = 0;
  if (!input || !out || (!rules && count) || input->parent || input->next
      || !mulib_term_validate_(input, 1, &budget, NULL, 0)) return 0;
  for (size_t i = 0; i < count; ++i) {
    mulib_term_bindings_ vars = {{0}, {0}, 0};
    budget = 65536;
    if (!rules[i].lhs || !rules[i].rhs || rules[i].lhs->parent || rules[i].lhs->next
        || rules[i].rhs->parent || rules[i].rhs->next
        || !mulib_term_validate_(rules[i].lhs, 1, &budget, &vars, 0)) return 0;
    budget = 65536;
    if (!mulib_term_validate_(rules[i].rhs, 1, &budget, &vars, 1)) return 0;
  }
  budget = 65536; root = mulib_term_clone_(input, NULL, 1, &budget);
  if (!root) return 0;
  for (;;) {
    int status = mulib_term_step_(&root, rules, count, done < limit, 1);
    if (status < 0) { mulib_ast_destroy(root); return 0; }
    if (status != 1) { *out = root; if (steps) *steps = done; return status == 2 ? 2 : 1; }
    ++done;
    budget = 65536;
    if (!mulib_term_validate_(root, 1, &budget, NULL, 0)) { mulib_ast_destroy(root); return 0; }
  }
}
#ifdef __cplusplus
}
#endif
#endif
