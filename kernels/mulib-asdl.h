/**
 * @file mulib-asdl.h
 * @brief Zephyr ASDL schemas, parsec parser and S-expression/YANTLR/C99 emitters.
 * Define MULIB_IMPL_ASDL, MULIB_IMPL_ARENA, MULIB_IMPL_PARSEC and
 * MULIB_IMPL_STRBUF in one C11 translation unit. Generated C requires only C99.
 * See manual/mulib-asdl.md for the output schemas, naming and ownership rules.
 */
#ifndef MULIB_ASDL_H
#define MULIB_ASDL_H
#include "mulib-parsec.h"
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_asdl_module mulib_asdl_module_t;
typedef struct mulib_asdl_span { size_t offset, length; } mulib_asdl_span_t;
typedef enum mulib_asdl_cardinality {
  MULIB_ASDL_REQUIRED, MULIB_ASDL_OPTIONAL, MULIB_ASDL_SEQUENCE
} mulib_asdl_cardinality_t;
typedef enum mulib_asdl_kind { MULIB_ASDL_PRODUCT, MULIB_ASDL_SUM } mulib_asdl_kind_t;
/** Borrowed declaration views. NULL field name means an unnamed field. */
typedef struct mulib_asdl_field {
  const char *type, *name;
  mulib_asdl_cardinality_t cardinality;
  mulib_asdl_span_t span;
} mulib_asdl_field_t;
typedef struct mulib_asdl_constructor {
  const char *name;
  const mulib_asdl_field_t *fields;
  size_t field_count;
  mulib_asdl_span_t span;
} mulib_asdl_constructor_t;
typedef struct mulib_asdl_type {
  const char *name;
  mulib_asdl_kind_t kind;
  const mulib_asdl_field_t *fields; /**< Products only. */
  size_t field_count;
  const mulib_asdl_constructor_t *constructors; /**< Sums only. */
  size_t constructor_count;
  const mulib_asdl_field_t *attributes; /**< Sums only. */
  size_t attribute_count;
  mulib_asdl_span_t span;
} mulib_asdl_type_t;
typedef enum mulib_asdl_error_code {
  MULIB_ASDL_OK, MULIB_ASDL_INVALID, MULIB_ASDL_SYNTAX, MULIB_ASDL_MEMORY,
  MULIB_ASDL_LIMIT, MULIB_ASDL_DUPLICATE, MULIB_ASDL_NAME, MULIB_ASDL_CYCLE
} mulib_asdl_error_code_t;
typedef struct mulib_asdl_error {
  mulib_asdl_error_code_t code;
  mulib_asdl_span_t span;
  size_t line, column; /**< One-based byte coordinates. */
  char message[192]; /**< Owned, valid after module destruction. */
} mulib_asdl_error_t;
typedef enum mulib_asdl_format {
  MULIB_ASDL_SEXPR, MULIB_ASDL_YANTLR, MULIB_ASDL_C99
} mulib_asdl_format_t;
/** Zero initialization selects S-expressions, module name "asdl", C prefix
 * "asdl" and parsec defaults (except max_steps defaults to 10000000).
 * module_name names bare descriptions; a source module wrapper overrides it.
 * c_prefix must start with an ASCII letter and contain only identifier bytes. */
typedef struct mulib_asdl_options {
  mulib_asdl_format_t format;
  const char *module_name, *c_prefix;
  mulib_parsec_options_t parser;
} mulib_asdl_options_t;

/* Array initializer macros: use COUNT only with real arrays, not pointers. */
#define MULIB_ASDL_COUNT(A) (sizeof(A) / sizeof((A)[0]))
#define MULIB_ASDL_FIELD(T, N) { (T), (N), MULIB_ASDL_REQUIRED, {0, 0} }
#define MULIB_ASDL_OPT(T, N) { (T), (N), MULIB_ASDL_OPTIONAL, {0, 0} }
#define MULIB_ASDL_SEQ(T, N) { (T), (N), MULIB_ASDL_SEQUENCE, {0, 0} }
#define MULIB_ASDL_ATOM(N) { (N), NULL, 0, {0, 0} }
#define MULIB_ASDL_CONSTRUCTOR(N, F) { (N), (F), MULIB_ASDL_COUNT(F), {0, 0} }
#define MULIB_ASDL_PRODUCT_TYPE(N, F) \
  { (N), MULIB_ASDL_PRODUCT, (F), MULIB_ASDL_COUNT(F), NULL, 0, NULL, 0, {0, 0} }
#define MULIB_ASDL_SUM_TYPE(N, C) \
  { (N), MULIB_ASDL_SUM, NULL, 0, (C), MULIB_ASDL_COUNT(C), NULL, 0, {0, 0} }
#define MULIB_ASDL_ATTR_SUM(N, C, A) \
  { (N), MULIB_ASDL_SUM, NULL, 0, (C), MULIB_ASDL_COUNT(C), (A), MULIB_ASDL_COUNT(A), {0, 0} }

static inline mulib_asdl_field_t
mulib_asdl_field(const char *type, const char *name, mulib_asdl_cardinality_t card) {
  mulib_asdl_field_t f = {type, name, card, {0, 0}}; return f;
}
static inline mulib_asdl_constructor_t
mulib_asdl_constructor(const char *name, const mulib_asdl_field_t *fields, size_t count) {
  mulib_asdl_constructor_t c = {name, fields, count, {0, 0}}; return c;
}
static inline mulib_asdl_type_t
mulib_asdl_product(const char *name, const mulib_asdl_field_t *fields, size_t count) {
  mulib_asdl_type_t t = {name, MULIB_ASDL_PRODUCT, fields, count, NULL, 0, NULL, 0, {0, 0}};
  return t;
}
static inline mulib_asdl_type_t
mulib_asdl_sum(const char *name, const mulib_asdl_constructor_t *ctors, size_t count,
    const mulib_asdl_field_t *attributes, size_t attribute_count) {
  mulib_asdl_type_t t = {name, MULIB_ASDL_SUM, NULL, 0, ctors, count,
      attributes, attribute_count, {0, 0}}; return t;
}
/** True exactly for sums without fields or attributes. NULL returns false. */
static inline int mulib_asdl_is_enum(const mulib_asdl_type_t *t) {
  size_t i;
  if (!t || t->kind != MULIB_ASDL_SUM || !t->constructor_count || !t->constructors || t->attribute_count)
    return 0;
  for (i = 0; i < t->constructor_count; ++i)
    if (t->constructors[i].field_count) return 0;
  return 1;
}
/** Entire-input parse and semantic validation; copies all source/name data.
 * NULL source is allowed only at length zero. Empty descriptions are valid.
 * Limits: 16 MiB source, 255 bytes per identifier, 65536 schema objects,
 * 1000000 semantic work units, 64 MiB output, plus parsec limits. */
mulib_asdl_module_t *mulib_asdl_parse(const char *, size_t,
    const mulib_asdl_options_t *, mulib_asdl_error_t *);
/** Deep-copy and validate borrowed API declarations. No input is retained.
 * name may be NULL ("asdl"). Nonzero counts require valid arrays. */
mulib_asdl_module_t *mulib_asdl_from_types(const char *name,
    const mulib_asdl_type_t *, size_t, mulib_asdl_error_t *);
void mulib_asdl_destroy(mulib_asdl_module_t *); /**< NULL is harmless. */
/** Borrow the module name, or NULL. */
const char *mulib_asdl_name(const mulib_asdl_module_t *);
/** Number of definitions; zero for NULL. */
size_t mulib_asdl_type_count(const mulib_asdl_module_t *);
/** Borrow a type in declaration order; NULL for an invalid index. */
const mulib_asdl_type_t *mulib_asdl_type(const mulib_asdl_module_t *, size_t);
/** Borrow a type by exact name; NULL if absent. Builtins have no definition. */
const mulib_asdl_type_t *mulib_asdl_find(const mulib_asdl_module_t *, const char *);
/** Atomic append of NUL-terminated text. Output length excludes the NUL.
 * Returns 1 on success, 0 on error; never writes files or invokes providers.
 * YANTLR output has no external dependencies: intermediary + set Schema record.
 * C99 constructors shallow-copy fields; allocated nodes are caller-owned. */
int mulib_asdl_generate(const mulib_asdl_module_t *, const mulib_asdl_options_t *,
    mulib_strbuf_t *, mulib_asdl_error_t *);
/** Parse, generate and destroy. Source may alias output. */
int mulib_asdl_compile(const char *, size_t, const mulib_asdl_options_t *,
    mulib_strbuf_t *, mulib_asdl_error_t *);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ASDL_H */

#if defined(MULIB_IMPL_ASDL) && !defined(MULIB_ASDL_IMPLEMENTED)
#define MULIB_ASDL_IMPLEMENTED
#include "mulib-arena.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_ASDL_OBJECT_LIMIT_ 65536u
#define MULIB_ASDL_WORK_LIMIT_ 1000000u
#define MULIB_ASDL_OUTPUT_LIMIT_ 67108864u
struct mulib_asdl_module {
  mulib_arena_t arena;
  const char *name, *source;
  size_t length, count, objects, work;
  mulib_asdl_type_t *types;
};
static int
mulib_asdl_error_(mulib_asdl_error_t *e, mulib_asdl_error_code_t code,
    const mulib_asdl_module_t *m, mulib_asdl_span_t span, const char *message) {
  size_t i, length = m ? m->length : 0;
  if (e && !e->code) {
    if (span.offset > length) span.offset = length;
    if (span.length > length - span.offset) span.length = length - span.offset;
    e->code = code; e->span = span; e->line = e->column = 1;
    snprintf(e->message, sizeof e->message, "%s", message);
    for (i = 0; m && m->source && i < span.offset; ++i)
      if (m->source[i] == '\n') { ++e->line; e->column = 1; } else ++e->column;
  }
  return 0;
}
static int
mulib_asdl_work_(mulib_asdl_module_t *m, mulib_asdl_error_t *e) {
  mulib_asdl_span_t z = {0, 0};
  if (m->work++ < MULIB_ASDL_WORK_LIMIT_) return 1;
  return mulib_asdl_error_(e, MULIB_ASDL_LIMIT, m, z, "semantic work limit");
}
static void *
mulib_asdl_alloc_(mulib_asdl_module_t *m, size_t n, size_t size, mulib_asdl_error_t *e) {
  void *p;
  mulib_asdl_span_t z = {0, 0};
  if (n > MULIB_ASDL_OBJECT_LIMIT_ - m->objects || (size && n > SIZE_MAX / size)) {
    mulib_asdl_error_(e, MULIB_ASDL_LIMIT, m, z, "schema object limit"); return NULL;
  }
  p = mulib_arena_alloc(&m->arena, n * size);
  if (!p) { mulib_asdl_error_(e, MULIB_ASDL_MEMORY, m, z, "out of memory"); return NULL; }
  m->objects += n; memset(p, 0, n * size); return p;
}
static char *
mulib_asdl_copy_(mulib_asdl_module_t *m, const char *s, size_t n, mulib_asdl_error_t *e) {
  char *p = (char *)mulib_arena_alloc(&m->arena, n + 1);
  mulib_asdl_span_t z = {0, 0};
  if (!p) { mulib_asdl_error_(e, MULIB_ASDL_MEMORY, m, z, "out of memory"); return NULL; }
  if (n) memcpy(p, s, n);
  p[n] = 0; return p;
}
static int mulib_asdl_lower_(unsigned char c) { return c >= 'a' && c <= 'z'; }
static int mulib_asdl_upper_(unsigned char c) { return c >= 'A' && c <= 'Z'; }
static int mulib_asdl_more_(unsigned char c) {
  return mulib_asdl_lower_(c) || mulib_asdl_upper_(c) || c == '_' || (c >= '0' && c <= '9');
}
static int
mulib_asdl_identifier_(const char *s, int kind) {
  size_t i;
  if (!s || !(kind == 1 ? mulib_asdl_lower_((unsigned char)*s) :
      kind == 2 ? mulib_asdl_upper_((unsigned char)*s) :
      mulib_asdl_lower_((unsigned char)*s) || mulib_asdl_upper_((unsigned char)*s))) return 0;
  for (i = 1; i <= 255 && s[i]; ++i)
    if (!mulib_asdl_more_((unsigned char)s[i])) return 0;
  return i <= 255;
}
static const char *
mulib_asdl_name_copy_(mulib_asdl_module_t *m, const char *s, int kind,
    mulib_asdl_span_t span, mulib_asdl_error_t *e) {
  if (!mulib_asdl_identifier_(s, kind)) {
    mulib_asdl_error_(e, MULIB_ASDL_NAME, m, span, "invalid identifier (maximum 255 bytes)");
    return NULL;
  }
  return mulib_asdl_copy_(m, s, strlen(s), e);
}
static int mulib_asdl_builtin_(const char *s) {
  return !strcmp(s, "int") || !strcmp(s, "string") || !strcmp(s, "identifier");
}
static size_t
mulib_asdl_lookup_(mulib_asdl_module_t *m, const char *s, mulib_asdl_error_t *e) {
  size_t i;
  for (i = 0; i < m->count; ++i) {
    if (!mulib_asdl_work_(m, e)) return SIZE_MAX;
    if (!strcmp(m->types[i].name, s)) return i;
  }
  return SIZE_MAX;
}
static int
mulib_asdl_fields_check_(mulib_asdl_module_t *m, const mulib_asdl_field_t *f, size_t n,
    const mulib_asdl_field_t *attrs, size_t na, mulib_asdl_error_t *e) {
  size_t i, j;
  for (i = 0; i < n; ++i) {
    if (!mulib_asdl_work_(m, e)) return 0;
    if (!mulib_asdl_builtin_(f[i].type) && mulib_asdl_lookup_(m, f[i].type, e) == SIZE_MAX) {
      return mulib_asdl_error_(e, MULIB_ASDL_NAME, m, f[i].span, "undefined field type");
    }
    if (!f[i].name) continue;
    for (j = 0; j < i + na; ++j) {
      const char *name = j < i ? f[j].name : attrs[j - i].name;
      if (!mulib_asdl_work_(m, e)) return 0;
      if (name && !strcmp(f[i].name, name))
        return mulib_asdl_error_(e, MULIB_ASDL_DUPLICATE, m, f[i].span, "duplicate field or attribute name");
    }
  }
  return !e->code;
}
static int
mulib_asdl_validate_(mulib_asdl_module_t *m, mulib_asdl_error_t *e) {
  size_t i, j, k, l, top, target;
  unsigned char *marks = NULL;
  size_t *stack = NULL, *next = NULL;
  mulib_asdl_span_t z = {0, 0};
  int ok = 0;
  for (i = 0; i < m->count; ++i) {
    const mulib_asdl_type_t *t = &m->types[i];
    if (mulib_asdl_builtin_(t->name) || !strcmp(t->name, "attributes") || !strcmp(t->name, "module"))
      return mulib_asdl_error_(e, MULIB_ASDL_NAME, m, t->span, "reserved type name");
    for (j = 0; j < i; ++j) {
      if (!mulib_asdl_work_(m, e)) return 0;
      if (!strcmp(t->name, m->types[j].name))
        return mulib_asdl_error_(e, MULIB_ASDL_DUPLICATE, m, t->span, "duplicate type name");
    }
    if (!mulib_asdl_fields_check_(m, t->fields, t->field_count, NULL, 0, e) ||
        !mulib_asdl_fields_check_(m, t->attributes, t->attribute_count, NULL, 0, e)) return 0;
    for (j = 0; j < t->constructor_count; ++j) {
      const mulib_asdl_constructor_t *c = &t->constructors[j];
      if (!mulib_asdl_fields_check_(m, c->fields, c->field_count, t->attributes, t->attribute_count, e)) return 0;
      for (k = 0; k <= i; ++k)
        for (l = 0; l < (k == i ? j : m->types[k].constructor_count); ++l) {
          if (!mulib_asdl_work_(m, e)) return 0;
          if (!strcmp(c->name, m->types[k].constructors[l].name))
            return mulib_asdl_error_(e, MULIB_ASDL_DUPLICATE, m, c->span, "duplicate constructor name");
        }
    }
  }
  /* Only cycles consisting entirely of products are forbidden. Crossing a sum
   * is a legitimate recursive algebraic type. Qualifiers do not hide cycles. */
  if (!m->count) return 1;
  marks = (unsigned char *)calloc(m->count, 1);
  stack = (size_t *)malloc(m->count * sizeof *stack);
  next = (size_t *)calloc(m->count, sizeof *next);
  if (!marks || !stack || !next) {
    mulib_asdl_error_(e, MULIB_ASDL_MEMORY, m, z, "out of memory"); goto done;
  }
  for (i = 0; i < m->count; ++i) {
    if (marks[i] || m->types[i].kind != MULIB_ASDL_PRODUCT) continue;
    top = 1; stack[0] = i; marks[i] = 1;
    while (top) {
      const mulib_asdl_type_t *t = &m->types[stack[top - 1]];
      const mulib_asdl_field_t *f;
      j = stack[top - 1];
      if (next[j] == t->field_count) { marks[j] = 2; --top; continue; }
      f = &t->fields[next[j]++];
      if (mulib_asdl_builtin_(f->type)) continue;
      target = mulib_asdl_lookup_(m, f->type, e);
      if (e->code) goto done;
      if (m->types[target].kind != MULIB_ASDL_PRODUCT || marks[target] == 2) continue;
      if (marks[target] == 1) {
        mulib_asdl_error_(e, MULIB_ASDL_CYCLE, m, f->span, "recursive product type"); goto done;
      }
      marks[target] = 1; stack[top++] = target;
    }
  }
  ok = 1;
done:
  free(marks); free(stack); free(next); return ok;
}
static mulib_asdl_field_t *
mulib_asdl_fields_copy_(mulib_asdl_module_t *m, const mulib_asdl_field_t *f,
    size_t n, mulib_asdl_error_t *e) {
  mulib_asdl_field_t *a;
  size_t i;
  mulib_asdl_span_t z = {0, 0};
  if (!f && n) { mulib_asdl_error_(e, MULIB_ASDL_INVALID, m, z, "null field array"); return NULL; }
  a = (mulib_asdl_field_t *)mulib_asdl_alloc_(m, n, sizeof *a, e);
  if (!a) return NULL;
  for (i = 0; i < n; ++i) {
    a[i] = f[i];
    a[i].type = mulib_asdl_name_copy_(m, f[i].type, 1, f[i].span, e);
    if (!a[i].type) return NULL;
    if (f[i].name && !(a[i].name = mulib_asdl_name_copy_(m, f[i].name, 0, f[i].span, e))) return NULL;
    if (f[i].cardinality < MULIB_ASDL_REQUIRED || f[i].cardinality > MULIB_ASDL_SEQUENCE) {
      mulib_asdl_error_(e, MULIB_ASDL_INVALID, m, f[i].span, "invalid field cardinality"); return NULL;
    }
  }
  return a;
}
void mulib_asdl_destroy(mulib_asdl_module_t *m) {
  if (m) { mulib_arena_reset(&m->arena); free(m); }
}
const char *mulib_asdl_name(const mulib_asdl_module_t *m) { return m ? m->name : NULL; }
size_t mulib_asdl_type_count(const mulib_asdl_module_t *m) { return m ? m->count : 0; }
const mulib_asdl_type_t *mulib_asdl_type(const mulib_asdl_module_t *m, size_t i) {
  return m && i < m->count ? &m->types[i] : NULL;
}
const mulib_asdl_type_t *mulib_asdl_find(const mulib_asdl_module_t *m, const char *s) {
  size_t i;
  if (m && s) for (i = 0; i < m->count; ++i)
    if (!strcmp(m->types[i].name, s)) return &m->types[i];
  return NULL;
}
mulib_asdl_module_t *
mulib_asdl_from_types(const char *name, const mulib_asdl_type_t *types,
    size_t count, mulib_asdl_error_t *error) {
  mulib_asdl_error_t local;
  mulib_asdl_module_t *m;
  mulib_asdl_span_t z = {0, 0};
  size_t i, j;
  if (!error) error = &local;
  memset(error, 0, sizeof *error);
  if (!types && count) {
    mulib_asdl_error_(error, MULIB_ASDL_INVALID, NULL, z, "null type array"); return NULL;
  }
  m = (mulib_asdl_module_t *)calloc(1, sizeof *m);
  if (!m) { mulib_asdl_error_(error, MULIB_ASDL_MEMORY, NULL, z, "out of memory"); return NULL; }
  mulib_arena_init(&m->arena, 4096);
  m->name = mulib_asdl_name_copy_(m, name ? name : "asdl", 0, z, error);
  m->types = (mulib_asdl_type_t *)mulib_asdl_alloc_(m, count, sizeof *m->types, error);
  if (!m->name || !m->types) goto fail;
  m->count = count;
  for (i = 0; i < count; ++i) {
    const mulib_asdl_type_t *s = &types[i];
    mulib_asdl_type_t *t = &m->types[i];
    mulib_asdl_constructor_t *cs;
    *t = *s;
    if ((t->kind == MULIB_ASDL_PRODUCT && (!t->field_count || t->constructor_count || t->attribute_count)) ||
        (t->kind == MULIB_ASDL_SUM && (t->field_count || !t->constructor_count)) ||
        (t->kind != MULIB_ASDL_PRODUCT && t->kind != MULIB_ASDL_SUM) ||
        (!t->constructors && t->constructor_count)) {
      mulib_asdl_error_(error, MULIB_ASDL_INVALID, m, t->span, "invalid product/sum declaration"); goto fail;
    }
    t->name = mulib_asdl_name_copy_(m, s->name, 1, s->span, error);
    t->fields = mulib_asdl_fields_copy_(m, s->fields, s->field_count, error);
    t->attributes = mulib_asdl_fields_copy_(m, s->attributes, s->attribute_count, error);
    cs = (mulib_asdl_constructor_t *)mulib_asdl_alloc_(m, s->constructor_count, sizeof *cs, error);
    t->constructors = cs;
    if (!t->name || !t->fields || !t->attributes || !cs) goto fail;
    for (j = 0; j < s->constructor_count; ++j) {
      cs[j] = s->constructors[j];
      cs[j].name = mulib_asdl_name_copy_(m, cs[j].name, 2, cs[j].span, error);
      cs[j].fields = mulib_asdl_fields_copy_(m, cs[j].fields, cs[j].field_count, error);
      if (!cs[j].name || !cs[j].fields) goto fail;
    }
  }
  if (!mulib_asdl_validate_(m, error)) goto fail;
  return m;
fail:
  mulib_asdl_destroy(m); return NULL;
}

/* The complete concrete grammar is a parsec graph. Captures are folded in
 * postorder; there is no second, more permissive ASDL recognizer. */
enum mulib_asdl_capture_ {
  MULIB_ASDL_ID_, MULIB_ASDL_QUAL_, MULIB_ASDL_FIELD_, MULIB_ASDL_FIELDS_,
  MULIB_ASDL_CTOR_, MULIB_ASDL_ATTR_, MULIB_ASDL_SUM_, MULIB_ASDL_TYPE_, MULIB_ASDL_DOC_
};
typedef struct mulib_asdl_node_ {
  size_t tag;
  mulib_asdl_span_t span;
  const char *text;
  struct mulib_asdl_node_ *child, *next;
} mulib_asdl_node_;
static int mulib_asdl_pred_(unsigned char c, void *u) {
  switch (*(const int *)u) {
  case 0: return mulib_asdl_more_(c);
  case 1: return mulib_asdl_lower_(c);
  case 2: return mulib_asdl_upper_(c);
  case 3: return mulib_asdl_lower_(c) || mulib_asdl_upper_(c);
  case 4: return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
  default: return c != '\r' && c != '\n' && c != 0;
  }
}
static int
mulib_asdl_recognize_(const char *s, size_t n, const mulib_parsec_options_t *o,
    mulib_parsec_result_t *r) {
  static const int kinds[] = {0, 1, 2, 3, 4, 5};
  mulib_parsec_parser_t more = mulib_parsec_satisfy(mulib_asdl_pred_, (void *)&kinds[0], "identifier byte");
  mulib_parsec_parser_t tail = mulib_parsec_repeat(&more, 0, SIZE_MAX);
  mulib_parsec_parser_t low[] = {mulib_parsec_satisfy(mulib_asdl_pred_, (void *)&kinds[1], "lowercase type"), tail};
  mulib_parsec_parser_t up[] = {mulib_parsec_satisfy(mulib_asdl_pred_, (void *)&kinds[2], "uppercase constructor"), tail};
  mulib_parsec_parser_t id[] = {mulib_parsec_satisfy(mulib_asdl_pred_, (void *)&kinds[3], "identifier"), tail};
  mulib_parsec_parser_t lowp = mulib_parsec_sequence(low, 2), upp = mulib_parsec_sequence(up, 2);
  mulib_parsec_parser_t idp = mulib_parsec_sequence(id, 2);
  mulib_parsec_parser_t lowcap = mulib_parsec_capture(&lowp, MULIB_ASDL_ID_);
  mulib_parsec_parser_t upcap = mulib_parsec_capture(&upp, MULIB_ASDL_ID_);
  mulib_parsec_parser_t idcap = mulib_parsec_capture(&idp, MULIB_ASDL_ID_);
  mulib_parsec_parser_t linebyte = mulib_parsec_satisfy(mulib_asdl_pred_, (void *)&kinds[5], "comment byte");
  mulib_parsec_parser_t line = mulib_parsec_repeat(&linebyte, 0, SIZE_MAX);
  mulib_parsec_parser_t starts[] = {mulib_parsec_literal("--", 2), mulib_parsec_literal("//", 2)};
  mulib_parsec_parser_t lineparts[] = {mulib_parsec_choice(starts, 2), line};
  mulib_parsec_parser_t close = mulib_parsec_literal("*/", 2);
  /* Block comments may include line endings, but not NUL. */
  mulib_parsec_parser_t blockbyte[] = {mulib_parsec_lookahead(&close, 0), mulib_parsec_range(1, 255)};
  mulib_parsec_parser_t blockunit, blockparts[3], blockrepeat;
  mulib_parsec_parser_t wsitems[3], wschoice, ws;
  mulib_parsec_parser_t qualitems[] = {mulib_parsec_byte('?'), mulib_parsec_byte('*')};
  mulib_parsec_parser_t qual = mulib_parsec_choice(qualitems, 2);
  mulib_parsec_parser_t qualcap = mulib_parsec_capture(&qual, MULIB_ASDL_QUAL_);
  mulib_parsec_parser_t fieldparts[6], fieldseq, fieldcap, commafield[3], commafieldseq, fieldsitems[6], fieldsseq, fieldscap;
  mulib_parsec_parser_t ctoritems[3], ctorseq, ctorcap, barctor[3], barctorseq;
  mulib_parsec_parser_t attritems[4], attrseq, attrcap, sumitems[3], sumseq, sumcap;
  mulib_parsec_parser_t choices[2], typeitems[5], typeseq, typecap, defs;
  mulib_parsec_parser_t moduleitems[9], module, documents[2], docitems[3], docseq, doc;
  blockunit = mulib_parsec_sequence(blockbyte, 2);
  blockrepeat = mulib_parsec_repeat(&blockunit, 0, SIZE_MAX);
  blockparts[0] = mulib_parsec_literal("/*", 2); blockparts[1] = blockrepeat; blockparts[2] = close;
  wsitems[0] = mulib_parsec_satisfy(mulib_asdl_pred_, (void *)&kinds[4], "whitespace");
  wsitems[1] = mulib_parsec_sequence(lineparts, 2); wsitems[2] = mulib_parsec_sequence(blockparts, 3);
  wschoice = mulib_parsec_choice(wsitems, 3); ws = mulib_parsec_repeat(&wschoice, 0, SIZE_MAX);
  fieldparts[0] = lowcap; fieldparts[1] = ws; fieldparts[2] = mulib_parsec_repeat(&qualcap, 0, 1);
  fieldparts[3] = ws; fieldparts[4] = mulib_parsec_repeat(&idcap, 0, 1); fieldparts[5] = ws;
  fieldseq = mulib_parsec_sequence(fieldparts, 6); fieldcap = mulib_parsec_capture(&fieldseq, MULIB_ASDL_FIELD_);
  commafield[0] = mulib_parsec_byte(','); commafield[1] = ws; commafield[2] = fieldcap;
  commafieldseq = mulib_parsec_sequence(commafield, 3);
  fieldsitems[0] = mulib_parsec_byte('('); fieldsitems[1] = ws; fieldsitems[2] = fieldcap;
  fieldsitems[3] = mulib_parsec_repeat(&commafieldseq, 0, SIZE_MAX);
  fieldsitems[4] = mulib_parsec_byte(')'); fieldsitems[5] = ws;
  fieldsseq = mulib_parsec_sequence(fieldsitems, 6); fieldscap = mulib_parsec_capture(&fieldsseq, MULIB_ASDL_FIELDS_);
  ctoritems[0] = upcap; ctoritems[1] = ws; ctoritems[2] = mulib_parsec_repeat(&fieldscap, 0, 1);
  ctorseq = mulib_parsec_sequence(ctoritems, 3); ctorcap = mulib_parsec_capture(&ctorseq, MULIB_ASDL_CTOR_);
  barctor[0] = mulib_parsec_byte('|'); barctor[1] = ws; barctor[2] = ctorcap;
  barctorseq = mulib_parsec_sequence(barctor, 3);
  attritems[0] = mulib_parsec_literal("attributes", 10); attritems[1] = mulib_parsec_lookahead(&more, 0);
  attritems[2] = ws; attritems[3] = fieldscap;
  attrseq = mulib_parsec_sequence(attritems, 4); attrcap = mulib_parsec_capture(&attrseq, MULIB_ASDL_ATTR_);
  sumitems[0] = ctorcap; sumitems[1] = mulib_parsec_repeat(&barctorseq, 0, SIZE_MAX);
  sumitems[2] = mulib_parsec_repeat(&attrcap, 0, 1);
  sumseq = mulib_parsec_sequence(sumitems, 3); sumcap = mulib_parsec_capture(&sumseq, MULIB_ASDL_SUM_);
  choices[0] = fieldscap; choices[1] = sumcap;
  typeitems[0] = lowcap; typeitems[1] = ws; typeitems[2] = mulib_parsec_byte('=');
  typeitems[3] = ws; typeitems[4] = mulib_parsec_choice(choices, 2);
  typeseq = mulib_parsec_sequence(typeitems, 5); typecap = mulib_parsec_capture(&typeseq, MULIB_ASDL_TYPE_);
  defs = mulib_parsec_repeat(&typecap, 0, SIZE_MAX);
  moduleitems[0] = mulib_parsec_literal("module", 6); moduleitems[1] = mulib_parsec_lookahead(&more, 0);
  moduleitems[2] = ws; moduleitems[3] = idcap; moduleitems[4] = ws;
  moduleitems[5] = mulib_parsec_byte('{'); moduleitems[6] = ws; moduleitems[7] = defs;
  moduleitems[8] = mulib_parsec_byte('}'); module = mulib_parsec_sequence(moduleitems, 9);
  documents[0] = module; documents[1] = defs;
  docitems[0] = ws; docitems[1] = mulib_parsec_choice(documents, 2); docitems[2] = ws;
  docseq = mulib_parsec_sequence(docitems, 3); doc = mulib_parsec_capture(&docseq, MULIB_ASDL_DOC_);
  {
    mulib_parsec_parser_t whole[] = {doc, mulib_parsec_end()};
    mulib_parsec_parser_t p = mulib_parsec_sequence(whole, 2);
    return mulib_parsec_run(&p, s, n, o, r);
  }
}
static size_t mulib_asdl_nodes_(const mulib_asdl_node_ *n) {
  size_t count = 0; for (; n; n = n->next) ++count; return count;
}
static mulib_asdl_field_t *
mulib_asdl_read_fields_(mulib_asdl_module_t *m, const mulib_asdl_node_ *node,
    size_t *count, mulib_asdl_error_t *e) {
  const mulib_asdl_node_ *n, *c;
  mulib_asdl_field_t *f;
  size_t i = 0;
  *count = node ? mulib_asdl_nodes_(node->child) : 0;
  f = (mulib_asdl_field_t *)mulib_asdl_alloc_(m, *count, sizeof *f, e);
  if (!f) return NULL;
  for (n = node ? node->child : NULL; n; n = n->next, ++i) {
    f[i].span = n->span; f[i].type = n->child->text; c = n->child->next;
    if (c && c->tag == MULIB_ASDL_QUAL_) {
      f[i].cardinality = *c->text == '?' ? MULIB_ASDL_OPTIONAL : MULIB_ASDL_SEQUENCE;
      c = c->next;
    }
    f[i].name = c ? c->text : NULL;
  }
  return f;
}
mulib_asdl_module_t *
mulib_asdl_parse(const char *source, size_t length, const mulib_asdl_options_t *options,
    mulib_asdl_error_t *error) {
  mulib_asdl_error_t local;
  mulib_asdl_module_t *m;
  mulib_parsec_options_t o = {0, 10000000, MULIB_ASDL_OBJECT_LIMIT_};
  mulib_parsec_result_t r;
  mulib_asdl_node_ **stack = NULL, *n, *root, *c, *body;
  size_t *indices = NULL, i, j, base, used = 0;
  mulib_asdl_span_t z = {0, 0};
  int status;
  if (!error) error = &local;
  memset(error, 0, sizeof *error);
  if ((!source && length) || length > 16777216u) {
    mulib_asdl_error_(error, length > 16777216u ? MULIB_ASDL_LIMIT : MULIB_ASDL_INVALID,
        NULL, z, "invalid or oversized source"); return NULL;
  }
  m = (mulib_asdl_module_t *)calloc(1, sizeof *m);
  if (!m) { mulib_asdl_error_(error, MULIB_ASDL_MEMORY, NULL, z, "out of memory"); return NULL; }
  mulib_arena_init(&m->arena, 4096); m->length = length;
  m->source = mulib_asdl_copy_(m, source, length, error);
  if (!m->source) goto fail;
  if (options) {
    o = options->parser;
    if (!o.max_steps) o.max_steps = 10000000;
    if (!o.max_captures || o.max_captures > MULIB_ASDL_OBJECT_LIMIT_) o.max_captures = MULIB_ASDL_OBJECT_LIMIT_;
  }
  status = mulib_asdl_recognize_(m->source, length, &o, &r);
  if (status != 1) {
    z.offset = r.error.offset;
    mulib_asdl_error_(error, r.error.code == MULIB_PARSEC_MEMORY ? MULIB_ASDL_MEMORY :
        r.error.code == MULIB_PARSEC_LIMIT ? MULIB_ASDL_LIMIT : MULIB_ASDL_SYNTAX,
        m, z, "input does not match Zephyr ASDL grammar");
    mulib_parsec_result_free(&r); goto fail;
  }
  stack = (mulib_asdl_node_ **)malloc(r.count * sizeof *stack);
  indices = (size_t *)malloc(r.count * sizeof *indices);
  if (!stack || !indices) {
    mulib_asdl_error_(error, MULIB_ASDL_MEMORY, m, z, "out of memory"); goto parsed_fail;
  }
  for (i = 0; i < r.count; ++i) {
    const mulib_parsec_capture_t *cap = &r.captures[i];
    n = (mulib_asdl_node_ *)mulib_asdl_alloc_(m, 1, sizeof *n, error);
    if (!n) goto parsed_fail;
    n->tag = cap->tag; n->span.offset = cap->offset; n->span.length = cap->length;
    base = used;
    while (base && indices[base - 1] >= cap->first) --base;
    if (base < used) n->child = stack[base];
    for (j = base; j + 1 < used; ++j) stack[j]->next = stack[j + 1];
    used = base; stack[used] = n; indices[used++] = i;
    if (cap->tag == MULIB_ASDL_ID_ || cap->tag == MULIB_ASDL_QUAL_) {
      if (cap->length > 255) {
        mulib_asdl_error_(error, MULIB_ASDL_NAME, m, n->span, "identifier exceeds 255 bytes"); goto parsed_fail;
      }
      n->text = mulib_asdl_copy_(m, m->source + cap->offset, cap->length, error);
      if (!n->text) goto parsed_fail;
    }
  }
  root = stack[0]; c = root->child;
  m->name = c && c->tag == MULIB_ASDL_ID_ ? c->text :
      mulib_asdl_name_copy_(m, options && options->module_name ? options->module_name : "asdl", 0, z, error);
  if (c && c->tag == MULIB_ASDL_ID_) c = c->next;
  m->count = mulib_asdl_nodes_(c);
  m->types = (mulib_asdl_type_t *)mulib_asdl_alloc_(m, m->count, sizeof *m->types, error);
  if (!m->name || !m->types) goto parsed_fail;
  for (i = 0; c; c = c->next, ++i) {
    mulib_asdl_type_t *t = &m->types[i];
    t->name = c->child->text; t->span = c->span; body = c->child->next;
    if (body->tag == MULIB_ASDL_FIELDS_) {
      t->kind = MULIB_ASDL_PRODUCT;
      t->fields = mulib_asdl_read_fields_(m, body, &t->field_count, error);
      if (!t->fields) goto parsed_fail;
    } else {
      mulib_asdl_constructor_t *cs;
      t->kind = MULIB_ASDL_SUM;
      for (n = body->child; n && n->tag == MULIB_ASDL_CTOR_; n = n->next) ++t->constructor_count;
      cs = (mulib_asdl_constructor_t *)mulib_asdl_alloc_(m, t->constructor_count, sizeof *cs, error);
      t->constructors = cs;
      if (!cs) goto parsed_fail;
      for (n = body->child, j = 0; j < t->constructor_count; n = n->next, ++j) {
        cs[j].name = n->child->text; cs[j].span = n->span;
        cs[j].fields = mulib_asdl_read_fields_(m, n->child->next, &cs[j].field_count, error);
        if (!cs[j].fields) goto parsed_fail;
      }
      if (n) {
        t->attributes = mulib_asdl_read_fields_(m, n->child, &t->attribute_count, error);
        if (!t->attributes) goto parsed_fail;
      }
    }
  }
  if (!mulib_asdl_validate_(m, error)) goto parsed_fail;
  free(stack); free(indices); mulib_parsec_result_free(&r); return m;
parsed_fail:
  free(stack); free(indices); mulib_parsec_result_free(&r);
fail:
  mulib_asdl_destroy(m); return NULL;
}

/* Bounded staging makes all three backends transactional. Names contain only
 * identifier bytes, so the shared quoted-name writer needs no escaping. */
typedef struct mulib_asdl_writer_ {
  mulib_strbuf_t text;
  mulib_asdl_error_t *error;
} mulib_asdl_writer_;
static int mulib_asdl_put_(mulib_asdl_writer_ *w, const char *s) {
  size_t n = strlen(s);
  mulib_asdl_span_t z = {0, 0};
  if (w->error->code) return 0;
  if (n > MULIB_ASDL_OUTPUT_LIMIT_ - w->text.length)
    return mulib_asdl_error_(w->error, MULIB_ASDL_LIMIT, NULL, z, "output byte limit");
  if (!mulib_strbuf_append_n(&w->text, s, n))
    return mulib_asdl_error_(w->error, MULIB_ASDL_MEMORY, NULL, z, "out of memory");
  return 1;
}
static void mulib_asdl_number_(mulib_asdl_writer_ *w, size_t n) {
  char s[32]; snprintf(s, sizeof s, "%zu", n); mulib_asdl_put_(w, s);
}
static void mulib_asdl_quote_(mulib_asdl_writer_ *w, const char *s) {
  mulib_asdl_put_(w, "\""); mulib_asdl_put_(w, s); mulib_asdl_put_(w, "\"");
}
static const char *mulib_asdl_card_(mulib_asdl_cardinality_t c) {
  return c == MULIB_ASDL_OPTIONAL ? "optional" : c == MULIB_ASDL_SEQUENCE ? "sequence" : "required";
}
static void
mulib_asdl_emit_fields_(mulib_asdl_writer_ *w, const mulib_asdl_field_t *f, size_t n, int y1) {
  size_t i;
  mulib_asdl_put_(w, y1 ? "[" : "(fields");
  for (i = 0; i < n; ++i) {
    mulib_asdl_put_(w, y1 ? (i ? ", {\"type\": " : "{\"type\": ") : " (field ");
    mulib_asdl_quote_(w, f[i].type);
    mulib_asdl_put_(w, y1 ? ", \"name\": " : " ");
    if (f[i].name) mulib_asdl_quote_(w, f[i].name); else mulib_asdl_put_(w, y1 ? "null" : "()");
    mulib_asdl_put_(w, y1 ? ", \"cardinality\": " : " ");
    mulib_asdl_quote_(w, mulib_asdl_card_(f[i].cardinality));
    mulib_asdl_put_(w, y1 ? "}" : ")");
  }
  mulib_asdl_put_(w, y1 ? "]" : ")");
}
static void
mulib_asdl_emit_schema_(mulib_asdl_writer_ *w, const mulib_asdl_module_t *m, int y1) {
  size_t i, j;
  if (y1) {
    mulib_asdl_put_(w, "intermediary "); mulib_asdl_quote_(w, m->name);
    mulib_asdl_put_(w, ";\nset Schema {\"name\": "); mulib_asdl_quote_(w, m->name);
    mulib_asdl_put_(w, ", \"types\": [");
  } else { mulib_asdl_put_(w, "(asdl "); mulib_asdl_quote_(w, m->name); }
  for (i = 0; i < m->count; ++i) {
    const mulib_asdl_type_t *t = &m->types[i];
    const char *kind = t->kind == MULIB_ASDL_PRODUCT ? "product" : "sum";
    if (y1) {
      mulib_asdl_put_(w, i ? ",\n  {\"kind\": " : "\n  {\"kind\": "); mulib_asdl_quote_(w, kind);
      mulib_asdl_put_(w, ", \"name\": "); mulib_asdl_quote_(w, t->name);
    } else {
      mulib_asdl_put_(w, "\n  ("); mulib_asdl_put_(w, kind); mulib_asdl_put_(w, " ");
      mulib_asdl_quote_(w, t->name);
    }
    if (t->kind == MULIB_ASDL_PRODUCT) {
      mulib_asdl_put_(w, y1 ? ", \"fields\": " : " ");
      mulib_asdl_emit_fields_(w, t->fields, t->field_count, y1);
    } else {
      mulib_asdl_put_(w, y1 ? ", \"constructors\": [" : " (constructors");
      for (j = 0; j < t->constructor_count; ++j) {
        const mulib_asdl_constructor_t *c = &t->constructors[j];
        mulib_asdl_put_(w, y1 ? (j ? ", {\"name\": " : "{\"name\": ") : " (constructor ");
        mulib_asdl_quote_(w, c->name);
        mulib_asdl_put_(w, y1 ? ", \"tag\": " : " "); mulib_asdl_number_(w, j + 1);
        mulib_asdl_put_(w, y1 ? ", \"fields\": " : " ");
        mulib_asdl_emit_fields_(w, c->fields, c->field_count, y1);
        mulib_asdl_put_(w, y1 ? "}" : ")");
      }
      mulib_asdl_put_(w, y1 ? "], \"attributes\": " : ") (attributes ");
      mulib_asdl_emit_fields_(w, t->attributes, t->attribute_count, y1);
      if (!y1) mulib_asdl_put_(w, ")");
    }
    mulib_asdl_put_(w, y1 ? "}" : ")");
  }
  mulib_asdl_put_(w, y1 ? "]};\n" : ")\n");
}

/* Injective C identifier encoding: '_' -> '_u'. Category prefixes and a
 * leading letter prevent keyword/reserved-identifier and unnamed-field clashes. */
static void mulib_asdl_cid_(mulib_asdl_writer_ *w, const char *s) {
  char byte[2] = {0, 0};
  for (; *s && !w->error->code; ++s) {
    byte[0] = *s; mulib_asdl_put_(w, *s == '_' ? "_u" : byte);
  }
}
static void mulib_asdl_cname_(mulib_asdl_writer_ *w, const char *prefix,
    const char *category, const char *name) {
  mulib_asdl_cid_(w, prefix); mulib_asdl_put_(w, category); mulib_asdl_cid_(w, name);
}
static void mulib_asdl_ctype_(mulib_asdl_writer_ *w, const char *p, const mulib_asdl_field_t *f) {
  mulib_asdl_cname_(w, p, f->cardinality == MULIB_ASDL_SEQUENCE ? "_seq_" :
      f->cardinality == MULIB_ASDL_OPTIONAL ? "_opt_" : "_t_", f->type);
}
static void mulib_asdl_cfield_(mulib_asdl_writer_ *w, const mulib_asdl_field_t *f,
    size_t i, int attr) {
  mulib_asdl_put_(w, attr ? "a_" : "f_");
  if (f->name) mulib_asdl_cid_(w, f->name); else mulib_asdl_number_(w, i + 1);
}
static void mulib_asdl_cfields_(mulib_asdl_writer_ *w, const char *p,
    const mulib_asdl_field_t *f, size_t n, int attr) {
  size_t i;
  for (i = 0; i < n; ++i) {
    mulib_asdl_put_(w, "    "); mulib_asdl_ctype_(w, p, &f[i]); mulib_asdl_put_(w, " ");
    mulib_asdl_cfield_(w, &f[i], i, attr); mulib_asdl_put_(w, ";\n");
  }
}
static void mulib_asdl_ctag_(mulib_asdl_writer_ *w, const char *p,
    const mulib_asdl_type_t *t, const char *ctor) {
  mulib_asdl_cname_(w, p, "_tag_", t->name); mulib_asdl_put_(w, "_"); mulib_asdl_cid_(w, ctor);
}
static void mulib_asdl_cenum_(mulib_asdl_writer_ *w, const char *p,
    const mulib_asdl_type_t *t, int simple) {
  size_t j;
  mulib_asdl_put_(w, simple ? "typedef enum " : "enum ");
  mulib_asdl_cname_(w, p, simple ? "_t_" : "_kind_", t->name); mulib_asdl_put_(w, " {\n");
  for (j = 0; j < t->constructor_count; ++j) {
    mulib_asdl_put_(w, "  "); mulib_asdl_ctag_(w, p, t, t->constructors[j].name);
    mulib_asdl_put_(w, " = "); mulib_asdl_number_(w, j + 1);
    mulib_asdl_put_(w, j + 1 < t->constructor_count ? ",\n" : "\n");
  }
  mulib_asdl_put_(w, "}");
  if (simple) { mulib_asdl_put_(w, " "); mulib_asdl_cname_(w, p, "_t_", t->name); }
  mulib_asdl_put_(w, ";\n");
}
static void mulib_asdl_cwrappers_(mulib_asdl_writer_ *w, const char *p, const char *name) {
  mulib_asdl_put_(w, "typedef struct { size_t count; "); mulib_asdl_cname_(w, p, "_t_", name);
  mulib_asdl_put_(w, " *items; } "); mulib_asdl_cname_(w, p, "_seq_", name); mulib_asdl_put_(w, ";\n");
  mulib_asdl_put_(w, "typedef struct { int present; "); mulib_asdl_cname_(w, p, "_t_", name);
  mulib_asdl_put_(w, " value; } "); mulib_asdl_cname_(w, p, "_opt_", name); mulib_asdl_put_(w, ";\n");
}
static void mulib_asdl_cmake_(mulib_asdl_writer_ *w, const char *p,
    const mulib_asdl_type_t *t, const mulib_asdl_constructor_t *ctor) {
  const mulib_asdl_field_t *f = ctor ? ctor->fields : t->fields;
  size_t n = ctor ? ctor->field_count : t->field_count, i;
  int simple = mulib_asdl_is_enum(t);
  mulib_asdl_put_(w, "static inline "); mulib_asdl_cname_(w, p, "_t_", t->name);
  mulib_asdl_put_(w, " "); mulib_asdl_cname_(w, p, "_make_", t->name);
  if (ctor) { mulib_asdl_put_(w, "_"); mulib_asdl_cid_(w, ctor->name); }
  mulib_asdl_put_(w, "(");
  for (i = 0; i < n + t->attribute_count; ++i) {
    int attr = i >= n;
    size_t at = attr ? i - n : i;
    const mulib_asdl_field_t *field = attr ? &t->attributes[at] : &f[at];
    if (i) mulib_asdl_put_(w, ", ");
    mulib_asdl_ctype_(w, p, field); mulib_asdl_put_(w, " "); mulib_asdl_cfield_(w, field, at, attr);
  }
  if (!n && !t->attribute_count) mulib_asdl_put_(w, "void");
  mulib_asdl_put_(w, ") {\n");
  if (simple) {
    mulib_asdl_put_(w, "  return "); mulib_asdl_ctag_(w, p, t, ctor->name); mulib_asdl_put_(w, ";\n}\n"); return;
  }
  mulib_asdl_put_(w, "  "); mulib_asdl_cname_(w, p, "_t_", t->name);
  mulib_asdl_put_(w, " node = ("); mulib_asdl_cname_(w, p, "_t_", t->name);
  mulib_asdl_put_(w, ")malloc(sizeof *node);\n  if (!node) return NULL;\n");
  if (ctor) {
    mulib_asdl_put_(w, "  node->kind = "); mulib_asdl_ctag_(w, p, t, ctor->name); mulib_asdl_put_(w, ";\n");
    if (!n) {
      mulib_asdl_put_(w, "  node->v.c_"); mulib_asdl_cid_(w, ctor->name); mulib_asdl_put_(w, ".unused = 0;\n");
    }
  }
  for (i = 0; i < n + t->attribute_count; ++i) {
    int attr = i >= n;
    size_t at = attr ? i - n : i;
    const mulib_asdl_field_t *field = attr ? &t->attributes[at] : &f[at];
    mulib_asdl_put_(w, "  node->");
    if (ctor && !attr) { mulib_asdl_put_(w, "v.c_"); mulib_asdl_cid_(w, ctor->name); mulib_asdl_put_(w, "."); }
    mulib_asdl_cfield_(w, field, at, attr); mulib_asdl_put_(w, " = ");
    mulib_asdl_cfield_(w, field, at, attr); mulib_asdl_put_(w, ";\n");
  }
  mulib_asdl_put_(w, "  return node;\n}\n");
}
static void mulib_asdl_emit_c_(mulib_asdl_writer_ *w, const mulib_asdl_module_t *m, const char *p) {
  size_t i, j;
  mulib_asdl_put_(w, "/* Generated ASDL types. Constructors borrow all child/string/sequence storage. */\n#ifndef ");
  mulib_asdl_cname_(w, p, "_", "ASDL_GENERATED_H"); mulib_asdl_put_(w, "\n#define ");
  mulib_asdl_cname_(w, p, "_", "ASDL_GENERATED_H");
  mulib_asdl_put_(w, "\n#include <stddef.h>\n#include <stdint.h>\n#include <stdlib.h>\n");
  mulib_asdl_put_(w, "typedef int64_t "); mulib_asdl_cname_(w, p, "_t_", "int"); mulib_asdl_put_(w, ";\n");
  mulib_asdl_put_(w, "typedef struct { const char *data; size_t length; } ");
  mulib_asdl_cname_(w, p, "_t_", "string"); mulib_asdl_put_(w, ";\ntypedef ");
  mulib_asdl_cname_(w, p, "_t_", "string"); mulib_asdl_put_(w, " ");
  mulib_asdl_cname_(w, p, "_t_", "identifier"); mulib_asdl_put_(w, ";\n");
  /* C99 cannot forward declare enums. Emit all finite enums first. */
  for (i = 0; i < m->count; ++i) if (mulib_asdl_is_enum(&m->types[i])) mulib_asdl_cenum_(w, p, &m->types[i], 1);
  for (i = 0; i < m->count; ++i) if (!mulib_asdl_is_enum(&m->types[i])) {
    mulib_asdl_put_(w, "typedef struct "); mulib_asdl_cname_(w, p, "_node_", m->types[i].name);
    mulib_asdl_put_(w, " *"); mulib_asdl_cname_(w, p, "_t_", m->types[i].name); mulib_asdl_put_(w, ";\n");
  }
  mulib_asdl_cwrappers_(w, p, "int"); mulib_asdl_cwrappers_(w, p, "string"); mulib_asdl_cwrappers_(w, p, "identifier");
  for (i = 0; i < m->count; ++i) mulib_asdl_cwrappers_(w, p, m->types[i].name);
  for (i = 0; i < m->count; ++i) {
    const mulib_asdl_type_t *t = &m->types[i];
    if (mulib_asdl_is_enum(t)) continue;
    if (t->kind == MULIB_ASDL_SUM) mulib_asdl_cenum_(w, p, t, 0);
    mulib_asdl_put_(w, "struct "); mulib_asdl_cname_(w, p, "_node_", t->name); mulib_asdl_put_(w, " {\n");
    if (t->kind == MULIB_ASDL_PRODUCT) mulib_asdl_cfields_(w, p, t->fields, t->field_count, 0);
    else {
      mulib_asdl_put_(w, "  enum "); mulib_asdl_cname_(w, p, "_kind_", t->name); mulib_asdl_put_(w, " kind;\n");
      mulib_asdl_cfields_(w, p, t->attributes, t->attribute_count, 1);
      mulib_asdl_put_(w, "  union {\n");
      for (j = 0; j < t->constructor_count; ++j) {
        const mulib_asdl_constructor_t *c = &t->constructors[j];
        mulib_asdl_put_(w, "    struct {\n"); mulib_asdl_cfields_(w, p, c->fields, c->field_count, 0);
        if (!c->field_count) mulib_asdl_put_(w, "      unsigned char unused;\n");
        mulib_asdl_put_(w, "    } c_"); mulib_asdl_cid_(w, c->name); mulib_asdl_put_(w, ";\n");
      }
      mulib_asdl_put_(w, "  } v;\n");
    }
    mulib_asdl_put_(w, "};\n");
  }
  for (i = 0; i < m->count; ++i) {
    const mulib_asdl_type_t *t = &m->types[i];
    if (t->kind == MULIB_ASDL_PRODUCT) mulib_asdl_cmake_(w, p, t, NULL);
    else for (j = 0; j < t->constructor_count; ++j) mulib_asdl_cmake_(w, p, t, &t->constructors[j]);
  }
  mulib_asdl_put_(w, "#endif\n");
}
int mulib_asdl_generate(const mulib_asdl_module_t *m, const mulib_asdl_options_t *o,
    mulib_strbuf_t *out, mulib_asdl_error_t *error) {
  mulib_asdl_error_t local;
  mulib_asdl_writer_ w;
  mulib_asdl_span_t z = {0, 0};
  mulib_asdl_format_t format = o ? o->format : MULIB_ASDL_SEXPR;
  const char *p = o && o->c_prefix ? o->c_prefix : "asdl";
  int ok;
  if (!error) error = &local;
  memset(error, 0, sizeof *error); memset(&w, 0, sizeof w); w.error = error;
  if (!m || !out || format < MULIB_ASDL_SEXPR || format > MULIB_ASDL_C99 ||
      (format == MULIB_ASDL_C99 && !mulib_asdl_identifier_(p, 0)))
    return mulib_asdl_error_(error, MULIB_ASDL_INVALID, NULL, z, "invalid module, output, format or C prefix");
  if (format == MULIB_ASDL_C99) mulib_asdl_emit_c_(&w, m, p);
  else mulib_asdl_emit_schema_(&w, m, format == MULIB_ASDL_YANTLR);
  ok = !error->code;
  if (ok && !mulib_strbuf_append_n(out, w.text.data, w.text.length))
    ok = mulib_asdl_error_(error, MULIB_ASDL_MEMORY, NULL, z, "cannot append generated output");
  mulib_strbuf_free(&w.text); return ok;
}
int mulib_asdl_compile(const char *s, size_t n, const mulib_asdl_options_t *o,
    mulib_strbuf_t *out, mulib_asdl_error_t *e) {
  mulib_asdl_module_t *m = mulib_asdl_parse(s, n, o, e);
  int ok;
  if (!m) return 0;
  ok = mulib_asdl_generate(m, o, out, e); mulib_asdl_destroy(m); return ok;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
