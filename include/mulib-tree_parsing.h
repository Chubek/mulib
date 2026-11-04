/** @file mulib-tree_parsing.h
 * @brief Bounded first-order term parsing into the shared MuLIB AST model.
 * Define MULIB_IMPL_TREE_PARSING, MULIB_IMPL_AST and MULIB_IMPL_STRBUF.
 */
#ifndef MULIB_TREE_PARSING_H
#define MULIB_TREE_PARSING_H
#include "mulib-ast.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Parse symbol or symbol(term,...), with ASCII identifiers, whitespace and
 * optional ? prefix for leaf variables. Empty argument lists are accepted.
 * Full input must be consumed. Maximum depth 128; node_limit is a hard cap.
 * Result owns its nodes; release with mulib_ast_destroy. NULL for invalid/OOM/
 * limit; optional error_offset identifies the byte position (end on success).
 * AST production holds the symbol; text is empty. No strings, comments or infix.
 */
mulib_ast_t *mulib_tree_parse(const char *source, size_t node_limit, size_t *error_offset);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TREE_PARSING) && !defined(MULIB_TREE_PARSING_IMPLEMENTED)
#define MULIB_TREE_PARSING_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_tree_parser_ { const char *start, *p; size_t remaining; } mulib_tree_parser_;
static void mulib_tree_space_(mulib_tree_parser_ *p)
{ while (*p->p == ' ' || *p->p == '\t' || *p->p == '\r' || *p->p == '\n') ++p->p; }
static int mulib_tree_letter_(unsigned char c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static mulib_ast_t *mulib_tree_parse_(mulib_tree_parser_ *p, unsigned depth)
{
  const char *start;
  char *name;
  size_t length;
  int variable;
  mulib_ast_t *node;
  mulib_tree_space_(p);
  if (depth > 128 || !p->remaining) return NULL;
  start = p->p; variable = *p->p == '?';
  if (variable) ++p->p;
  if (!mulib_tree_letter_((unsigned char)*p->p)) return NULL;
  ++p->p;
  while (mulib_tree_letter_((unsigned char)*p->p) || (*p->p >= '0' && *p->p <= '9')) ++p->p;
  length = (size_t)(p->p - start);
  name = (char *)malloc(length + 1);
  if (!name) return NULL;
  memcpy(name, start, length); name[length] = 0;
  node = mulib_ast_create(name, "", 0); free(name);
  if (!node) return NULL;
  --p->remaining; node->offset = (size_t)(start - p->start); mulib_tree_space_(p);
  if (*p->p == '(') {
    if (variable) { mulib_ast_destroy(node); return NULL; }
    ++p->p; mulib_tree_space_(p);
    if (*p->p != ')') for (;;) {
      mulib_ast_t *child = mulib_tree_parse_(p, depth + 1);
      if (!child || !mulib_ast_append(node, child)) { mulib_ast_destroy(child); mulib_ast_destroy(node); return NULL; }
      mulib_tree_space_(p);
      if (*p->p != ',') break;
      ++p->p;
    }
    if (*p->p != ')') { mulib_ast_destroy(node); return NULL; }
    ++p->p;
  }
  return node;
}
mulib_ast_t *mulib_tree_parse(const char *s, size_t limit, size_t *error)
{
  mulib_tree_parser_ parser;
  mulib_ast_t *root;
  if (!s) { if (error) *error = 0; return NULL; }
  parser.start = parser.p = s; parser.remaining = limit;
  root = mulib_tree_parse_(&parser, 1); mulib_tree_space_(&parser);
  if (*parser.p) { mulib_ast_destroy(root); root = NULL; }
  if (error) *error = (size_t)(parser.p - s);
  return root;
}
#ifdef __cplusplus
}
#endif
#endif
