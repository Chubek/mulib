/**
 * @file mulib-yantlr.h
 * @brief YANTLR host language, dependency linking and provider-driven generation.
 * Define MULIB_IMPL_YANTLR once, and explicitly implement ARENA, JSON, PARSEC,
 * PEG, STRBUF and UTF8. No standard-library services are installed implicitly.
 */
#ifndef MULIB_YANTLR_H
#define MULIB_YANTLR_H
#include "mulib-parsec.h"
#include "mulib-json.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_yantlr_module mulib_yantlr_module_t;
typedef struct mulib_yantlr_run mulib_yantlr_run_t;
typedef struct mulib_yantlr_value mulib_yantlr_value_t;
typedef enum mulib_yantlr_error_code {
  MULIB_YANTLR_OK, MULIB_YANTLR_INVALID, MULIB_YANTLR_IO,
  MULIB_YANTLR_SYNTAX, MULIB_YANTLR_MEMORY, MULIB_YANTLR_LIMIT,
  MULIB_YANTLR_DUPLICATE, MULIB_YANTLR_NAME, MULIB_YANTLR_SERVICE,
  MULIB_YANTLR_CYCLE, MULIB_YANTLR_TYPE, MULIB_YANTLR_PROVIDER,
  MULIB_YANTLR_OUTPUT
} mulib_yantlr_error_code_t;
typedef struct mulib_yantlr_span { size_t offset, length; } mulib_yantlr_span_t;
typedef struct mulib_yantlr_error {
  mulib_yantlr_error_code_t code;
  mulib_yantlr_span_t span;
  size_t line, column;
  char message[256]; /**< Owned diagnostic, including after module destruction. */
} mulib_yantlr_error_t;
/** Zero selects defaults: parser depth 2048, steps 10000000, captures 1000000;
 * host depth 256, steps 1000000, values 1000000, artifact bytes 64 MiB.
 * Limits apply separately to parsing, linking and evaluation. */
typedef struct mulib_yantlr_options {
  mulib_parsec_options_t parser;
  size_t max_depth, max_steps, max_values, max_artifact_bytes;
} mulib_yantlr_options_t;
typedef enum mulib_yantlr_type {
  MULIB_YANTLR_NULL, MULIB_YANTLR_BOOL, MULIB_YANTLR_NUMBER,
  MULIB_YANTLR_STRING, MULIB_YANTLR_ARRAY, MULIB_YANTLR_RECORD,
  MULIB_YANTLR_FUNCTION, MULIB_YANTLR_OPAQUE
} mulib_yantlr_type_t;
/** Return 1 and a run-owned value on success, otherwise 0. All arguments are
 * borrowed. Use fail() for a specific diagnostic. Functions are curried. */
typedef int (*mulib_yantlr_native_fn)(mulib_yantlr_run_t *,
    const mulib_yantlr_value_t *const *, size_t,
    const mulib_yantlr_value_t **out, void *user);
/** Exactly one of function and constant must be non-NULL. Native arity >= 1.
 * Names are exact canonical paths, e.g. Gen[Language]::Parser, or bare names.
 * Registry data is borrowed only during evaluate(); user data must outlive run. */
typedef struct mulib_yantlr_binding {
  const char *name;
  size_t arity;
  mulib_yantlr_native_fn function;
  const mulib_json_t *constant;
  void *user;
} mulib_yantlr_binding_t;
typedef struct mulib_yantlr_dependency {
  const char *name;
  mulib_yantlr_span_t span; /**< First free occurrence, even inside a lambda. */
} mulib_yantlr_dependency_t;
typedef struct mulib_yantlr_artifact {
  const char *path;
  const void *data;
  size_t length;
} mulib_yantlr_artifact_t;

/** Parse/copy an entire .y1 document. No providers are called. NULL on error. */
mulib_yantlr_module_t *mulib_yantlr_parse(const char *, size_t,
    const mulib_yantlr_options_t *, mulib_yantlr_error_t *);
/** Read a .y1 file in binary mode, then parse it. No path guessing. */
mulib_yantlr_module_t *mulib_yantlr_load(const char *,
    const mulib_yantlr_options_t *, mulib_yantlr_error_t *);
/** Release a module after all its runs; NULL is harmless. */
void mulib_yantlr_module_destroy(mulib_yantlr_module_t *);
/** Borrow the intermediary name, or NULL. */
const char *mulib_yantlr_name(const mulib_yantlr_module_t *);
/** Number of top-level set declarations. */
size_t mulib_yantlr_declaration_count(const mulib_yantlr_module_t *);
/** Borrow a declaration name; optionally copy its complete declaration span. */
const char *mulib_yantlr_declaration(const mulib_yantlr_module_t *, size_t,
    mulib_yantlr_span_t *);
/** Number of distinct external symbols; strings never imply dependencies. */
size_t mulib_yantlr_dependency_count(const mulib_yantlr_module_t *);
/** Borrow an external symbol in first-occurrence order, or NULL. */
const mulib_yantlr_dependency_t *mulib_yantlr_dependency(
    const mulib_yantlr_module_t *, size_t);
/** Preflight every name and declaration cycle before any callbacks. Evaluate
 * all declarations, dependencies first, staging artifacts in memory. On failure
 * destroy all staged values/artifacts and return NULL. Module must outlive run. */
mulib_yantlr_run_t *mulib_yantlr_evaluate(const mulib_yantlr_module_t *,
    const mulib_yantlr_binding_t *, size_t, const mulib_yantlr_options_t *,
    mulib_yantlr_error_t *);
/** Release all values, opaque owners and artifacts; NULL is harmless. */
void mulib_yantlr_run_destroy(mulib_yantlr_run_t *);
/** Borrow an evaluated declaration, or NULL for an unknown name/failed run. */
const mulib_yantlr_value_t *mulib_yantlr_get(const mulib_yantlr_run_t *, const char *);
/** Borrow the sticky first error (OK on a successful run). */
const mulib_yantlr_error_t *mulib_yantlr_error(const mulib_yantlr_run_t *);
/** Set a provider error at the current expression; always return 0. */
int mulib_yantlr_fail(mulib_yantlr_run_t *, mulib_yantlr_error_code_t, const char *);
/** Apply one argument to a native function or lexical closure. May also be used
 * by providers. First failure poisons the run and hides all artifacts. */
const mulib_yantlr_value_t *mulib_yantlr_apply(mulib_yantlr_run_t *,
    const mulib_yantlr_value_t *, const mulib_yantlr_value_t *);
/** Return the value type; a NULL pointer has type NULL. */
mulib_yantlr_type_t mulib_yantlr_typeof(const mulib_yantlr_value_t *);
/** Return a number, or zero for a non-number/NULL. */
double mulib_yantlr_numeric(const mulib_yantlr_value_t *);
/** Return a boolean, or zero for a non-boolean/NULL. */
int mulib_yantlr_boolean(const mulib_yantlr_value_t *);
/** Borrow UTF-8 bytes, including NULs; length is optional. NULL if not a string. */
const char *mulib_yantlr_text(const mulib_yantlr_value_t *, size_t *);
/** Return array/record size, otherwise zero. */
size_t mulib_yantlr_size(const mulib_yantlr_value_t *);
/** Borrow an array element or indexed record value; NULL if out of bounds. */
const mulib_yantlr_value_t *mulib_yantlr_at(const mulib_yantlr_value_t *, size_t);
/** Borrow a record key and optionally its byte length; NULL if out of bounds. */
const char *mulib_yantlr_key(const mulib_yantlr_value_t *, size_t, size_t *);
/** Borrow a record member by NUL-terminated key, or NULL if absent. */
const mulib_yantlr_value_t *mulib_yantlr_member(const mulib_yantlr_value_t *, const char *);
/** Borrow an opaque pointer only when its exact type name matches. */
void *mulib_yantlr_pointer(const mulib_yantlr_value_t *, const char *type_name);
/** Run-owned immutable constructors. Arrays/records copy references and keys;
 * children must belong to the same run. Duplicate record keys are errors. */
const mulib_yantlr_value_t *mulib_yantlr_null(mulib_yantlr_run_t *);
/** Allocate a run-owned boolean. */
const mulib_yantlr_value_t *mulib_yantlr_bool(mulib_yantlr_run_t *, int);
/** Allocate a run-owned finite binary64 number. Nonfinite input is an error. */
const mulib_yantlr_value_t *mulib_yantlr_number(mulib_yantlr_run_t *, double);
/** Copy length-delimited UTF-8 bytes into the run, including embedded NULs. */
const mulib_yantlr_value_t *mulib_yantlr_string(mulib_yantlr_run_t *, const char *, size_t);
/** Copy same-run element references into an immutable run-owned array. */
const mulib_yantlr_value_t *mulib_yantlr_array(mulib_yantlr_run_t *,
    const mulib_yantlr_value_t *const *, size_t);
/** Record keys are NUL-terminated UTF-8. Parsed records also support NUL keys
 * through indexed key()/at() queries. */
const mulib_yantlr_value_t *mulib_yantlr_record(mulib_yantlr_run_t *,
    const char *const *, const mulib_yantlr_value_t *const *, size_t);
/** Deep-copy JSON into this run. */
const mulib_yantlr_value_t *mulib_yantlr_from_json(mulib_yantlr_run_t *, const mulib_json_t *);
/** Create a curried native function. user remains borrowed. */
const mulib_yantlr_value_t *mulib_yantlr_function(mulib_yantlr_run_t *, size_t,
    mulib_yantlr_native_fn, void *user);
/** Take ownership of pointer only on success; destroy is optional. */
const mulib_yantlr_value_t *mulib_yantlr_opaque(mulib_yantlr_run_t *, const char *,
    void *pointer, void (*destroy)(void *));
/** Stage a copied artifact. Paths must be canonical relative slash paths,
 * without empty, dot or dot-dot components, backslashes or control bytes.
 * Duplicate paths fail. Never writes files or invokes a shell/compiler. */
int mulib_yantlr_emit(mulib_yantlr_run_t *, const char *, const void *, size_t);
/** Return the artifact count; zero for NULL/failed runs. */
size_t mulib_yantlr_artifact_count(const mulib_yantlr_run_t *);
/** Borrow an artifact in emission order; NULL for failed runs/out of bounds. */
const mulib_yantlr_artifact_t *mulib_yantlr_artifact(const mulib_yantlr_run_t *, size_t);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_YANTLR_H */

#if defined(MULIB_IMPL_YANTLR) && !defined(MULIB_YANTLR_IMPLEMENTED)
#define MULIB_YANTLR_IMPLEMENTED
#include "mulib-arena.h"
#include "mulib-peg.h"
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
/* The entire host grammar is compiled to parsec, not parsed by a second,
 * permissive recognizer. Enclosed rule strings are ordinary JSON strings. */
static const char mulib_yantlr_grammar_[] =
  "document <- WS 'intermediary' !MORE WS string WS ';' WS declaration*;\n"
  "declaration <- 'set' !MORE WS identifier WS expression ';' WS;\n"
  "expression <- lambda / infix;\n"
  "lambda <- '\\\\' WS (identifier WS)+ '->' WS expression;\n"
  "infix <- application (backop application)*;\n"
  "backop <- '`' WS qualified '`' WS;\n"
  "application <- postfix+;\n"
  "postfix <- atom (member / constructor)*;\n"
  "member <- '.' WS identifier WS;\n"
  "constructor <- '!' WS atom;\n"
  "atom <- group / record / array / string WS / number WS / boolean WS / nil WS / qualified;\n"
  "group <- '(' WS expression ')' WS;\n"
  "record <- '{' WS (pair (',' WS pair)*)? '}' WS;\n"
  "pair <- string WS ':' WS expression;\n"
  "array <- '[' WS (expression (',' WS expression)*)? ']' WS;\n"
  "boolean <- ('true' / 'false') !MORE;\n"
  "nil <- 'null' !MORE;\n"
  "qualified <- identifier WS parameters? ('::' WS identifier WS)*;\n"
  "parameters <- '[' WS identifier WS (',' WS identifier WS)* ']' WS;\n"
  "identifier <- [A-Za-z_] MORE*;\n"
  "number <- '-'? ('0' / [1-9] [0-9]*) ('.' [0-9]+)? ([eE] [+-]? [0-9]+)? !MORE;\n"
  "string <- '\"' ('\\\\' ([\"\\\\/bfnrt] / 'u' H H H H) / [^\"\\\\\\x00-\\x1f])* '\"';\n"
  "H <- [0-9A-Fa-f];\n"
  "MORE <- [A-Za-z_0-9];\n"
  "WS <- ([ \\t\\r\\n\\f\\v] / '//' [^\\r\\n]* / '/*' (!'*/' .)* '*/')*;\n";
typedef enum mulib_yantlr_node_kind_ {
  MULIB_Y1_DOCUMENT_, MULIB_Y1_DECLARATION_, MULIB_Y1_EXPRESSION_,
  MULIB_Y1_LAMBDA_, MULIB_Y1_INFIX_, MULIB_Y1_BACKOP_, MULIB_Y1_APPLICATION_,
  MULIB_Y1_POSTFIX_, MULIB_Y1_MEMBER_, MULIB_Y1_CONSTRUCTOR_, MULIB_Y1_ATOM_,
  MULIB_Y1_GROUP_, MULIB_Y1_RECORD_, MULIB_Y1_PAIR_, MULIB_Y1_ARRAY_,
  MULIB_Y1_BOOL_, MULIB_Y1_NULL_, MULIB_Y1_PATH_, MULIB_Y1_PARAMETERS_,
  MULIB_Y1_IDENTIFIER_, MULIB_Y1_NUMBER_, MULIB_Y1_STRING_
} mulib_yantlr_node_kind_;
typedef struct mulib_yantlr_node_ mulib_yantlr_node_;
struct mulib_yantlr_node_ {
  mulib_yantlr_node_kind_ kind;
  mulib_yantlr_span_t span;
  mulib_yantlr_node_ *child, *next;
  const char *text;
  size_t length;
  double number;
};
typedef struct mulib_yantlr_decl_ {
  const char *name;
  mulib_yantlr_node_ *node, *expression;
} mulib_yantlr_decl_;
struct mulib_yantlr_module {
  mulib_arena_t arena;
  const char *source, *name;
  size_t length, count, dependency_count;
  mulib_yantlr_decl_ *declarations;
  mulib_yantlr_dependency_t *dependencies;
};
typedef struct mulib_yantlr_env_ {
  const char *name;
  const mulib_yantlr_value_t *value;
  const struct mulib_yantlr_env_ *next;
} mulib_yantlr_env_;
typedef struct mulib_yantlr_entry_ {
  const char *key;
  size_t key_length;
  const mulib_yantlr_value_t *value;
} mulib_yantlr_entry_;
struct mulib_yantlr_value {
  mulib_yantlr_type_t type;
  mulib_yantlr_run_t *owner;
  mulib_yantlr_value_t *next;
  union {
    double number;
    struct { const char *data; size_t length; } text;
    struct { mulib_yantlr_entry_ *items; size_t count; } list;
    struct {
      mulib_yantlr_native_fn native;
      void *user;
      size_t arity, given;
      const mulib_yantlr_value_t **arguments;
      const mulib_yantlr_node_ *parameter, *body;
      const mulib_yantlr_env_ *environment;
    } function;
    struct { const char *type; void *pointer; void (*destroy)(void *); } opaque;
  } as;
};
typedef struct mulib_yantlr_output_ {
  mulib_yantlr_artifact_t artifact;
  struct mulib_yantlr_output_ *next;
} mulib_yantlr_output_;
struct mulib_yantlr_run {
  mulib_arena_t arena;
  const mulib_yantlr_module_t *module;
  mulib_yantlr_options_t options;
  mulib_yantlr_error_t error;
  mulib_yantlr_span_t current;
  size_t steps, depth, value_count, output_bytes, output_count;
  const mulib_yantlr_binding_t *bindings;
  size_t binding_count;
  const mulib_yantlr_value_t **globals, **externals;
  unsigned char *marks;
  size_t *order, order_count;
  mulib_yantlr_value_t *values;
  mulib_yantlr_output_ *outputs, *last_output;
};
static mulib_yantlr_options_t
mulib_yantlr_options_(const mulib_yantlr_options_t *o) {
  mulib_yantlr_options_t r;
  memset(&r, 0, sizeof r);
  if (o) r = *o;
  if (!r.parser.max_depth) r.parser.max_depth = 2048;
  if (!r.parser.max_steps) r.parser.max_steps = 10000000;
  if (!r.parser.max_captures) r.parser.max_captures = 1000000;
  if (!r.max_depth) r.max_depth = 256;
  if (!r.max_steps) r.max_steps = 1000000;
  if (!r.max_values) r.max_values = 1000000;
  if (!r.max_artifact_bytes) r.max_artifact_bytes = 64u * 1024u * 1024u;
  return r;
}
static int
mulib_yantlr_error_(mulib_yantlr_error_t *e, mulib_yantlr_error_code_t code,
    const char *source, mulib_yantlr_span_t span, const char *message) {
  size_t i;
  if (e && !e->code) {
    e->code = code; e->span = span; e->line = e->column = 1;
    snprintf(e->message, sizeof e->message, "%s", message ? message : "YANTLR error");
    for (i = 0; source && i < span.offset; ++i)
      if (source[i] == '\n') { ++e->line; e->column = 1; } else ++e->column;
  }
  return 0;
}
static void *
mulib_yantlr_alloc_(mulib_arena_t *a, size_t count, size_t size) {
  void *p;
  if (size && count > SIZE_MAX / size) return NULL;
  p = mulib_arena_alloc(a, count * size);
  if (p) memset(p, 0, count * size);
  return p;
}
static char *
mulib_yantlr_copy_(mulib_arena_t *a, const char *s, size_t n) {
  char *p;
  if ((!s && n) || n == SIZE_MAX) return NULL;
  p = (char *)mulib_arena_alloc(a, n + 1);
  if (p) { if (n) memcpy(p, s, n); p[n] = 0; }
  return p;
}
static size_t
mulib_yantlr_find_(const mulib_yantlr_module_t *m, const char *name) {
  size_t i;
  for (i = 0; i < m->count; ++i)
    if (!strcmp(m->declarations[i].name, name)) return i;
  return SIZE_MAX;
}
static int
mulib_yantlr_path_(mulib_yantlr_module_t *m, mulib_yantlr_node_ *n) {
  mulib_strbuf_t b;
  mulib_yantlr_node_ *c, *p;
  int first = 1, ok = 1;
  mulib_strbuf_init(&b);
  for (c = n->child; c && ok; c = c->next) {
    if (c->kind == MULIB_Y1_PARAMETERS_) {
      ok = mulib_strbuf_putc(&b, '[');
      for (p = c->child; p && ok; p = p->next) {
        if (p != c->child) ok = mulib_strbuf_putc(&b, ',');
        if (ok) ok = mulib_strbuf_append(&b, p->text);
      }
      if (ok) ok = mulib_strbuf_putc(&b, ']');
    } else {
      if (!first) ok = mulib_strbuf_append(&b, "::");
      if (ok) ok = mulib_strbuf_append(&b, c->text);
      first = 0;
    }
  }
  if (ok) {
    n->text = mulib_yantlr_copy_(&m->arena, b.data, b.length);
    n->length = b.length; ok = n->text != NULL;
  }
  mulib_strbuf_free(&b);
  return ok;
}
typedef struct mulib_yantlr_scope_ {
  const mulib_yantlr_node_ *parameters;
  const struct mulib_yantlr_scope_ *next;
} mulib_yantlr_scope_;
static int
mulib_yantlr_local_(const mulib_yantlr_scope_ *s, const char *name) {
  const mulib_yantlr_node_ *p;
  for (; s; s = s->next)
    for (p = s->parameters; p && p->kind == MULIB_Y1_IDENTIFIER_; p = p->next)
      if (!strcmp(p->text, name)) return 1;
  return 0;
}
/* Shared static walk: collect free symbols at parse time, or traverse the
 * declaration DAG at link time. Lambda parameters never become dependencies. */
static int mulib_yantlr_walk_(mulib_yantlr_run_t *, const mulib_yantlr_node_ *,
    const mulib_yantlr_scope_ *, int);
static int
mulib_yantlr_enter_(mulib_yantlr_run_t *r) {
  if (r->error.code) return 0;
  if (r->steps >= r->options.max_steps || r->depth >= r->options.max_depth)
    return mulib_yantlr_fail(r, MULIB_YANTLR_LIMIT, "host resource limit");
  ++r->steps; ++r->depth; return 1;
}
static int
mulib_yantlr_link_decl_(mulib_yantlr_run_t *r, size_t i) {
  if (r->marks[i] == 2) return 1;
  if (r->marks[i] == 1)
    return mulib_yantlr_fail(r, MULIB_YANTLR_CYCLE, "cyclic set dependency");
  r->marks[i] = 1;
  if (!mulib_yantlr_walk_(r, r->module->declarations[i].expression, NULL, 1)) return 0;
  r->marks[i] = 2; r->order[r->order_count++] = i; return 1;
}
static int
mulib_yantlr_walk_(mulib_yantlr_run_t *r, const mulib_yantlr_node_ *n,
    const mulib_yantlr_scope_ *scope, int link) {
  const mulib_yantlr_node_ *c;
  mulib_yantlr_scope_ nested;
  size_t i;
  int ok = 1;
  r->current = n->span;
  if (!mulib_yantlr_enter_(r)) return 0;
  if (n->kind == MULIB_Y1_PATH_) {
    if (!mulib_yantlr_local_(scope, n->text)) {
      i = mulib_yantlr_find_(r->module, n->text);
      if (link) {
        if (i != SIZE_MAX) ok = mulib_yantlr_link_decl_(r, i);
      } else if (i == SIZE_MAX) {
        mulib_yantlr_module_t *m = (mulib_yantlr_module_t *)r->module;
        for (i = 0; i < m->dependency_count; ++i)
          if (!strcmp(m->dependencies[i].name, n->text)) break;
        if (i == m->dependency_count) {
          m->dependencies[i].name = n->text; m->dependencies[i].span = n->span;
          ++m->dependency_count;
        }
      }
    }
  } else {
    c = n->child;
    if (n->kind == MULIB_Y1_LAMBDA_) {
      nested.parameters = c; nested.next = scope; scope = &nested;
      while (c && c->kind == MULIB_Y1_IDENTIFIER_) c = c->next;
    }
    for (; c && ok; c = c->next) ok = mulib_yantlr_walk_(r, c, scope, link);
  }
  --r->depth; return ok;
}
mulib_yantlr_module_t *
mulib_yantlr_parse(const char *source, size_t length,
    const mulib_yantlr_options_t *options, mulib_yantlr_error_t *error) {
  mulib_yantlr_error_t local;
  mulib_yantlr_options_t o = mulib_yantlr_options_(options);
  mulib_yantlr_module_t *m = NULL;
  mulib_peg_t *g;
  mulib_peg_error_t ge;
  mulib_parsec_result_t result;
  mulib_yantlr_node_ **stack = NULL, *root = NULL, *n, *c;
  size_t *indices = NULL, used = 0, i, j, base, paths = 0;
  mulib_yantlr_span_t zero = {0, 0};
  int status;
  if (!error) error = &local;
  memset(error, 0, sizeof *error);
  if ((!source && length) || length == SIZE_MAX) {
    mulib_yantlr_error_(error, MULIB_YANTLR_INVALID, NULL, zero, "invalid source"); return NULL;
  }
  g = mulib_peg_compile(mulib_yantlr_grammar_, sizeof mulib_yantlr_grammar_ - 1, &ge);
  if (!g) {
    mulib_yantlr_error_(error, MULIB_YANTLR_MEMORY, NULL, zero, "host grammar allocation failed");
    return NULL;
  }
  status = mulib_peg_run(g, NULL, source, length, 1, &o.parser, &result);
  mulib_peg_destroy(g);
  if (status != 1) {
    zero.offset = result.error.offset;
    mulib_yantlr_error_(error,
        result.error.code == MULIB_PARSEC_MEMORY ? MULIB_YANTLR_MEMORY :
        result.error.code == MULIB_PARSEC_LIMIT ? MULIB_YANTLR_LIMIT : MULIB_YANTLR_SYNTAX,
        source, zero, "input does not match YANTLR host grammar");
    mulib_parsec_result_free(&result); return NULL;
  }
  m = (mulib_yantlr_module_t *)calloc(1, sizeof *m);
  if (!m) goto memory;
  mulib_arena_init(&m->arena, 4096); m->length = length;
  m->source = mulib_yantlr_copy_(&m->arena, source, length);
  if (!m->source || result.count > SIZE_MAX / sizeof *stack ||
      result.count > SIZE_MAX / sizeof *indices) goto memory;
  stack = (mulib_yantlr_node_ **)malloc(result.count * sizeof *stack);
  indices = (size_t *)malloc(result.count * sizeof *indices);
  if (!stack || !indices) goto memory;
  for (i = 0; i < result.count; ++i) {
    const mulib_parsec_capture_t *cap = &result.captures[i];
    /* Rule tags follow the grammar order above; only lexical helpers are
     * discarded. Structural children are folded from a linear postorder stack. */
    if (cap->tag > MULIB_Y1_STRING_) continue;
    n = (mulib_yantlr_node_ *)mulib_yantlr_alloc_(&m->arena, 1, sizeof *n);
    if (!n) goto memory;
    n->kind = (mulib_yantlr_node_kind_)cap->tag;
    n->span.offset = cap->offset; n->span.length = cap->length;
    base = used;
    while (base && indices[base - 1] >= cap->first) --base;
    if (base < used) n->child = stack[base];
    for (j = base; j + 1 < used; ++j) stack[j]->next = stack[j + 1];
    used = base; stack[used] = n; indices[used++] = i;
    if (n->kind == MULIB_Y1_IDENTIFIER_) {
      n->text = mulib_yantlr_copy_(&m->arena, source + cap->offset, cap->length);
      n->length = cap->length;
      if (!n->text) goto memory;
      if (!strcmp(n->text, "set") || !strcmp(n->text, "intermediary") ||
          !strcmp(n->text, "true") || !strcmp(n->text, "false") || !strcmp(n->text, "null")) {
        mulib_yantlr_error_(error, MULIB_YANTLR_SYNTAX, source, n->span, "reserved identifier"); goto fail;
      }
    } else if (n->kind == MULIB_Y1_STRING_ || n->kind == MULIB_Y1_NUMBER_) {
      mulib_json_error_t je;
      mulib_json_t *v = mulib_json_parse(source + cap->offset, cap->length, &je);
      if (!v) {
        mulib_yantlr_span_t at = n->span;
        at.offset += je.offset; at.length = je.offset < cap->length ? 1 : 0;
        mulib_yantlr_error_(error, je.message && strstr(je.message, "memory") && !strstr(je.message, "range") ?
            MULIB_YANTLR_MEMORY : MULIB_YANTLR_SYNTAX, source, at, je.message); goto fail;
      }
      if (n->kind == MULIB_Y1_STRING_) {
        const char *s = mulib_json_text(v, &n->length);
        n->text = mulib_yantlr_copy_(&m->arena, s, n->length);
      } else n->number = mulib_json_numeric(v);
      mulib_json_destroy(v);
      if (n->kind == MULIB_Y1_STRING_ && !n->text) goto memory;
    } else if (n->kind == MULIB_Y1_BOOL_) n->number = source[cap->offset] == 't';
    else if (n->kind == MULIB_Y1_PATH_) {
      if (!mulib_yantlr_path_(m, n)) goto memory;
      ++paths;
    } else if (n->kind == MULIB_Y1_RECORD_) {
      mulib_yantlr_node_ *p;
      for (c = n->child; c; c = c->next)
        for (p = n->child; p != c; p = p->next)
          if (p->child->length == c->child->length &&
              !memcmp(p->child->text, c->child->text, c->child->length)) {
            mulib_yantlr_error_(error, MULIB_YANTLR_DUPLICATE, source, c->child->span,
                "duplicate record key"); goto fail;
          }
    } else if (n->kind == MULIB_Y1_LAMBDA_) {
      mulib_yantlr_node_ *p;
      for (c = n->child; c && c->kind == MULIB_Y1_IDENTIFIER_; c = c->next)
        for (p = n->child; p != c; p = p->next)
          if (!strcmp(c->text, p->text)) {
            mulib_yantlr_error_(error, MULIB_YANTLR_DUPLICATE, source, c->span,
                "duplicate lambda parameter"); goto fail;
          }
    }
  }
  root = stack[0]; m->name = root->child->text;
  if (!root->child->length || strlen(m->name) != root->child->length) {
    mulib_yantlr_error_(error, MULIB_YANTLR_SYNTAX, source, root->child->span,
        "intermediary name must be nonempty and NUL-free"); goto fail;
  }
  for (c = root->child->next; c; c = c->next) ++m->count;
  m->declarations = (mulib_yantlr_decl_ *)mulib_yantlr_alloc_(&m->arena, m->count, sizeof *m->declarations);
  m->dependencies = (mulib_yantlr_dependency_t *)mulib_yantlr_alloc_(&m->arena, paths, sizeof *m->dependencies);
  if (!m->declarations || !m->dependencies) goto memory;
  i = 0;
  for (c = root->child->next; c; c = c->next, ++i) {
    for (j = 0; j < i; ++j)
      if (!strcmp(m->declarations[j].name, c->child->text)) {
        mulib_yantlr_error_(error, MULIB_YANTLR_DUPLICATE, source, c->child->span,
            "duplicate set declaration"); goto fail;
      }
    m->declarations[i].name = c->child->text;
    m->declarations[i].node = c;
    m->declarations[i].expression = c->child->next;
  }
  {
    mulib_yantlr_run_t walk;
    memset(&walk, 0, sizeof walk); walk.module = m; walk.options = o;
    for (i = 0; i < m->count; ++i)
      if (!mulib_yantlr_walk_(&walk, m->declarations[i].expression, NULL, 0)) {
        *error = walk.error; goto fail;
      }
  }
  free(stack); free(indices); mulib_parsec_result_free(&result); return m;
memory:
  mulib_yantlr_error_(error, MULIB_YANTLR_MEMORY, source, zero, "out of memory");
fail:
  free(stack); free(indices); mulib_parsec_result_free(&result);
  mulib_yantlr_module_destroy(m); return NULL;
}
mulib_yantlr_module_t *
mulib_yantlr_load(const char *path, const mulib_yantlr_options_t *o, mulib_yantlr_error_t *e) {
  FILE *f;
  mulib_strbuf_t b;
  char bytes[4096];
  size_t n;
  int bad;
  mulib_yantlr_module_t *m;
  mulib_yantlr_span_t zero = {0, 0};
  if (e) memset(e, 0, sizeof *e);
  if (!path || !(f = fopen(path, "rb"))) {
    mulib_yantlr_error_(e, MULIB_YANTLR_IO, NULL, zero, "cannot open .y1 file"); return NULL;
  }
  mulib_strbuf_init(&b);
  while ((n = fread(bytes, 1, sizeof bytes, f)) != 0) {
    if (!mulib_strbuf_append_n(&b, bytes, n)) {
      fclose(f); mulib_strbuf_free(&b);
      mulib_yantlr_error_(e, MULIB_YANTLR_MEMORY, NULL, zero, "out of memory"); return NULL;
    }
    if (ferror(f) || feof(f)) break;
  }
  bad = ferror(f); if (fclose(f)) bad = 1;
  if (bad) {
    mulib_strbuf_free(&b);
    mulib_yantlr_error_(e, MULIB_YANTLR_IO, NULL, zero, "cannot read .y1 file"); return NULL;
  }
  m = mulib_yantlr_parse(b.data, b.length, o, e); mulib_strbuf_free(&b); return m;
}
void mulib_yantlr_module_destroy(mulib_yantlr_module_t *m) {
  if (m) { mulib_arena_reset(&m->arena); free(m); }
}
const char *mulib_yantlr_name(const mulib_yantlr_module_t *m) { return m ? m->name : NULL; }
size_t mulib_yantlr_declaration_count(const mulib_yantlr_module_t *m) { return m ? m->count : 0; }
const char *mulib_yantlr_declaration(const mulib_yantlr_module_t *m, size_t i, mulib_yantlr_span_t *s) {
  if (!m || i >= m->count) return NULL;
  if (s) *s = m->declarations[i].node->span;
  return m->declarations[i].name;
}
size_t mulib_yantlr_dependency_count(const mulib_yantlr_module_t *m) { return m ? m->dependency_count : 0; }
const mulib_yantlr_dependency_t *mulib_yantlr_dependency(const mulib_yantlr_module_t *m, size_t i) {
  return m && i < m->dependency_count ? &m->dependencies[i] : NULL;
}
int mulib_yantlr_fail(mulib_yantlr_run_t *r, mulib_yantlr_error_code_t code, const char *message) {
  if (r) mulib_yantlr_error_(&r->error, code == MULIB_YANTLR_OK ? MULIB_YANTLR_PROVIDER : code,
      r->module->source, r->current, message);
  return 0;
}
const mulib_yantlr_error_t *mulib_yantlr_error(const mulib_yantlr_run_t *r) { return r ? &r->error : NULL; }
static void *mulib_yantlr_run_alloc_(mulib_yantlr_run_t *r, size_t n, size_t size) {
  void *p;
  if (!r || r->error.code) return NULL;
  p = mulib_yantlr_alloc_(&r->arena, n, size);
  if (!p) mulib_yantlr_fail(r, MULIB_YANTLR_MEMORY, "out of memory");
  return p;
}
static const char *mulib_yantlr_run_copy_(mulib_yantlr_run_t *r, const char *s, size_t n) {
  char *p;
  if (!r || r->error.code) return NULL;
  p = mulib_yantlr_copy_(&r->arena, s, n);
  if (!p) mulib_yantlr_fail(r, MULIB_YANTLR_MEMORY, "invalid size or out of memory");
  return p;
}
static mulib_yantlr_value_t *mulib_yantlr_value_(mulib_yantlr_run_t *r, mulib_yantlr_type_t type) {
  mulib_yantlr_value_t *v;
  if (!r || r->error.code) return NULL;
  if (r->value_count >= r->options.max_values) {
    mulib_yantlr_fail(r, MULIB_YANTLR_LIMIT, "value limit"); return NULL;
  }
  v = (mulib_yantlr_value_t *)mulib_yantlr_run_alloc_(r, 1, sizeof *v);
  if (v) {
    v->type = type; v->owner = r; v->next = r->values; r->values = v; ++r->value_count;
  }
  return v;
}
mulib_yantlr_type_t mulib_yantlr_typeof(const mulib_yantlr_value_t *v) { return v ? v->type : MULIB_YANTLR_NULL; }
double mulib_yantlr_numeric(const mulib_yantlr_value_t *v) { return v && v->type == MULIB_YANTLR_NUMBER ? v->as.number : 0; }
int mulib_yantlr_boolean(const mulib_yantlr_value_t *v) { return v && v->type == MULIB_YANTLR_BOOL ? v->as.number != 0 : 0; }
const char *mulib_yantlr_text(const mulib_yantlr_value_t *v, size_t *n) {
  if (n) *n = v && v->type == MULIB_YANTLR_STRING ? v->as.text.length : 0;
  return v && v->type == MULIB_YANTLR_STRING ? v->as.text.data : NULL;
}
size_t mulib_yantlr_size(const mulib_yantlr_value_t *v) {
  return v && (v->type == MULIB_YANTLR_ARRAY || v->type == MULIB_YANTLR_RECORD) ? v->as.list.count : 0;
}
const mulib_yantlr_value_t *mulib_yantlr_at(const mulib_yantlr_value_t *v, size_t i) {
  return i < mulib_yantlr_size(v) ? v->as.list.items[i].value : NULL;
}
const char *mulib_yantlr_key(const mulib_yantlr_value_t *v, size_t i, size_t *n) {
  int ok = v && v->type == MULIB_YANTLR_RECORD && i < v->as.list.count;
  if (n) *n = ok ? v->as.list.items[i].key_length : 0;
  return ok ? v->as.list.items[i].key : NULL;
}
static const mulib_yantlr_value_t *mulib_yantlr_member_(const mulib_yantlr_value_t *v, const char *s, size_t n) {
  size_t i;
  if (!v || v->type != MULIB_YANTLR_RECORD) return NULL;
  for (i = 0; i < v->as.list.count; ++i)
    if (v->as.list.items[i].key_length == n && !memcmp(v->as.list.items[i].key, s, n))
      return v->as.list.items[i].value;
  return NULL;
}
const mulib_yantlr_value_t *mulib_yantlr_member(const mulib_yantlr_value_t *v, const char *s) {
  return s ? mulib_yantlr_member_(v, s, strlen(s)) : NULL;
}
void *mulib_yantlr_pointer(const mulib_yantlr_value_t *v, const char *type) {
  return v && v->type == MULIB_YANTLR_OPAQUE && type && !strcmp(type, v->as.opaque.type) ? v->as.opaque.pointer : NULL;
}
const mulib_yantlr_value_t *mulib_yantlr_null(mulib_yantlr_run_t *r) { return mulib_yantlr_value_(r, MULIB_YANTLR_NULL); }
const mulib_yantlr_value_t *mulib_yantlr_bool(mulib_yantlr_run_t *r, int b) {
  mulib_yantlr_value_t *v = mulib_yantlr_value_(r, MULIB_YANTLR_BOOL);
  if (v) v->as.number = !!b;
  return v;
}
const mulib_yantlr_value_t *mulib_yantlr_number(mulib_yantlr_run_t *r, double d) {
  mulib_yantlr_value_t *v;
  if (!isfinite(d)) { mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "nonfinite number"); return NULL; }
  v = mulib_yantlr_value_(r, MULIB_YANTLR_NUMBER); if (v) v->as.number = d; return v;
}
const mulib_yantlr_value_t *mulib_yantlr_string(mulib_yantlr_run_t *r, const char *s, size_t n) {
  mulib_yantlr_value_t *v;
  if ((!s && n) || n == SIZE_MAX || !mulib_utf8_validate((const unsigned char *)s, n)) {
    mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "invalid UTF-8 string"); return NULL;
  }
  v = mulib_yantlr_value_(r, MULIB_YANTLR_STRING);
  if (!v) return NULL;
  v->as.text.data = mulib_yantlr_run_copy_(r, s, n); v->as.text.length = n;
  return v->as.text.data ? v : NULL;
}
static mulib_yantlr_value_t *mulib_yantlr_list_(mulib_yantlr_run_t *r, mulib_yantlr_type_t t, size_t n) {
  mulib_yantlr_value_t *v = mulib_yantlr_value_(r, t);
  if (!v) return NULL;
  v->as.list.items = (mulib_yantlr_entry_ *)mulib_yantlr_run_alloc_(r, n, sizeof *v->as.list.items);
  v->as.list.count = n; return v->as.list.items ? v : NULL;
}
static int mulib_yantlr_owned_(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *v) {
  return v && v->owner == r ? 1 : mulib_yantlr_fail(r, MULIB_YANTLR_INVALID, "value belongs to another run or is NULL");
}
const mulib_yantlr_value_t *mulib_yantlr_array(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *const *a, size_t n) {
  mulib_yantlr_value_t *v;
  size_t i;
  if (!a && n) { mulib_yantlr_fail(r, MULIB_YANTLR_INVALID, "null elements"); return NULL; }
  v = mulib_yantlr_list_(r, MULIB_YANTLR_ARRAY, n); if (!v) return NULL;
  for (i = 0; i < n; ++i) {
    if (!mulib_yantlr_owned_(r, a[i])) return NULL;
    v->as.list.items[i].value = a[i];
  }
  return v;
}
static int mulib_yantlr_set_entry_(mulib_yantlr_run_t *r, mulib_yantlr_value_t *v,
    size_t i, const char *key, size_t n, const mulib_yantlr_value_t *item) {
  size_t j;
  if (!mulib_yantlr_owned_(r, item)) return 0;
  for (j = 0; j < i; ++j)
    if (v->as.list.items[j].key_length == n && !memcmp(v->as.list.items[j].key, key, n))
      return mulib_yantlr_fail(r, MULIB_YANTLR_DUPLICATE, "duplicate record key");
  v->as.list.items[i].key = mulib_yantlr_run_copy_(r, key, n);
  v->as.list.items[i].key_length = n; v->as.list.items[i].value = item;
  return v->as.list.items[i].key != NULL;
}
const mulib_yantlr_value_t *mulib_yantlr_record(mulib_yantlr_run_t *r, const char *const *keys,
    const mulib_yantlr_value_t *const *a, size_t n) {
  mulib_yantlr_value_t *v;
  size_t i, len;
  if ((!a || !keys) && n) { mulib_yantlr_fail(r, MULIB_YANTLR_INVALID, "null members"); return NULL; }
  v = mulib_yantlr_list_(r, MULIB_YANTLR_RECORD, n); if (!v) return NULL;
  for (i = 0; i < n; ++i) {
    if (!keys[i]) { mulib_yantlr_fail(r, MULIB_YANTLR_INVALID, "null key"); return NULL; }
    len = strlen(keys[i]);
    if (!mulib_utf8_validate((const unsigned char *)keys[i], len)) {
      mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "invalid UTF-8 key"); return NULL;
    }
    if (!mulib_yantlr_set_entry_(r, v, i, keys[i], len, a[i])) return NULL;
  }
  return v;
}
const mulib_yantlr_value_t *mulib_yantlr_from_json(mulib_yantlr_run_t *r, const mulib_json_t *j) {
  const mulib_yantlr_value_t *v = NULL, *item;
  mulib_yantlr_value_t *list;
  size_t i, n;
  const char *s;
  if (!r || !mulib_yantlr_enter_(r)) return NULL;
  switch (mulib_json_typeof(j)) {
    case MULIB_JSON_NULL: v = mulib_yantlr_null(r); break;
    case MULIB_JSON_BOOL: v = mulib_yantlr_bool(r, mulib_json_boolean(j)); break;
    case MULIB_JSON_NUMBER: v = mulib_yantlr_number(r, mulib_json_numeric(j)); break;
    case MULIB_JSON_STRING:
      s = mulib_json_text(j, &n); v = mulib_yantlr_string(r, s, n); break;
    case MULIB_JSON_ARRAY: case MULIB_JSON_OBJECT:
      n = mulib_json_size(j);
      list = mulib_yantlr_list_(r, mulib_json_typeof(j) == MULIB_JSON_ARRAY ? MULIB_YANTLR_ARRAY : MULIB_YANTLR_RECORD, n);
      if (!list) break;
      for (i = 0; i < n; ++i) {
        item = mulib_yantlr_from_json(r, mulib_json_at(j, i)); if (!item) break;
        if (list->type == MULIB_YANTLR_RECORD) {
          size_t len;
          s = mulib_json_key(j, i, &len);
          if (!mulib_yantlr_set_entry_(r, list, i, s, len, item)) break;
        } else list->as.list.items[i].value = item;
      }
      if (i == n) v = list;
      break;
  }
  --r->depth; return v;
}
const mulib_yantlr_value_t *mulib_yantlr_function(mulib_yantlr_run_t *r, size_t arity,
    mulib_yantlr_native_fn fn, void *user) {
  mulib_yantlr_value_t *v;
  if (!arity || !fn) { mulib_yantlr_fail(r, MULIB_YANTLR_INVALID, "invalid native function"); return NULL; }
  v = mulib_yantlr_value_(r, MULIB_YANTLR_FUNCTION);
  if (v) { v->as.function.native = fn; v->as.function.arity = arity; v->as.function.user = user; }
  return v;
}
const mulib_yantlr_value_t *mulib_yantlr_opaque(mulib_yantlr_run_t *r, const char *type,
    void *pointer, void (*destroy)(void *)) {
  mulib_yantlr_value_t *v;
  const char *copy;
  if (!type || !*type) { mulib_yantlr_fail(r, MULIB_YANTLR_INVALID, "missing opaque type"); return NULL; }
  copy = mulib_yantlr_run_copy_(r, type, strlen(type)); if (!copy) return NULL;
  v = mulib_yantlr_value_(r, MULIB_YANTLR_OPAQUE);
  if (v) { v->as.opaque.type = copy; v->as.opaque.pointer = pointer; v->as.opaque.destroy = destroy; }
  return v;
}
static const mulib_yantlr_value_t *mulib_yantlr_eval_(mulib_yantlr_run_t *,
    const mulib_yantlr_node_ *, const mulib_yantlr_env_ *);
const mulib_yantlr_value_t *mulib_yantlr_apply(mulib_yantlr_run_t *r,
    const mulib_yantlr_value_t *fn, const mulib_yantlr_value_t *arg) {
  mulib_yantlr_value_t *v;
  const mulib_yantlr_value_t *out = NULL;
  mulib_yantlr_env_ *env;
  const mulib_yantlr_node_ *p;
  size_t n;
  int status;
  if (!r || !mulib_yantlr_enter_(r)) return NULL;
  if (!mulib_yantlr_owned_(r, fn) || !mulib_yantlr_owned_(r, arg)) goto done;
  if (fn->type != MULIB_YANTLR_FUNCTION) {
    mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "application requires a function"); goto done;
  }
  if (fn->as.function.native) {
    n = fn->as.function.given;
    v = mulib_yantlr_value_(r, MULIB_YANTLR_FUNCTION); if (!v) goto done;
    v->as.function = fn->as.function;
    v->as.function.arguments = (const mulib_yantlr_value_t **)mulib_yantlr_run_alloc_(r, n + 1, sizeof *v->as.function.arguments);
    if (!v->as.function.arguments) goto done;
    if (n) memcpy(v->as.function.arguments, fn->as.function.arguments, n * sizeof *v->as.function.arguments);
    v->as.function.arguments[n] = arg; v->as.function.given = n + 1;
    if (n + 1 < v->as.function.arity) out = v;
    else {
      status = v->as.function.native(r, v->as.function.arguments, n + 1, &out, v->as.function.user);
      if (status != 1 || !out) {
        mulib_yantlr_fail(r, MULIB_YANTLR_PROVIDER, "provider failed or omitted its result"); out = NULL;
      } else if (!mulib_yantlr_owned_(r, out)) out = NULL;
    }
  } else {
    env = (mulib_yantlr_env_ *)mulib_yantlr_run_alloc_(r, 1, sizeof *env); if (!env) goto done;
    p = fn->as.function.parameter;
    env->name = p->text; env->value = arg; env->next = fn->as.function.environment;
    p = p->next;
    if (p->kind == MULIB_Y1_IDENTIFIER_) {
      v = mulib_yantlr_value_(r, MULIB_YANTLR_FUNCTION); if (!v) goto done;
      v->as.function = fn->as.function; v->as.function.parameter = p;
      v->as.function.environment = env; out = v;
    } else out = mulib_yantlr_eval_(r, fn->as.function.body, env);
  }
done:
  --r->depth; return r->error.code ? NULL : out;
}
static const mulib_yantlr_value_t *mulib_yantlr_global_(mulib_yantlr_run_t *r, size_t i) {
  if (r->marks[i] == 2) return r->globals[i];
  if (r->marks[i] == 1) { mulib_yantlr_fail(r, MULIB_YANTLR_CYCLE, "cyclic evaluation"); return NULL; }
  r->marks[i] = 1;
  r->globals[i] = mulib_yantlr_eval_(r, r->module->declarations[i].expression, NULL);
  if (r->globals[i]) r->marks[i] = 2;
  return r->globals[i];
}
static const mulib_yantlr_value_t *mulib_yantlr_lookup_(mulib_yantlr_run_t *r,
    const char *name, const mulib_yantlr_env_ *env) {
  size_t i;
  for (; env; env = env->next) if (!strcmp(env->name, name)) return env->value;
  i = mulib_yantlr_find_(r->module, name);
  if (i != SIZE_MAX) return mulib_yantlr_global_(r, i);
  for (i = 0; i < r->binding_count; ++i)
    if (!strcmp(r->bindings[i].name, name)) {
      const mulib_yantlr_binding_t *b = &r->bindings[i];
      if (!r->externals[i]) r->externals[i] = b->function ?
          mulib_yantlr_function(r, b->arity, b->function, b->user) : mulib_yantlr_from_json(r, b->constant);
      return r->externals[i];
    }
  mulib_yantlr_fail(r, MULIB_YANTLR_NAME, "unresolved symbol"); return NULL;
}
static const mulib_yantlr_value_t *
mulib_yantlr_eval_(mulib_yantlr_run_t *r, const mulib_yantlr_node_ *n, const mulib_yantlr_env_ *env) {
  const mulib_yantlr_node_ *c, *p;
  const mulib_yantlr_value_t *v = NULL, *arg, *fn;
  mulib_yantlr_value_t *list;
  mulib_yantlr_span_t old = r->current;
  size_t count, i;
  r->current = n->span;
  if (!mulib_yantlr_enter_(r)) { r->current = old; return NULL; }
  switch (n->kind) {
    case MULIB_Y1_STRING_: v = mulib_yantlr_string(r, n->text, n->length); break;
    case MULIB_Y1_NUMBER_: v = mulib_yantlr_number(r, n->number); break;
    case MULIB_Y1_BOOL_: v = mulib_yantlr_bool(r, n->number != 0); break;
    case MULIB_Y1_NULL_: v = mulib_yantlr_null(r); break;
    case MULIB_Y1_PATH_: v = mulib_yantlr_lookup_(r, n->text, env); break;
    case MULIB_Y1_LAMBDA_:
      list = mulib_yantlr_value_(r, MULIB_YANTLR_FUNCTION); if (!list) break;
      list->as.function.parameter = n->child;
      for (c = n->child; c->kind == MULIB_Y1_IDENTIFIER_; c = c->next) { }
      list->as.function.body = c; list->as.function.environment = env; v = list; break;
    case MULIB_Y1_ARRAY_: case MULIB_Y1_RECORD_:
      count = 0; for (c = n->child; c; c = c->next) ++count;
      list = mulib_yantlr_list_(r, n->kind == MULIB_Y1_ARRAY_ ? MULIB_YANTLR_ARRAY : MULIB_YANTLR_RECORD, count);
      if (!list) break;
      i = 0;
      for (c = n->child; c; c = c->next, ++i) {
        p = list->type == MULIB_YANTLR_RECORD ? c->child->next : c;
        arg = mulib_yantlr_eval_(r, p, env); if (!arg) break;
        if (list->type == MULIB_YANTLR_RECORD) {
          if (!mulib_yantlr_set_entry_(r, list, i, c->child->text, c->child->length, arg)) break;
        } else list->as.list.items[i].value = arg;
      }
      if (i == count) v = list;
      break;
    case MULIB_Y1_APPLICATION_: case MULIB_Y1_POSTFIX_: case MULIB_Y1_INFIX_:
      v = mulib_yantlr_eval_(r, n->child, env);
      for (c = n->child->next; c && v; c = c->next) {
        r->current = c->span;
        if (c->kind == MULIB_Y1_MEMBER_) {
          arg = mulib_yantlr_member(v, c->child->text);
          if (!arg) mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "missing record member or non-record base");
          v = arg;
        } else if (c->kind == MULIB_Y1_BACKOP_) {
          fn = mulib_yantlr_eval_(r, c->child, env);
          if (!fn) { v = NULL; break; }
          fn = mulib_yantlr_apply(r, fn, v);
          c = c->next;
          arg = fn ? mulib_yantlr_eval_(r, c, env) : NULL;
          v = arg ? mulib_yantlr_apply(r, fn, arg) : NULL;
        } else {
          arg = mulib_yantlr_eval_(r, c->kind == MULIB_Y1_CONSTRUCTOR_ ? c->child : c, env);
          v = arg ? mulib_yantlr_apply(r, v, arg) : NULL;
        }
      }
      break;
    default: v = mulib_yantlr_eval_(r, n->child, env); break;
  }
  --r->depth; r->current = old; return r->error.code ? NULL : v;
}
static int mulib_yantlr_ident_first_(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int mulib_yantlr_binding_name_(const char *s) {
  size_t i = 0;
  int parameters = 0, after = 0;
  if (!s) return 0;
  for (;;) {
    if (!mulib_yantlr_ident_first_((unsigned char)s[i])) return 0;
    do { ++i; } while (mulib_yantlr_ident_first_((unsigned char)s[i]) || (s[i] >= '0' && s[i] <= '9'));
    if (!s[i]) return !parameters;
    if (s[i] == '[' && !parameters && !after) { parameters = 1; ++i; }
    else if (s[i] == ',' && parameters) ++i;
    else {
      if (s[i] == ']' && parameters) { parameters = 0; ++i; }
      if (!s[i]) return !parameters;
      if (parameters || s[i] != ':' || s[i + 1] != ':') return 0;
      i += 2; after = 1;
    }
  }
}
mulib_yantlr_run_t *
mulib_yantlr_evaluate(const mulib_yantlr_module_t *m, const mulib_yantlr_binding_t *bindings,
    size_t count, const mulib_yantlr_options_t *options, mulib_yantlr_error_t *error) {
  mulib_yantlr_run_t *r;
  size_t i, j;
  mulib_yantlr_span_t zero = {0, 0};
  if (error) memset(error, 0, sizeof *error);
  if (!m || (!bindings && count)) {
    mulib_yantlr_error_(error, MULIB_YANTLR_INVALID, NULL, zero, "invalid evaluation arguments"); return NULL;
  }
  r = (mulib_yantlr_run_t *)calloc(1, sizeof *r);
  if (!r) { mulib_yantlr_error_(error, MULIB_YANTLR_MEMORY, NULL, zero, "out of memory"); return NULL; }
  mulib_arena_init(&r->arena, 4096); r->module = m; r->options = mulib_yantlr_options_(options);
  r->bindings = bindings; r->binding_count = count;
  r->globals = (const mulib_yantlr_value_t **)mulib_yantlr_run_alloc_(r, m->count, sizeof *r->globals);
  r->externals = (const mulib_yantlr_value_t **)mulib_yantlr_run_alloc_(r, count, sizeof *r->externals);
  r->marks = (unsigned char *)mulib_yantlr_run_alloc_(r, m->count, 1);
  r->order = (size_t *)mulib_yantlr_run_alloc_(r, m->count, sizeof *r->order);
  if (r->error.code) goto fail;
  for (i = 0; i < count; ++i) {
    const mulib_yantlr_binding_t *b = &bindings[i];
    if (!mulib_yantlr_binding_name_(b->name) || (!!b->function == !!b->constant) ||
        (b->function && !b->arity)) {
      mulib_yantlr_fail(r, MULIB_YANTLR_INVALID, "invalid provider binding"); goto fail;
    }
    for (j = 0; j < i; ++j) if (!strcmp(b->name, bindings[j].name)) break;
    if (j < i || mulib_yantlr_find_(m, b->name) != SIZE_MAX) {
      mulib_yantlr_fail(r, MULIB_YANTLR_DUPLICATE, "duplicate provider or declaration binding"); goto fail;
    }
  }
  for (i = 0; i < m->dependency_count; ++i) {
    const char *name = m->dependencies[i].name;
    for (j = 0; j < count; ++j) if (!strcmp(name, bindings[j].name)) break;
    if (j == count) {
      char message[256];
      r->current = m->dependencies[i].span;
      snprintf(message, sizeof message, "unresolved %s: %s", strchr(name, '[') || strstr(name, "::") ? "service" : "name", name);
      mulib_yantlr_fail(r, strchr(name, '[') || strstr(name, "::") ? MULIB_YANTLR_SERVICE : MULIB_YANTLR_NAME, message);
      goto fail;
    }
  }
  for (i = 0; i < m->count; ++i) if (!mulib_yantlr_link_decl_(r, i)) goto fail;
  memset(r->marks, 0, m->count); r->steps = r->depth = 0;
  for (i = 0; i < m->count; ++i) if (!mulib_yantlr_global_(r, r->order[i])) goto fail;
  /* Force all external values now: retained closures must not borrow registry
   * storage after evaluate returns. lookup then only needs this copied table. */
  for (i = 0; i < count; ++i)
    if (!r->externals[i] && !mulib_yantlr_lookup_(r, bindings[i].name, NULL)) goto fail;
  if (count) {
    mulib_yantlr_binding_t *copy = (mulib_yantlr_binding_t *)mulib_yantlr_run_alloc_(r, count, sizeof *copy);
    if (!copy) goto fail;
    for (i = 0; i < count; ++i) {
      copy[i] = bindings[i]; copy[i].name = mulib_yantlr_run_copy_(r, bindings[i].name, strlen(bindings[i].name));
      copy[i].constant = NULL;
      if (!copy[i].name) goto fail;
    }
    r->bindings = copy;
  }
  return r;
fail:
  if (error) *error = r->error;
  mulib_yantlr_run_destroy(r); return NULL;
}
void mulib_yantlr_run_destroy(mulib_yantlr_run_t *r) {
  mulib_yantlr_value_t *v;
  if (!r) return;
  for (v = r->values; v; v = v->next)
    if (v->type == MULIB_YANTLR_OPAQUE && v->as.opaque.destroy) v->as.opaque.destroy(v->as.opaque.pointer);
  mulib_arena_reset(&r->arena); free(r);
}
const mulib_yantlr_value_t *mulib_yantlr_get(const mulib_yantlr_run_t *r, const char *name) {
  size_t i;
  if (!r || r->error.code || !name) return NULL;
  i = mulib_yantlr_find_(r->module, name); return i == SIZE_MAX ? NULL : r->globals[i];
}
static int mulib_yantlr_output_path_(const char *s) {
  size_t i = 0, start = 0;
  if (!s || !*s) return 0;
  for (;;) {
    unsigned char c = (unsigned char)s[i];
    if (i > 4095 || (c && (c < 32 || c == 127 || c == '\\' || c == ':'))) return 0;
    if (!c || c == '/') {
      size_t n = i - start;
      if (!n || (n == 1 && s[start] == '.') || (n == 2 && s[start] == '.' && s[start + 1] == '.')) return 0;
      if (!c) return 1;
      start = i + 1;
    }
    ++i;
  }
}
int mulib_yantlr_emit(mulib_yantlr_run_t *r, const char *path, const void *data, size_t length) {
  mulib_yantlr_output_ *p, *out;
  void *copy;
  if (!r || r->error.code) return 0;
  if (!mulib_yantlr_output_path_(path) || (!data && length))
    return mulib_yantlr_fail(r, MULIB_YANTLR_OUTPUT, "invalid artifact path or bytes");
  for (p = r->outputs; p; p = p->next)
    if (!strcmp(p->artifact.path, path)) return mulib_yantlr_fail(r, MULIB_YANTLR_OUTPUT, "duplicate artifact path");
  if (length > r->options.max_artifact_bytes - r->output_bytes || r->output_count >= r->options.max_values)
    return mulib_yantlr_fail(r, MULIB_YANTLR_LIMIT, "artifact limit");
  out = (mulib_yantlr_output_ *)mulib_yantlr_run_alloc_(r, 1, sizeof *out);
  copy = mulib_yantlr_run_alloc_(r, length, 1);
  if (!out || !copy) return 0;
  out->artifact.path = mulib_yantlr_run_copy_(r, path, strlen(path)); if (!out->artifact.path) return 0;
  if (length) memcpy(copy, data, length);
  out->artifact.data = copy; out->artifact.length = length;
  if (r->last_output) r->last_output->next = out; else r->outputs = out;
  r->last_output = out; ++r->output_count; r->output_bytes += length; return 1;
}
size_t mulib_yantlr_artifact_count(const mulib_yantlr_run_t *r) { return r && !r->error.code ? r->output_count : 0; }
const mulib_yantlr_artifact_t *mulib_yantlr_artifact(const mulib_yantlr_run_t *r, size_t i) {
  const mulib_yantlr_output_ *p;
  if (!r || r->error.code || i >= r->output_count) return NULL;
  p = r->outputs; while (i--) p = p->next; return &p->artifact;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
