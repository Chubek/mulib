/**
 * @file mulib-yantlr-stdlib.h
 * @brief Opt-in YANTLR value, diagnostic, artifact and PEG services (version 1).
 * Define MULIB_IMPL_YANTLR_STDLIB once. Implement YANTLR and its dependencies
 * explicitly; this header never installs services in the core implicitly.
 */
#ifndef MULIB_YANTLR_STDLIB_H
#define MULIB_YANTLR_STDLIB_H
#include "mulib-yantlr.h"
#define MULIB_YANTLR_STDLIB_VERSION 1
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_yantlr_stdlib mulib_yantlr_stdlib_t;
/** Per-operation bounds. Zero selects 100000 items, 16 MiB, depth 64 and
 * parsec defaults (captures additionally capped by max_items). */
typedef struct mulib_yantlr_stdlib_options {
  size_t max_items, max_bytes, max_depth;
  mulib_parsec_options_t parser;
} mulib_yantlr_stdlib_options_t;
/** Allocate an immutable registry. NULL on allocation failure. Must outlive
 * every run using its bindings, including retained partial applications. */
mulib_yantlr_stdlib_t *mulib_yantlr_stdlib_create(const mulib_yantlr_stdlib_options_t *);
/** Borrow the complete registry, suitable for evaluate or copying/merging with
 * application bindings. NULL library returns NULL and count zero. */
const mulib_yantlr_binding_t *mulib_yantlr_stdlib_bindings(const mulib_yantlr_stdlib_t *, size_t *count);
/** Release the registry and its constants, after all dependent runs. */
void mulib_yantlr_stdlib_destroy(mulib_yantlr_stdlib_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_YANTLR_STDLIB) && !defined(MULIB_YANTLR_STDLIB_IMPLEMENTED)
#define MULIB_YANTLR_STDLIB_IMPLEMENTED
#include "mulib-peg.h"
#include "mulib-strbuf.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
/* One catalogue is the source of truth for names, arities and dispatch. */
#define MULIB_YANTLR_STDLIB_EXPORTS_(X) \
  X(IDENTITY, "identity", 1) X(TYPE, "Services[Core]::Type", 1) \
  X(ADD, "add", 2) X(SUB, "sub", 2) X(MUL, "mul", 2) \
  X(DIV, "div", 2) X(MOD, "mod", 2) \
  X(EQ, "eq", 2) X(NEQ, "neq", 2) X(LT, "lt", 2) \
  X(LEQ, "leq", 2) X(GT, "gt", 2) X(GEQ, "geq", 2) \
  X(NOT, "not", 1) X(AND, "and", 2) X(OR, "or", 2) \
  X(CHOOSE, "Services[Core]::Choose", 3) \
  X(ALENGTH, "Services[Array]::Length", 1) X(AT, "Services[Array]::At", 2) \
  X(MAP, "Services[Array]::Map", 2) X(FILTER, "Services[Array]::Filter", 2) \
  X(FOLD, "Services[Array]::Fold", 3) X(ACONCAT, "Services[Array]::Concat", 2) \
  X(RANGE, "Services[Array]::Range", 1) \
  X(SLENGTH, "Services[String]::Length", 1) X(SCONCAT, "Services[String]::Concat", 2) \
  X(SLICE, "Services[String]::Slice", 3) X(JOIN, "Services[String]::Join", 2) \
  X(KEYS, "Services[Record]::Keys", 1) X(VALUES, "Services[Record]::Values", 1) \
  X(GET, "Services[Record]::Get", 2) X(HAS, "Services[Record]::Has", 2) \
  X(JPARSE, "Services[JSON]::Parse", 1) X(JSTRINGIFY, "Services[JSON]::Stringify", 1) \
  X(DEBUG, "Services[Diag]::Levels::Debug", 1) X(INFO, "Services[Diag]::Levels::Info", 1) \
  X(WARNING, "Services[Diag]::Levels::Warning", 1) X(ERROR, "Services[Diag]::Levels::Error", 1) \
  X(ASSERT, "Services[Diag]::Assert", 2) X(FAIL, "Services[Diag]::Fail", 1) \
  X(EMIT, "Services[IO]::Emit", 2) \
  X(PEG, "Services[Parse]::PEG", 1) X(EBNF, "Services[Parse]::EBNF", 1) \
  X(RUN, "Services[Parse]::Run", 2)
typedef enum mulib_yantlr_stdlib_op_ {
#define MULIB_YANTLR_STDLIB_ENUM_(id, name, arity) MULIB_Y1S_##id##_,
  MULIB_YANTLR_STDLIB_EXPORTS_(MULIB_YANTLR_STDLIB_ENUM_)
#undef MULIB_YANTLR_STDLIB_ENUM_
  MULIB_Y1S_COUNT_
} mulib_yantlr_stdlib_op_;
typedef struct mulib_yantlr_stdlib_entry_ {
  mulib_yantlr_stdlib_op_ op;
  const mulib_yantlr_stdlib_t *library;
} mulib_yantlr_stdlib_entry_;
struct mulib_yantlr_stdlib {
  mulib_yantlr_stdlib_options_t options;
  mulib_json_t *numeric;
  mulib_yantlr_binding_t bindings[MULIB_Y1S_COUNT_ + 1];
  mulib_yantlr_stdlib_entry_ entries[MULIB_Y1S_COUNT_];
};
static int mulib_yantlr_stdlib_call_(mulib_yantlr_run_t *,
    const mulib_yantlr_value_t *const *, size_t, const mulib_yantlr_value_t **, void *);
mulib_yantlr_stdlib_t *mulib_yantlr_stdlib_create(const mulib_yantlr_stdlib_options_t *options) {
  static const struct { const char *name; size_t arity; } specs[] = {
#define MULIB_YANTLR_STDLIB_SPEC_(id, name, arity) {name, arity},
    MULIB_YANTLR_STDLIB_EXPORTS_(MULIB_YANTLR_STDLIB_SPEC_)
#undef MULIB_YANTLR_STDLIB_SPEC_
  };
  mulib_yantlr_stdlib_t *s = (mulib_yantlr_stdlib_t *)calloc(1, sizeof *s);
  size_t i;
  if (!s) return NULL;
  if (options) s->options = *options;
  if (!s->options.max_items) s->options.max_items = 100000;
  if (!s->options.max_bytes) s->options.max_bytes = 16u * 1024u * 1024u;
  if (!s->options.max_depth) s->options.max_depth = 64;
  /* Recursive conversion uses native stack frames; never allow an unsafe depth. */
  if (s->options.max_depth > 128) s->options.max_depth = 128;
  if (!s->options.parser.max_captures || s->options.parser.max_captures > s->options.max_items)
    s->options.parser.max_captures = s->options.max_items;
  s->numeric = mulib_json_string("numeric");
  if (!s->numeric) { free(s); return NULL; }
  for (i = 0; i < MULIB_Y1S_COUNT_; ++i) {
    s->entries[i].op = (mulib_yantlr_stdlib_op_)i; s->entries[i].library = s;
    s->bindings[i].name = specs[i].name; s->bindings[i].arity = specs[i].arity;
    s->bindings[i].function = mulib_yantlr_stdlib_call_; s->bindings[i].user = &s->entries[i];
  }
  s->bindings[i].name = "Numeric"; s->bindings[i].constant = s->numeric;
  return s;
}
const mulib_yantlr_binding_t *mulib_yantlr_stdlib_bindings(const mulib_yantlr_stdlib_t *s, size_t *n) {
  if (n) *n = s ? MULIB_Y1S_COUNT_ + 1 : 0;
  return s ? s->bindings : NULL;
}
void mulib_yantlr_stdlib_destroy(mulib_yantlr_stdlib_t *s) {
  if (s) { mulib_json_destroy(s->numeric); free(s); }
}
static int mulib_yantlr_stdlib_type_(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *v,
    mulib_yantlr_type_t t) {
  return mulib_yantlr_typeof(v) == t || mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: unexpected argument type");
}
static int mulib_yantlr_stdlib_bound_(mulib_yantlr_run_t *r, size_t n, size_t limit) {
  return n <= limit || mulib_yantlr_fail(r, MULIB_YANTLR_LIMIT, "stdlib: operation limit");
}
static int mulib_yantlr_stdlib_index_(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *v, size_t *n) {
  double d;
  if (!mulib_yantlr_stdlib_type_(r, v, MULIB_YANTLR_NUMBER)) return 0;
  d = mulib_yantlr_numeric(v);
  /* The strict SIZE_MAX comparison is conservative on narrow size_t and avoids
   * rounding SIZE_MAX upwards into an unrepresentable integer on 64-bit hosts. */
  if (d < 0 || d >= (double)SIZE_MAX || d > 9007199254740991.0 || floor(d) != d)
    return mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: expected a nonnegative exact index");
  *n = (size_t)d; return 1;
}
static const char *mulib_yantlr_stdlib_text_(mulib_yantlr_run_t *r, const mulib_yantlr_value_t *v,
    size_t *n, size_t limit) {
  const char *s = mulib_yantlr_text(v, n);
  if (!s) { mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: expected a string"); return NULL; }
  return mulib_yantlr_stdlib_bound_(r, *n, limit) ? s : NULL;
}
static void *mulib_yantlr_stdlib_alloc_(mulib_yantlr_run_t *r, size_t n, size_t size) {
  void *p;
  if (n > SIZE_MAX / size) { mulib_yantlr_fail(r, MULIB_YANTLR_LIMIT, "stdlib: size overflow"); return NULL; }
  p = malloc(n ? n * size : 1);
  if (!p) mulib_yantlr_fail(r, MULIB_YANTLR_MEMORY, "stdlib: out of memory");
  return p;
}
static int mulib_yantlr_stdlib_append_(mulib_yantlr_run_t *r, mulib_strbuf_t *b,
    const char *s, size_t n, size_t limit) {
  if (!mulib_yantlr_stdlib_bound_(r, n, limit - b->length)) return 0;
  return mulib_strbuf_append_n(b, s, n) || mulib_yantlr_fail(r, MULIB_YANTLR_MEMORY, "stdlib: out of memory");
}
/* Bounded deep conversion, preserving embedded NUL record keys. */
static mulib_json_t *mulib_yantlr_stdlib_json_(mulib_yantlr_run_t *r,
    const mulib_yantlr_value_t *v, const mulib_yantlr_stdlib_options_t *o,
    size_t depth, size_t *items, size_t *bytes) {
  mulib_json_t *j = NULL, *child;
  const char *text;
  size_t i, n, len;
  mulib_yantlr_type_t t = mulib_yantlr_typeof(v);
  if (depth >= o->max_depth || *items >= o->max_items) {
    mulib_yantlr_fail(r, MULIB_YANTLR_LIMIT, "stdlib: JSON traversal limit"); return NULL;
  }
  ++*items;
  switch (t) {
    case MULIB_YANTLR_NULL: j = mulib_json_new(MULIB_JSON_NULL); break;
    case MULIB_YANTLR_BOOL: j = mulib_json_bool(mulib_yantlr_boolean(v)); break;
    case MULIB_YANTLR_NUMBER: j = mulib_json_number(mulib_yantlr_numeric(v)); break;
    case MULIB_YANTLR_STRING:
      text = mulib_yantlr_text(v, &len);
      if (!mulib_yantlr_stdlib_bound_(r, len, o->max_bytes - *bytes)) return NULL;
      *bytes += len; j = mulib_json_string_n(text, len); break;
    case MULIB_YANTLR_ARRAY: case MULIB_YANTLR_RECORD:
      n = mulib_yantlr_size(v);
      if (!mulib_yantlr_stdlib_bound_(r, n, o->max_items - *items)) return NULL;
      j = mulib_json_new(t == MULIB_YANTLR_ARRAY ? MULIB_JSON_ARRAY : MULIB_JSON_OBJECT);
      if (!j) break;
      for (i = 0; i < n; ++i) {
        child = mulib_yantlr_stdlib_json_(r, mulib_yantlr_at(v, i), o, depth + 1, items, bytes);
        if (!child) { mulib_json_destroy(j); return NULL; }
        if (t == MULIB_YANTLR_RECORD) {
          text = mulib_yantlr_key(v, i, &len);
          if (!mulib_yantlr_stdlib_bound_(r, len, o->max_bytes - *bytes)) {
            mulib_json_destroy(child); mulib_json_destroy(j); return NULL;
          }
          *bytes += len;
          if (mulib_json_set_n(j, text, len, child)) continue;
        } else if (mulib_json_append(j, child)) continue;
        mulib_json_destroy(child); mulib_json_destroy(j);
        mulib_yantlr_fail(r, MULIB_YANTLR_MEMORY, "stdlib: JSON allocation failed"); return NULL;
      }
      break;
    default: mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: functions and opaque values are not JSON"); return NULL;
  }
  if (!j) mulib_yantlr_fail(r, MULIB_YANTLR_MEMORY, "stdlib: JSON allocation failed");
  return j;
}
static int mulib_yantlr_stdlib_check_json_(mulib_yantlr_run_t *r, const mulib_json_t *j,
    const mulib_yantlr_stdlib_options_t *o, size_t depth, size_t *items) {
  size_t i;
  if (depth >= o->max_depth || *items >= o->max_items)
    return mulib_yantlr_fail(r, MULIB_YANTLR_LIMIT, "stdlib: JSON traversal limit");
  ++*items;
  for (i = 0; i < mulib_json_size(j); ++i)
    if (!mulib_yantlr_stdlib_check_json_(r, mulib_json_at(j, i), o, depth + 1, items)) return 0;
  return 1;
}
static void mulib_yantlr_stdlib_grammar_destroy_(void *p) { mulib_peg_destroy((mulib_peg_t *)p); }
static int mulib_yantlr_stdlib_call_(mulib_yantlr_run_t *r,
    const mulib_yantlr_value_t *const *a, size_t argc, const mulib_yantlr_value_t **out, void *user) {
  const mulib_yantlr_stdlib_entry_ *e = (const mulib_yantlr_stdlib_entry_ *)user;
  const mulib_yantlr_stdlib_options_t *o = &e->library->options;
  mulib_yantlr_stdlib_op_ op = e->op;
  const mulib_yantlr_value_t *v = NULL, **array, *item, *fn;
  const char *s, *t, *key;
  size_t n = 0, m = 0, i, j, used = 0, start, count;
  double x, y;
  int b = 0, cmp = 0;
  mulib_strbuf_t buffer;
  (void)argc;
  *out = NULL;
  if (op >= MULIB_Y1S_ADD_ && op <= MULIB_Y1S_MOD_) {
    if (!mulib_yantlr_stdlib_type_(r, a[0], MULIB_YANTLR_NUMBER) ||
        !mulib_yantlr_stdlib_type_(r, a[1], MULIB_YANTLR_NUMBER)) return 0;
    x = mulib_yantlr_numeric(a[0]); y = mulib_yantlr_numeric(a[1]);
    if ((op == MULIB_Y1S_DIV_ || op == MULIB_Y1S_MOD_) && y == 0)
      return mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: division by zero");
    switch (op) {
      case MULIB_Y1S_ADD_: x += y; break;
      case MULIB_Y1S_SUB_: x -= y; break;
      case MULIB_Y1S_MUL_: x *= y; break;
      case MULIB_Y1S_DIV_: x /= y; break;
      default: x = fmod(x, y); break;
    }
    v = mulib_yantlr_number(r, x);
  } else if (op >= MULIB_Y1S_EQ_ && op <= MULIB_Y1S_GEQ_) {
    mulib_yantlr_type_t type = mulib_yantlr_typeof(a[0]);
    if (!mulib_yantlr_stdlib_type_(r, a[1], type)) return 0;
    if (type == MULIB_YANTLR_NUMBER) {
      x = mulib_yantlr_numeric(a[0]); y = mulib_yantlr_numeric(a[1]); cmp = (x > y) - (x < y);
    } else if (type == MULIB_YANTLR_STRING) {
      s = mulib_yantlr_stdlib_text_(r, a[0], &n, o->max_bytes);
      t = mulib_yantlr_stdlib_text_(r, a[1], &m, o->max_bytes);
      if (!s || !t) return 0;
      cmp = memcmp(s, t, n < m ? n : m);
      if (!cmp) cmp = (n > m) - (n < m);
    } else if ((op == MULIB_Y1S_EQ_ || op == MULIB_Y1S_NEQ_) &&
        (type == MULIB_YANTLR_BOOL || type == MULIB_YANTLR_NULL))
      cmp = mulib_yantlr_boolean(a[0]) - mulib_yantlr_boolean(a[1]);
    else return mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: comparison requires compatible scalars");
    switch (op) {
      case MULIB_Y1S_EQ_: b = cmp == 0; break;
      case MULIB_Y1S_NEQ_: b = cmp != 0; break;
      case MULIB_Y1S_LT_: b = cmp < 0; break;
      case MULIB_Y1S_LEQ_: b = cmp <= 0; break;
      case MULIB_Y1S_GT_: b = cmp > 0; break;
      default: b = cmp >= 0; break;
    }
    v = mulib_yantlr_bool(r, b);
  } else switch (op) {
    case MULIB_Y1S_IDENTITY_: v = a[0]; break;
    case MULIB_Y1S_TYPE_: {
      static const char *const types[] = {"null", "bool", "number", "string", "array", "record", "function", "opaque"};
      s = types[mulib_yantlr_typeof(a[0])]; v = mulib_yantlr_string(r, s, strlen(s)); break;
    }
    case MULIB_Y1S_NOT_: case MULIB_Y1S_AND_: case MULIB_Y1S_OR_: case MULIB_Y1S_CHOOSE_:
      if (!mulib_yantlr_stdlib_type_(r, a[0], MULIB_YANTLR_BOOL)) return 0;
      b = mulib_yantlr_boolean(a[0]);
      if (op == MULIB_Y1S_CHOOSE_) { v = b ? a[1] : a[2]; break; }
      if (op == MULIB_Y1S_NOT_) b = !b;
      else {
        if (!mulib_yantlr_stdlib_type_(r, a[1], MULIB_YANTLR_BOOL)) return 0;
        b = op == MULIB_Y1S_AND_ ? b && mulib_yantlr_boolean(a[1]) : b || mulib_yantlr_boolean(a[1]);
      }
      v = mulib_yantlr_bool(r, b); break;
    case MULIB_Y1S_ALENGTH_: case MULIB_Y1S_AT_:
      if (!mulib_yantlr_stdlib_type_(r, a[0], MULIB_YANTLR_ARRAY)) return 0;
      n = mulib_yantlr_size(a[0]);
      if (op == MULIB_Y1S_ALENGTH_) v = mulib_yantlr_number(r, (double)n);
      else {
        if (!mulib_yantlr_stdlib_index_(r, a[1], &i)) return 0;
        if (i >= n) return mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: array index out of bounds");
        v = mulib_yantlr_at(a[0], i);
      }
      break;
    case MULIB_Y1S_MAP_: case MULIB_Y1S_FILTER_: case MULIB_Y1S_FOLD_:
      if (!mulib_yantlr_stdlib_type_(r, a[0], MULIB_YANTLR_FUNCTION) ||
          !mulib_yantlr_stdlib_type_(r, a[op == MULIB_Y1S_FOLD_ ? 2 : 1], MULIB_YANTLR_ARRAY)) return 0;
      item = a[op == MULIB_Y1S_FOLD_ ? 2 : 1]; n = mulib_yantlr_size(item);
      if (!mulib_yantlr_stdlib_bound_(r, n, o->max_items)) return 0;
      if (op == MULIB_Y1S_FOLD_) {
        v = a[1];
        for (i = 0; i < n && v; ++i) {
          fn = mulib_yantlr_apply(r, a[0], v);
          v = fn ? mulib_yantlr_apply(r, fn, mulib_yantlr_at(item, i)) : NULL;
        }
        break;
      }
      array = (const mulib_yantlr_value_t **)mulib_yantlr_stdlib_alloc_(r, n, sizeof *array);
      if (!array) return 0;
      for (i = 0; i < n; ++i) {
        v = mulib_yantlr_apply(r, a[0], mulib_yantlr_at(item, i));
        if (!v) break;
        if (op == MULIB_Y1S_MAP_) array[used++] = v;
        else {
          if (!mulib_yantlr_stdlib_type_(r, v, MULIB_YANTLR_BOOL)) break;
          if (mulib_yantlr_boolean(v)) array[used++] = mulib_yantlr_at(item, i);
        }
      }
      v = i == n ? mulib_yantlr_array(r, array, used) : NULL; free(array); break;
    case MULIB_Y1S_ACONCAT_: case MULIB_Y1S_RANGE_: case MULIB_Y1S_KEYS_: case MULIB_Y1S_VALUES_:
      if (op == MULIB_Y1S_RANGE_) {
        if (!mulib_yantlr_stdlib_index_(r, a[0], &n)) return 0;
      } else {
        if (!mulib_yantlr_stdlib_type_(r, a[0], op == MULIB_Y1S_ACONCAT_ ? MULIB_YANTLR_ARRAY : MULIB_YANTLR_RECORD)) return 0;
        n = mulib_yantlr_size(a[0]);
        if (op == MULIB_Y1S_ACONCAT_) {
          if (!mulib_yantlr_stdlib_type_(r, a[1], MULIB_YANTLR_ARRAY)) return 0;
          m = mulib_yantlr_size(a[1]);
        }
      }
      if (!mulib_yantlr_stdlib_bound_(r, n, o->max_items) ||
          !mulib_yantlr_stdlib_bound_(r, m, o->max_items - n)) return 0;
      array = (const mulib_yantlr_value_t **)mulib_yantlr_stdlib_alloc_(r, n + m, sizeof *array);
      if (!array) return 0;
      for (i = 0; i < n + m; ++i) {
        if (op == MULIB_Y1S_RANGE_) array[i] = mulib_yantlr_number(r, (double)i);
        else if (op == MULIB_Y1S_KEYS_) {
          s = mulib_yantlr_key(a[0], i, &count);
          if (!mulib_yantlr_stdlib_bound_(r, count, o->max_bytes - used)) break;
          used += count; array[i] = mulib_yantlr_string(r, s, count);
        } else array[i] = i < n ? mulib_yantlr_at(a[0], i) : mulib_yantlr_at(a[1], i - n);
        if (!array[i]) break;
      }
      v = i == n + m ? mulib_yantlr_array(r, array, n + m) : NULL; free(array); break;
    case MULIB_Y1S_SLENGTH_: case MULIB_Y1S_SLICE_:
      s = mulib_yantlr_stdlib_text_(r, a[0], &n, o->max_bytes); if (!s) return 0;
      if (op == MULIB_Y1S_SLENGTH_) v = mulib_yantlr_number(r, (double)n);
      else {
        if (!mulib_yantlr_stdlib_index_(r, a[1], &start) || !mulib_yantlr_stdlib_index_(r, a[2], &count)) return 0;
        if (start > n || count > n - start)
          return mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: string slice out of bounds");
        v = mulib_yantlr_string(r, s + start, count);
      }
      break;
    case MULIB_Y1S_SCONCAT_: case MULIB_Y1S_JOIN_:
      s = mulib_yantlr_stdlib_text_(r, a[0], &n, o->max_bytes); if (!s) return 0;
      mulib_strbuf_init(&buffer);
      if (op == MULIB_Y1S_SCONCAT_) {
        t = mulib_yantlr_stdlib_text_(r, a[1], &m, o->max_bytes);
        b = t && mulib_yantlr_stdlib_append_(r, &buffer, s, n, o->max_bytes) &&
            mulib_yantlr_stdlib_append_(r, &buffer, t, m, o->max_bytes);
      } else {
        b = mulib_yantlr_stdlib_type_(r, a[1], MULIB_YANTLR_ARRAY);
        count = mulib_yantlr_size(a[1]);
        if (b) b = mulib_yantlr_stdlib_bound_(r, count, o->max_items);
        for (i = 0; i < count && b; ++i) {
          t = mulib_yantlr_stdlib_text_(r, mulib_yantlr_at(a[1], i), &m, o->max_bytes);
          b = t && (!i || mulib_yantlr_stdlib_append_(r, &buffer, s, n, o->max_bytes)) &&
              mulib_yantlr_stdlib_append_(r, &buffer, t, m, o->max_bytes);
        }
      }
      if (b) v = mulib_yantlr_string(r, buffer.data, buffer.length);
      mulib_strbuf_free(&buffer); break;
    case MULIB_Y1S_GET_: case MULIB_Y1S_HAS_:
      if (!mulib_yantlr_stdlib_type_(r, a[0], MULIB_YANTLR_RECORD)) return 0;
      s = mulib_yantlr_stdlib_text_(r, a[1], &n, o->max_bytes); if (!s) return 0;
      m = mulib_yantlr_size(a[0]);
      if (!mulib_yantlr_stdlib_bound_(r, m, o->max_items)) return 0;
      for (i = 0; i < m; ++i) {
        key = mulib_yantlr_key(a[0], i, &count);
        if (!mulib_yantlr_stdlib_bound_(r, count, o->max_bytes - used)) return 0;
        used += count;
        if (n == count && !memcmp(s, key, n)) { v = mulib_yantlr_at(a[0], i); break; }
      }
      if (op == MULIB_Y1S_HAS_) v = mulib_yantlr_bool(r, v != NULL);
      else if (!v) return mulib_yantlr_fail(r, MULIB_YANTLR_NAME, "stdlib: record key not found");
      break;
    case MULIB_Y1S_JPARSE_: case MULIB_Y1S_JSTRINGIFY_: {
      mulib_json_t *json;
      mulib_json_error_t error;
      char *encoded;
      if (op == MULIB_Y1S_JPARSE_) {
        s = mulib_yantlr_stdlib_text_(r, a[0], &n, o->max_bytes); if (!s) return 0;
        json = mulib_json_parse(s, n, &error);
        if (!json) return mulib_yantlr_fail(r, error.message && strstr(error.message, "limit") ?
            MULIB_YANTLR_LIMIT : error.message && strstr(error.message, "memory") && !strstr(error.message, "range") ?
            MULIB_YANTLR_MEMORY : MULIB_YANTLR_SYNTAX, error.message);
        if (mulib_yantlr_stdlib_check_json_(r, json, o, 0, &used)) v = mulib_yantlr_from_json(r, json);
      } else {
        json = mulib_yantlr_stdlib_json_(r, a[0], o, 0, &used, &n);
        if (!json) return 0;
        encoded = mulib_json_stringify(json, 0);
        if (!encoded) mulib_yantlr_fail(r, MULIB_YANTLR_MEMORY, "stdlib: JSON serialization failed");
        else {
          n = strlen(encoded);
          if (mulib_yantlr_stdlib_bound_(r, n, o->max_bytes)) v = mulib_yantlr_string(r, encoded, n);
          free(encoded);
        }
      }
      mulib_json_destroy(json); break;
    }
    case MULIB_Y1S_DEBUG_: case MULIB_Y1S_INFO_: case MULIB_Y1S_WARNING_: case MULIB_Y1S_ERROR_:
      s = mulib_yantlr_stdlib_text_(r, a[0], &n, o->max_bytes); if (!s) return 0;
      if (n != 7 || memcmp(s, "numeric", 7))
        return mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: diagnostic level expects Numeric");
      v = mulib_yantlr_number(r, (double)(op - MULIB_Y1S_DEBUG_)); break;
    case MULIB_Y1S_ASSERT_: case MULIB_Y1S_FAIL_:
      if (op == MULIB_Y1S_ASSERT_) {
        if (!mulib_yantlr_stdlib_type_(r, a[0], MULIB_YANTLR_BOOL)) return 0;
        s = mulib_yantlr_stdlib_text_(r, a[1], &n, o->max_bytes);
      } else s = mulib_yantlr_stdlib_text_(r, a[0], &n, o->max_bytes);
      if (!s) return 0;
      if (memchr(s, 0, n)) return mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: diagnostic message contains NUL");
      if (op == MULIB_Y1S_FAIL_ || !mulib_yantlr_boolean(a[0]))
        return mulib_yantlr_fail(r, MULIB_YANTLR_PROVIDER, s);
      v = a[0]; break;
    case MULIB_Y1S_EMIT_:
      s = mulib_yantlr_stdlib_text_(r, a[0], &n, o->max_bytes);
      t = mulib_yantlr_stdlib_text_(r, a[1], &m, o->max_bytes);
      if (!s || !t) return 0;
      if (memchr(s, 0, n)) return mulib_yantlr_fail(r, MULIB_YANTLR_OUTPUT, "stdlib: artifact path contains NUL");
      if (!mulib_yantlr_emit(r, s, t, m)) return 0;
      v = a[1]; break;
    case MULIB_Y1S_PEG_: case MULIB_Y1S_EBNF_: {
      mulib_peg_error_t error;
      mulib_peg_t *grammar;
      s = mulib_yantlr_stdlib_text_(r, a[0], &n, o->max_bytes); if (!s) return 0;
      grammar = op == MULIB_Y1S_PEG_ ? mulib_peg_compile(s, n, &error) : mulib_peg_compile_ebnf(s, n, &error);
      if (!grammar) return mulib_yantlr_fail(r, error.message && strstr(error.message, "memory") ?
          MULIB_YANTLR_MEMORY : error.message && strstr(error.message, "limit") ? MULIB_YANTLR_LIMIT : MULIB_YANTLR_SYNTAX,
          error.message);
      v = mulib_yantlr_opaque(r, "mulib.yantlr.stdlib.grammar.v1", grammar, mulib_yantlr_stdlib_grammar_destroy_);
      if (!v) mulib_peg_destroy(grammar);
      break;
    }
    case MULIB_Y1S_RUN_: {
      mulib_peg_t *grammar = (mulib_peg_t *)mulib_yantlr_pointer(a[0], "mulib.yantlr.stdlib.grammar.v1");
      mulib_parsec_result_t result;
      const char *const keys[] = {"matched", "consumed", "offset", "captures"};
      const char *const capture_keys[] = {"rule", "offset", "length", "first"};
      const mulib_yantlr_value_t *fields[4], *capture_fields[4];
      int status;
      if (!grammar) return mulib_yantlr_fail(r, MULIB_YANTLR_TYPE, "stdlib: expected a compiled grammar");
      s = mulib_yantlr_stdlib_text_(r, a[1], &n, o->max_bytes); if (!s) return 0;
      status = mulib_peg_run(grammar, NULL, s, n, 1, &o->parser, &result);
      if (status < 0) {
        mulib_yantlr_fail(r, result.error.code == MULIB_PARSEC_MEMORY ? MULIB_YANTLR_MEMORY :
            result.error.code == MULIB_PARSEC_LIMIT ? MULIB_YANTLR_LIMIT : MULIB_YANTLR_PROVIDER,
            "stdlib: parser failed");
        mulib_parsec_result_free(&result); return 0;
      }
      array = (const mulib_yantlr_value_t **)mulib_yantlr_stdlib_alloc_(r, result.count, sizeof *array);
      if (!array) { mulib_parsec_result_free(&result); return 0; }
      for (i = 0; i < result.count; ++i) {
        const mulib_parsec_capture_t *c = &result.captures[i];
        t = mulib_peg_rule_name(grammar, c->tag);
        capture_fields[0] = mulib_yantlr_string(r, t, strlen(t));
        capture_fields[1] = mulib_yantlr_number(r, (double)c->offset);
        capture_fields[2] = mulib_yantlr_number(r, (double)c->length);
        capture_fields[3] = mulib_yantlr_number(r, (double)c->first);
        for (j = 0; j < 4 && capture_fields[j]; ++j) {}
        if (j != 4) break;
        array[i] = mulib_yantlr_record(r, capture_keys, capture_fields, 4);
        if (!array[i]) break;
      }
      if (i == result.count) {
        fields[0] = mulib_yantlr_bool(r, status == 1);
        fields[1] = mulib_yantlr_number(r, (double)result.consumed);
        fields[2] = mulib_yantlr_number(r, (double)(status == 1 ? result.consumed : result.error.offset));
        fields[3] = mulib_yantlr_array(r, array, result.count);
        for (j = 0; j < 4 && fields[j]; ++j) {}
        if (j == 4) v = mulib_yantlr_record(r, keys, fields, 4);
      }
      free(array); mulib_parsec_result_free(&result); break;
    }
    default: return mulib_yantlr_fail(r, MULIB_YANTLR_PROVIDER, "stdlib: unknown operation");
  }
  *out = v; return v != NULL;
}
#undef MULIB_YANTLR_STDLIB_EXPORTS_
#ifdef __cplusplus
}
#endif
#endif /* implementation */
