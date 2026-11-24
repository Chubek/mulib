/**
 * @file mulib-microjs.h
 * @brief Embeddable, garbage-collected JavaScript core compiled to MiniVM.
 * Define MULIB_IMPL_MICROJS and MULIB_IMPL_MINIVM in one translation unit.
 * See the manual's ECMAScript 2025 conformance ledger for implemented features
 * and outstanding requirements. This is not yet a conforming ECMAScript
 * engine.
 */
#ifndef MULIB_MICROJS_H
#define MULIB_MICROJS_H
#include "mulib-minivm.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_microjs mulib_microjs_t;
  typedef struct mulib_microjs_object mulib_microjs_object_t;
  typedef enum mulib_microjs_type
  {
    MULIB_MICROJS_NULL,
    MULIB_MICROJS_NUMBER,
    MULIB_MICROJS_BOOLEAN,
    MULIB_MICROJS_STRING,
    MULIB_MICROJS_UNDEFINED,
    MULIB_MICROJS_OBJECT,
    MULIB_MICROJS_FUNCTION
  } mulib_microjs_type_t;
  /** Owned result. Strings are length-delimited WTF-8 (UTF-8 plus lone
   * surrogates). Objects/functions are rooted handles. Do not shallow-copy.
   * Initialize to zero; release with value_free. Results survive state
   * destruction.
   */
  typedef struct mulib_microjs_value
  {
    mulib_microjs_type_t type;
    double number;
    int boolean;
    char *string;
    size_t length;
    mulib_microjs_object_t *object;
  } mulib_microjs_value_t;
  typedef enum mulib_microjs_status
  {
    MULIB_MICROJS_OK,
    MULIB_MICROJS_INVALID,
    MULIB_MICROJS_SYNTAX,
    MULIB_MICROJS_TYPE,
    MULIB_MICROJS_NAME,
    MULIB_MICROJS_ARITHMETIC,
    MULIB_MICROJS_LIMIT,
    MULIB_MICROJS_MEMORY,
    MULIB_MICROJS_EXCEPTION
  } mulib_microjs_status_t;
  /** Static diagnostic, zero-based byte offset and one-based line/column. */
  typedef struct mulib_microjs_error
  {
    mulib_microjs_status_t code;
    size_t offset, line, column;
    const char *message;
  } mulib_microjs_error_t;
  /** Exact limits, including zero. depth <= 128; variables bounds live
   * bindings and properties; string_bytes bounds UTF-16 storage per string. */
  typedef struct mulib_microjs_limits
  {
    size_t source_bytes, steps, depth, variables, string_bytes;
  } mulib_microjs_limits_t;
  /** Arguments are borrowed. Return an owned result initialized from zero.
   * Callbacks may use get/set/make APIs, but cannot reenter eval/call/collect.
   * user is borrowed until state destruction. */
  typedef mulib_microjs_status_t (*mulib_microjs_native_fn) (
      mulib_microjs_t *, const mulib_microjs_value_t *, size_t,
      mulib_microjs_value_t *, void *);
  /** Defaults: 1 MiB source, 100000 steps, depth 64, 4096 bindings/properties,
   * 64 KiB/string. The separately bounded managed heap defaults to 64 MiB. */
  mulib_microjs_limits_t mulib_microjs_default_limits (void);
  mulib_microjs_t *mulib_microjs_create (const mulib_microjs_limits_t *limits);
  /** Like create, with an exact managed-heap byte budget. */
  mulib_microjs_t *
  mulib_microjs_create_with_heap (const mulib_microjs_limits_t *limits,
                                  size_t heap_bytes);
  void mulib_microjs_destroy (mulib_microjs_t *interpreter);
  void mulib_microjs_value_free (mulib_microjs_value_t *value);
  /** Produce another owned result from a borrowed value; out must be empty. */
  mulib_microjs_status_t
  mulib_microjs_value_copy (mulib_microjs_t *,
                            const mulib_microjs_value_t *value,
                            mulib_microjs_value_t *out);
  /** Create a rooted empty ordinary object (array != 0 creates an array). */
  mulib_microjs_status_t
  mulib_microjs_make_object (mulib_microjs_t *, int array,
                             mulib_microjs_value_t *out);
  /** Compile the whole script before execution; syntax errors have no side
   * effects. Runtime errors preserve earlier effects. out must be empty and is
   * unchanged on error. Empty/declaration-only scripts return undefined. */
  mulib_microjs_status_t mulib_microjs_eval (mulib_microjs_t *, const char *,
                                             size_t, mulib_microjs_value_t *,
                                             mulib_microjs_error_t *);
  /** Copy a global lexical/object binding to an owned output. */
  mulib_microjs_status_t mulib_microjs_get_global (mulib_microjs_t *,
                                                   const char *name,
                                                   mulib_microjs_value_t *out);
  /** Copy input into a mutable global binding (or update an existing binding).
   */
  mulib_microjs_status_t
  mulib_microjs_set_global (mulib_microjs_t *, const char *name,
                            const mulib_microjs_value_t *value);
  mulib_microjs_status_t mulib_microjs_bind (mulib_microjs_t *,
                                             const char *name,
                                             mulib_microjs_native_fn function,
                                             void *user);
  /** Call a rooted function; arguments borrow from the caller. */
  mulib_microjs_status_t
  mulib_microjs_call (mulib_microjs_t *, const mulib_microjs_value_t *function,
                      const mulib_microjs_value_t *arguments, size_t count,
                      mulib_microjs_value_t *out,
                      mulib_microjs_error_t *error);
  /** Copy an ordinary own property (or string/array length) into an owned
   * result. */
  mulib_microjs_status_t
  mulib_microjs_get_property (mulib_microjs_t *,
                              const mulib_microjs_value_t *object,
                              const char *name, mulib_microjs_value_t *out);
  mulib_microjs_status_t mulib_microjs_set_property (
      mulib_microjs_t *, const mulib_microjs_value_t *object, const char *name,
      const mulib_microjs_value_t *value);
  /** Collect unreachable cycles. Returns managed bytes still live. No-op
   * during execution. Public object/function results are roots until
   * value_free. */
  size_t mulib_microjs_collect (mulib_microjs_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_MICROJS) && !defined(MULIB_MICROJS_IMPLEMENTED)
#define MULIB_MICROJS_IMPLEMENTED
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /* Values never pass through MiniVM's integer arithmetic. Its stack contains
   * checked indices into an execution-local table of values/references. */
  typedef struct mulib_microjs_v_
  {
    mulib_microjs_type_t type;
    double number;
    mulib_microjs_object_t *object;
  } mulib_microjs_v_;
  enum
  {
    MULIB_MICROJS_S_,
    MULIB_MICROJS_O_,
    MULIB_MICROJS_E_,
    MULIB_MICROJS_B_,
    MULIB_MICROJS_F_,
    MULIB_MICROJS_N_,
    MULIB_MICROJS_P_
  };
  enum
  {
    MULIB_MICROJS_LITERAL_ = 256,
    MULIB_MICROJS_ID_,
    MULIB_MICROJS_MEMBER_,
    MULIB_MICROJS_INVOKE_,
    MULIB_MICROJS_FUNCTION_,
    MULIB_MICROJS_ARRAY_,
    MULIB_MICROJS_OBJECT_,
    MULIB_MICROJS_BLOCK_,
    MULIB_MICROJS_DECLARE_,
    MULIB_MICROJS_IF_,
    MULIB_MICROJS_WHILE_,
    MULIB_MICROJS_FOR_,
    MULIB_MICROJS_RETURN_,
    MULIB_MICROJS_THROW_,
    MULIB_MICROJS_EXPR_,
    MULIB_MICROJS_EMPTY_,
    MULIB_MICROJS_BREAK_,
    MULIB_MICROJS_CONTINUE_,
    MULIB_MICROJS_EQ_,
    MULIB_MICROJS_NE_,
    MULIB_MICROJS_SEQ_,
    MULIB_MICROJS_SNE_,
    MULIB_MICROJS_LE_,
    MULIB_MICROJS_GE_,
    MULIB_MICROJS_AND_,
    MULIB_MICROJS_OR_,
    MULIB_MICROJS_NULLISH_,
    MULIB_MICROJS_POW_,
    MULIB_MICROJS_SHL_,
    MULIB_MICROJS_SHR_,
    MULIB_MICROJS_USHR_,
    MULIB_MICROJS_INC_,
    MULIB_MICROJS_DEC_,
    MULIB_MICROJS_ADDSET_,
    MULIB_MICROJS_SUBSET_,
    MULIB_MICROJS_MULSET_,
    MULIB_MICROJS_DIVSET_,
    MULIB_MICROJS_MODSET_,
    MULIB_MICROJS_TYPEOF_,
    MULIB_MICROJS_VOID_,
    MULIB_MICROJS_NEG_,
    MULIB_MICROJS_POS_,
    MULIB_MICROJS_GET_,
    MULIB_MICROJS_TRUTH_,
    MULIB_MICROJS_ISNULL_,
    MULIB_MICROJS_ENTER_,
    MULIB_MICROJS_LEAVE_,
    MULIB_MICROJS_LAST_,
    MULIB_MICROJS_INIT_,
    MULIB_MICROJS_THIS_,
    MULIB_MICROJS_POSTINC_,
    MULIB_MICROJS_POSTDEC_,
    MULIB_MICROJS_ARG_,
    MULIB_MICROJS_PAIR_
  };
  /* One traced cell format keeps ownership and the nonrecursive collector
   * small. NODE links: a/b/c/d children, next statement/argument, key
   * identifier/string. PROGRAM owns code and a parallel table of host
   * operation nodes. */
  struct mulib_microjs_object
  {
    mulib_microjs_object_t *heap_next, *work, *a, *b, *c, *d, *next, *key;
    mulib_microjs_t *owner;
    mulib_microjs_v_ value;
    size_t bytes, roots, length, offset, count, capacity;
    unsigned marked;
    int kind, op, flags, initialized;
    uint16_t *units;
    mulib_minivm_instruction_t *code;
    mulib_microjs_object_t **operations;
    mulib_microjs_native_fn native;
    void *user;
  };
  struct mulib_microjs
  {
    mulib_microjs_limits_t limits;
    mulib_microjs_object_t *heap, *global, *intrinsics;
    size_t heap_bytes, heap_limit, bindings, roots, steps, calls;
    int running, destroyed;
    mulib_microjs_error_t error;
  };
  typedef struct mulib_microjs_ref_
  {
    mulib_microjs_v_ value;
    mulib_microjs_object_t *base, *key;
    int reference;
  } mulib_microjs_ref_;
  typedef struct mulib_microjs_run_
  {
    mulib_microjs_t *vm;
    mulib_microjs_object_t *environment, *program;
    mulib_microjs_ref_ *values;
    size_t count, capacity;
    mulib_microjs_v_ last, this_value;
  } mulib_microjs_run_;
  typedef struct mulib_microjs_host_
  {
    mulib_microjs_run_ *run;
    mulib_microjs_object_t *node;
  } mulib_microjs_host_;

  static mulib_microjs_v_
  mulib_microjs_undefined_ (void)
  {
    mulib_microjs_v_ v = { MULIB_MICROJS_UNDEFINED, 0, NULL };
    return v;
  }
  static mulib_microjs_v_
  mulib_microjs_scalar_ (mulib_microjs_type_t t, double n)
  {
    mulib_microjs_v_ v = { t, n, NULL };
    return v;
  }
  static mulib_microjs_v_
  mulib_microjs_object_value_ (mulib_microjs_object_t *o)
  {
    mulib_microjs_v_ v = { MULIB_MICROJS_OBJECT, 0, o };
    if (o && o->kind == MULIB_MICROJS_S_)
      v.type = MULIB_MICROJS_STRING;
    if (o && o->kind == MULIB_MICROJS_F_)
      v.type = MULIB_MICROJS_FUNCTION;
    return v;
  }
  static int
  mulib_microjs_fail_ (mulib_microjs_t *v, mulib_microjs_status_t c, size_t at,
                       const char *message)
  {
    if (!v->error.code)
      {
        v->error.code = c;
        v->error.offset = at;
        v->error.message = message;
      }
    return 0;
  }
  static void *
  mulib_microjs_alloc_ (mulib_microjs_t *v, size_t n)
  {
    void *p;
    if (n > v->heap_limit - v->heap_bytes)
      {
        mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0, "managed heap limit");
        return NULL;
      }
    p = calloc (1, n ? n : 1);
    if (!p)
      {
        mulib_microjs_fail_ (v, MULIB_MICROJS_MEMORY, 0, "allocation failed");
        return NULL;
      }
    v->heap_bytes += n;
    return p;
  }
  static void
  mulib_microjs_release_ (mulib_microjs_t *v, void *p, size_t n)
  {
    if (p)
      {
        free (p);
        v->heap_bytes -= n;
      }
  }
  static mulib_microjs_object_t *
  mulib_microjs_cell_ (mulib_microjs_t *v, int kind)
  {
    mulib_microjs_object_t *o
        = (mulib_microjs_object_t *)mulib_microjs_alloc_ (v, sizeof *o);
    if (!o)
      return NULL;
    o->kind = kind;
    o->owner = v;
    o->bytes = sizeof *o;
    o->heap_next = v->heap;
    v->heap = o;
    o->value = mulib_microjs_undefined_ ();
    return o;
  }
  static void
  mulib_microjs_mark_ (mulib_microjs_object_t *o,
                       mulib_microjs_object_t **work)
  {
    if (o && !o->marked)
      {
        o->marked = 1;
        o->work = *work;
        *work = o;
      }
  }
  size_t
  mulib_microjs_collect (mulib_microjs_t *v)
  {
    mulib_microjs_object_t *o, *work = NULL, **link;
    size_t i;
    if (!v || v->running)
      return v ? v->heap_bytes : 0;
    for (o = v->heap; o; o = o->heap_next)
      o->marked = 0;
    mulib_microjs_mark_ (v->global, &work);
    mulib_microjs_mark_ (v->intrinsics, &work);
    for (o = v->heap; o; o = o->heap_next)
      if (o->roots)
        mulib_microjs_mark_ (o, &work);
    while (work)
      {
        o = work;
        work = o->work;
        mulib_microjs_mark_ (o->a, &work);
        mulib_microjs_mark_ (o->b, &work);
        mulib_microjs_mark_ (o->c, &work);
        mulib_microjs_mark_ (o->d, &work);
        mulib_microjs_mark_ (o->next, &work);
        mulib_microjs_mark_ (o->key, &work);
        mulib_microjs_mark_ (o->value.object, &work);
        if (o->kind == MULIB_MICROJS_P_)
          for (i = 0; i < o->count; ++i)
            mulib_microjs_mark_ (o->operations[i], &work);
      }
    link = &v->heap;
    while ((o = *link) != NULL)
      {
        if (o->marked)
          {
            link = &o->heap_next;
            continue;
          }
        *link = o->heap_next;
        if (o->kind == MULIB_MICROJS_B_ && !(o->flags & 4))
          --v->bindings;
        free (o->units);
        free (o->code);
        free (o->operations);
        v->heap_bytes -= o->bytes;
        free (o);
      }
    return v->heap_bytes;
  }
  mulib_microjs_limits_t
  mulib_microjs_default_limits (void)
  {
    mulib_microjs_limits_t l = { 1048576, 100000, 64, 4096, 65536 };
    return l;
  }
  mulib_microjs_t *
  mulib_microjs_create_with_heap (const mulib_microjs_limits_t *l,
                                  size_t bytes)
  {
    mulib_microjs_t *v;
    if (l && l->depth > 128)
      return NULL;
    v = (mulib_microjs_t *)calloc (1, sizeof *v);
    if (!v)
      return NULL;
    v->limits = l ? *l : mulib_microjs_default_limits ();
    v->heap_limit = bytes;
    v->global = mulib_microjs_cell_ (v, MULIB_MICROJS_E_);
    if (!v->global)
      {
        free (v);
        return NULL;
      }
    v->global->flags = 1;
    return v;
  }
  mulib_microjs_t *
  mulib_microjs_create (const mulib_microjs_limits_t *l)
  {
    return mulib_microjs_create_with_heap (l, 64u * 1024u * 1024u);
  }
  void
  mulib_microjs_destroy (mulib_microjs_t *v)
  {
    if (!v || v->running || v->destroyed)
      return;
    v->destroyed = 1;
    v->global = v->intrinsics = NULL;
    (void)mulib_microjs_collect (v);
    if (!v->roots)
      free (v);
  }
  void
  mulib_microjs_value_free (mulib_microjs_value_t *v)
  {
    if (!v)
      return;
    if ((v->type == MULIB_MICROJS_OBJECT || v->type == MULIB_MICROJS_FUNCTION)
        && v->object)
      {
        mulib_microjs_t *vm = v->object->owner;
        --v->object->roots;
        --vm->roots;
        if (vm->destroyed && !vm->roots)
          {
            (void)mulib_microjs_collect (vm);
            free (vm);
          }
      }
    if (v->type == MULIB_MICROJS_STRING)
      free (v->string);
    memset (v, 0, sizeof *v);
  }

  /* ECMA-262 6.1.4: strings are sequences of UTF-16 code units, including lone
   * surrogates. Only the embedding boundary uses WTF-8. */
  static int
  mulib_microjs_decode_ (const char *s, size_t n, size_t *i, uint32_t *cp)
  {
    unsigned c, k, need;
    uint32_t x, min;
    if (*i == n)
      return 0;
    c = (unsigned char)s[(*i)++];
    if (c < 128)
      {
        *cp = c;
        return 1;
      }
    if (c >= 0xc2 && c <= 0xdf)
      {
        need = 1;
        x = c & 31;
        min = 128;
      }
    else if (c >= 0xe0 && c <= 0xef)
      {
        need = 2;
        x = c & 15;
        min = 2048;
      }
    else if (c >= 0xf0 && c <= 0xf4)
      {
        need = 3;
        x = c & 7;
        min = 65536;
      }
    else
      return 0;
    if (n - *i < need)
      return 0;
    for (k = 0; k < need; ++k)
      {
        c = (unsigned char)s[(*i)++];
        if ((c & 0xc0) != 0x80)
          return 0;
        x = (x << 6) | (c & 63);
      }
    if (x < min || x > 0x10ffff)
      return 0;
    *cp = x;
    return 1;
  }
  static mulib_microjs_object_t *
  mulib_microjs_string_ (mulib_microjs_t *v, const uint16_t *s, size_t n)
  {
    mulib_microjs_object_t *o;
    if (n > SIZE_MAX / sizeof *s || n * sizeof *s > v->limits.string_bytes)
      {
        mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0,
                             "string storage limit");
        return NULL;
      }
    o = mulib_microjs_cell_ (v, MULIB_MICROJS_S_);
    if (!o)
      return NULL;
    if (n)
      {
        o->units = (uint16_t *)mulib_microjs_alloc_ (v, n * sizeof *s);
        if (!o->units)
          return NULL;
        o->bytes += n * sizeof *s;
        if (s)
          memcpy (o->units, s, n * sizeof *s);
      }
    o->length = n;
    return o;
  }
  static mulib_microjs_object_t *
  mulib_microjs_utf8_ (mulib_microjs_t *v, const char *s, size_t n)
  {
    size_t i = 0, count = 0, j = 0;
    uint32_t cp;
    mulib_microjs_object_t *o;
    while (i < n)
      {
        if (!mulib_microjs_decode_ (s, n, &i, &cp))
          {
            mulib_microjs_fail_ (v, MULIB_MICROJS_SYNTAX, i, "invalid UTF-8");
            return NULL;
          }
        count += cp > 65535 ? 2 : 1;
      }
    o = mulib_microjs_string_ (v, NULL, count);
    if (!o)
      return NULL;
    i = 0;
    while (i < n)
      {
        (void)mulib_microjs_decode_ (s, n, &i, &cp);
        if (cp > 65535)
          {
            cp -= 65536;
            o->units[j++] = (uint16_t)(0xd800 + (cp >> 10));
            cp = 0xdc00 + (cp & 1023);
          }
        o->units[j++] = (uint16_t)cp;
      }
    return o;
  }
  static int
  mulib_microjs_string_equal_ (const mulib_microjs_object_t *a,
                               const mulib_microjs_object_t *b)
  {
    return a == b
           || (a && b && a->length == b->length
               && (!a->length
                   || !memcmp (a->units, b->units,
                               a->length * sizeof *a->units)));
  }
  static int
  mulib_microjs_ascii_ (const mulib_microjs_object_t *o, const char *s)
  {
    size_t i, n = strlen (s);
    if (!o || o->length != n)
      return 0;
    for (i = 0; i < n; ++i)
      if (o->units[i] != (unsigned char)s[i])
        return 0;
    return 1;
  }
  static int
  mulib_microjs_export_ (mulib_microjs_t *v, mulib_microjs_v_ x,
                         mulib_microjs_value_t *out)
  {
    mulib_microjs_value_t r = { 0 };
    size_t i, j = 0;
    uint32_t cp;
    r.type = x.type;
    r.number = x.number;
    r.boolean = x.number != 0;
    if (x.type == MULIB_MICROJS_STRING)
      {
        if (x.object->length > (SIZE_MAX - 1) / 3)
          return mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0,
                                      "string export limit");
        r.string = (char *)malloc (x.object->length * 3 + 1);
        if (!r.string)
          return mulib_microjs_fail_ (v, MULIB_MICROJS_MEMORY, 0,
                                      "string export allocation");
        for (i = 0; i < x.object->length; ++i)
          {
            cp = x.object->units[i];
            if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < x.object->length
                && x.object->units[i + 1] >= 0xdc00
                && x.object->units[i + 1] <= 0xdfff)
              cp = 65536 + ((cp - 0xd800) << 10)
                   + (x.object->units[++i] - 0xdc00);
            if (cp < 128)
              r.string[j++] = (char)cp;
            else if (cp < 2048)
              {
                r.string[j++] = (char)(0xc0 | (cp >> 6));
                r.string[j++] = (char)(0x80 | (cp & 63));
              }
            else if (cp < 65536)
              {
                r.string[j++] = (char)(0xe0 | (cp >> 12));
                r.string[j++] = (char)(0x80 | ((cp >> 6) & 63));
                r.string[j++] = (char)(0x80 | (cp & 63));
              }
            else
              {
                r.string[j++] = (char)(0xf0 | (cp >> 18));
                r.string[j++] = (char)(0x80 | ((cp >> 12) & 63));
                r.string[j++] = (char)(0x80 | ((cp >> 6) & 63));
                r.string[j++] = (char)(0x80 | (cp & 63));
              }
          }
        r.string[j] = 0;
        r.length = j;
      }
    else if (x.type == MULIB_MICROJS_OBJECT
             || x.type == MULIB_MICROJS_FUNCTION)
      {
        r.object = x.object;
        ++r.object->roots;
        ++v->roots;
      }
    *out = r;
    return 1;
  }
  static int
  mulib_microjs_import_ (mulib_microjs_t *v, const mulib_microjs_value_t *x,
                         mulib_microjs_v_ *out)
  {
    mulib_microjs_v_ r = mulib_microjs_scalar_ (x->type, x->number);
    if (x->type == MULIB_MICROJS_BOOLEAN)
      r.number = !!x->boolean;
    else if (x->type == MULIB_MICROJS_STRING)
      {
        if (!x->string && x->length)
          return mulib_microjs_fail_ (v, MULIB_MICROJS_INVALID, 0,
                                      "invalid string");
        r.object = mulib_microjs_utf8_ (v, x->string, x->length);
        if (!r.object)
          return 0;
      }
    else if (x->type == MULIB_MICROJS_OBJECT
             || x->type == MULIB_MICROJS_FUNCTION)
      {
        if (!x->object || x->object->owner != v)
          return mulib_microjs_fail_ (v, MULIB_MICROJS_INVALID, 0,
                                      "foreign object");
        if (x->object->kind
            != (x->type == MULIB_MICROJS_FUNCTION ? MULIB_MICROJS_F_
                                                  : MULIB_MICROJS_O_))
          return mulib_microjs_fail_ (v, MULIB_MICROJS_INVALID, 0,
                                      "object type mismatch");
        r.object = x->object;
      }
    else if (x->type != MULIB_MICROJS_NULL
             && x->type != MULIB_MICROJS_UNDEFINED
             && x->type != MULIB_MICROJS_NUMBER)
      return mulib_microjs_fail_ (v, MULIB_MICROJS_INVALID, 0,
                                  "invalid value type");
    *out = r;
    return 1;
  }
  static int
  mulib_microjs_truth_ (mulib_microjs_v_ v)
  {
    if (v.type == MULIB_MICROJS_NULL || v.type == MULIB_MICROJS_UNDEFINED)
      return 0;
    if (v.type == MULIB_MICROJS_STRING)
      return v.object->length != 0;
    if (v.type == MULIB_MICROJS_NUMBER || v.type == MULIB_MICROJS_BOOLEAN)
      return v.number != 0 && !isnan (v.number);
    return 1;
  }
  static int
  mulib_microjs_space_ (uint32_t c)
  {
    return c == 9 || c == 10 || c == 11 || c == 12 || c == 13 || c == 32
           || c == 0xa0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200a)
           || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f
           || c == 0x3000 || c == 0xfeff;
  }
  static int
  mulib_microjs_digit_ (uint32_t c)
  {
    if (c >= '0' && c <= '9')
      return (int)(c - '0');
    if (c >= 'a' && c <= 'f')
      return (int)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
      return (int)(c - 'A' + 10);
    return -1;
  }
  /* Locale independent decimal conversion with bounded significant digits.
   * Correct rounding for all decimal inputs remains a conformance work item.
   */
  static double
  mulib_microjs_numeric_ (const uint16_t *s, size_t n)
  {
    size_t i = 0, end = n, digits = 0, integers = 0, first = SIZE_MAX,
           kept = 0;
    long double mantissa = 0, exponent = 0, scale, value;
    int sign = 1, esign = 1, fraction = 0, base = 10, d;
    while (i < end && mulib_microjs_space_ (s[i]))
      ++i;
    while (end > i && mulib_microjs_space_ (s[end - 1]))
      --end;
    if (i == end)
      return 0;
    if (s[i] == '+' || s[i] == '-')
      {
        sign = s[i++] == '-' ? -1 : 1;
        if (i == end)
          return NAN;
      }
    if (end - i == 8 && s[i] == 'I' && s[i + 1] == 'n' && s[i + 2] == 'f'
        && s[i + 3] == 'i' && s[i + 4] == 'n' && s[i + 5] == 'i'
        && s[i + 6] == 't' && s[i + 7] == 'y')
      return sign * INFINITY;
    if (end - i > 2 && s[i] == '0')
      {
        if (s[i + 1] == 'x' || s[i + 1] == 'X')
          base = 16;
        if (s[i + 1] == 'o' || s[i + 1] == 'O')
          base = 8;
        if (s[i + 1] == 'b' || s[i + 1] == 'B')
          base = 2;
        if (base != 10)
          {
            if (i && (s[i - 1] == '+' || s[i - 1] == '-'))
              return NAN;
            for (i += 2; i < end; ++i)
              {
                d = mulib_microjs_digit_ (s[i]);
                if (d < 0 || d >= base)
                  return NAN;
                mantissa = mantissa * base + d;
              }
            return (double)mantissa;
          }
      }
    while (i < end)
      {
        if (s[i] == '.' && !fraction)
          {
            fraction = 1;
            ++i;
            continue;
          }
        if (s[i] < '0' || s[i] > '9')
          break;
        d = s[i++] - '0';
        if (!fraction)
          ++integers;
        if (first == SIZE_MAX && d)
          first = digits;
        if (first != SIZE_MAX && kept < 19)
          {
            mantissa = mantissa * 10 + d;
            ++kept;
          }
        ++digits;
      }
    if (!digits)
      return NAN;
    if (i < end && (s[i] == 'e' || s[i] == 'E'))
      {
        ++i;
        if (i < end && (s[i] == '+' || s[i] == '-'))
          esign = s[i++] == '-' ? -1 : 1;
        if (i == end || s[i] < '0' || s[i] > '9')
          return NAN;
        while (i < end && s[i] >= '0' && s[i] <= '9')
          {
            if (exponent < 1e20L)
              exponent = exponent * 10 + (s[i] - '0');
            ++i;
          }
      }
    if (i != end)
      return NAN;
    if (first == SIZE_MAX)
      return sign < 0 ? -0.0 : 0.0;
    exponent
        = esign * exponent + (long double)integers - (long double)first - 1;
    if (exponent > DBL_MAX_10_EXP + 1)
      return sign * INFINITY;
    if (exponent < DBL_MIN_10_EXP - DBL_MANT_DIG - 8)
      return sign < 0 ? -0.0 : 0.0;
    value = mantissa / powl (10, (long double)kept - 1);
    scale = powl (10, exponent / 2);
    value *= scale;
    value *= scale;
    return sign * (double)value;
  }
  static double
  mulib_microjs_number_ (mulib_microjs_v_ v)
  {
    switch (v.type)
      {
      case MULIB_MICROJS_NULL:
        return 0;
      case MULIB_MICROJS_BOOLEAN:
      case MULIB_MICROJS_NUMBER:
        return v.number;
      case MULIB_MICROJS_STRING:
        return mulib_microjs_numeric_ (v.object->units, v.object->length);
      default:
        return NAN;
      }
  }
  static mulib_microjs_object_t *
  mulib_microjs_text_ (mulib_microjs_t *v, mulib_microjs_v_ x)
  {
    char buf[128];
    const char *s = buf;
    if (x.type == MULIB_MICROJS_STRING)
      return x.object;
    switch (x.type)
      {
      case MULIB_MICROJS_UNDEFINED:
        s = "undefined";
        break;
      case MULIB_MICROJS_NULL:
        s = "null";
        break;
      case MULIB_MICROJS_BOOLEAN:
        s = x.number ? "true" : "false";
        break;
      case MULIB_MICROJS_NUMBER:
        if (isnan (x.number))
          s = "NaN";
        else if (isinf (x.number))
          s = x.number < 0 ? "-Infinity" : "Infinity";
        else if (x.number == 0)
          s = "0";
        else
          (void)snprintf (buf, sizeof buf, "%.17g", x.number);
        break;
      default:
        s = "[object Object]";
        break;
      }
    return mulib_microjs_utf8_ (v, s, strlen (s));
  }
  static mulib_microjs_object_t *
  mulib_microjs_find_ (mulib_microjs_object_t *o, mulib_microjs_object_t *key,
                       int chain)
  {
    mulib_microjs_object_t *b;
    for (; o; o = chain ? o->b : NULL)
      for (b = o->a; b; b = b->next)
        if (mulib_microjs_string_equal_ (b->key, key))
          return b;
    return NULL;
  }
  static mulib_microjs_object_t *
  mulib_microjs_binding_ (mulib_microjs_t *v, mulib_microjs_object_t *env,
                          mulib_microjs_object_t *key, int flags, int init)
  {
    mulib_microjs_object_t *b;
    if (!(flags & 4) && v->bindings == v->limits.variables)
      {
        mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0,
                             "binding/property limit");
        return NULL;
      }
    b = mulib_microjs_cell_ (v, MULIB_MICROJS_B_);
    if (!b)
      return NULL;
    if (!(flags & 4))
      ++v->bindings;
    b->key = key;
    b->flags = flags;
    b->initialized = init;
    b->next = env->a;
    env->a = b;
    return b;
  }
  static int
  mulib_microjs_index_ (mulib_microjs_object_t *s, uint32_t *out)
  {
    size_t i;
    uint64_t n = 0;
    if (!s->length || (s->length > 1 && s->units[0] == '0'))
      return 0;
    for (i = 0; i < s->length; ++i)
      {
        if (s->units[i] < '0' || s->units[i] > '9')
          return 0;
        n = n * 10 + (s->units[i] - '0');
        if (n >= UINT32_MAX)
          return 0;
      }
    *out = (uint32_t)n;
    return 1;
  }
  static int
  mulib_microjs_property_ (mulib_microjs_t *v, mulib_microjs_v_ x,
                           mulib_microjs_object_t *key, mulib_microjs_v_ *out)
  {
    mulib_microjs_object_t *b;
    uint32_t index;
    *out = mulib_microjs_undefined_ ();
    if (x.type == MULIB_MICROJS_NULL || x.type == MULIB_MICROJS_UNDEFINED)
      return mulib_microjs_fail_ (v, MULIB_MICROJS_TYPE, 0,
                                  "property of null or undefined");
    if (x.type == MULIB_MICROJS_STRING)
      {
        if (mulib_microjs_ascii_ (key, "length"))
          *out = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER,
                                        (double)x.object->length);
        else if (mulib_microjs_index_ (key, &index)
                 && index < x.object->length)
          {
            b = mulib_microjs_string_ (v, x.object->units + index, 1);
            if (!b)
              return 0;
            *out = mulib_microjs_object_value_ (b);
          }
      }
    else if (x.object)
      {
        if (x.object->kind == MULIB_MICROJS_O_ && x.object->flags
            && mulib_microjs_ascii_ (key, "length"))
          *out = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER,
                                        (double)x.object->length);
        else
          {
            b = mulib_microjs_find_ (x.object, key, 0);
            if (b)
              *out = b->value;
          }
      }
    return 1;
  }
  static int
  mulib_microjs_put_property_ (mulib_microjs_t *v, mulib_microjs_object_t *o,
                               mulib_microjs_object_t *key,
                               mulib_microjs_v_ value)
  {
    mulib_microjs_object_t *b, **link;
    uint32_t index;
    if (!o || (o->kind != MULIB_MICROJS_O_ && o->kind != MULIB_MICROJS_F_))
      return mulib_microjs_fail_ (v, MULIB_MICROJS_TYPE, 0,
                                  "property assignment requires object");
    if (o->kind == MULIB_MICROJS_O_ && o->flags
        && mulib_microjs_ascii_ (key, "length"))
      {
        double n = mulib_microjs_number_ (value);
        if (!isfinite (n) || n < 0 || n > UINT32_MAX || floor (n) != n)
          return mulib_microjs_fail_ (v, MULIB_MICROJS_ARITHMETIC, 0,
                                      "invalid array length");
        o->length = (size_t)n;
        link = &o->a;
        while ((b = *link) != NULL)
          {
            if (mulib_microjs_index_ (b->key, &index) && index >= o->length)
              *link = b->next;
            else
              link = &b->next;
          }
        return 1;
      }
    b = mulib_microjs_find_ (o, key, 0);
    if (!b)
      b = mulib_microjs_binding_ (v, o, key, 0, 1);
    if (!b)
      return 0;
    b->value = value;
    if (o->kind == MULIB_MICROJS_O_ && o->flags
        && mulib_microjs_index_ (key, &index) && index >= o->length)
      o->length = (size_t)index + 1;
    return 1;
  }

  typedef struct mulib_microjs_parser_
  {
    mulib_microjs_t *vm;
    const char *source;
    size_t length, pos, start, depth, work;
    int token, newline, functions, loops;
    mulib_microjs_object_t *text;
    double number;
  } mulib_microjs_parser_;
  static int
  mulib_microjs_alpha_ (unsigned c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
           || c == '$';
  }
  static int
  mulib_microjs_decimal_ (unsigned c)
  {
    return c >= '0' && c <= '9';
  }
  static int
  mulib_microjs_parse_fail_ (mulib_microjs_parser_ *p, const char *s)
  {
    return mulib_microjs_fail_ (p->vm, MULIB_MICROJS_SYNTAX, p->start, s);
  }
  static int
  mulib_microjs_parse_step_ (mulib_microjs_parser_ *p)
  {
    if (p->work++ >= p->vm->limits.steps)
      return mulib_microjs_fail_ (p->vm, MULIB_MICROJS_LIMIT, p->start,
                                  "compiler step limit");
    return 1;
  }
  static int
  mulib_microjs_escape_ (mulib_microjs_parser_ *p, uint32_t *cp)
  {
    unsigned c;
    size_t k, count = 0;
    uint32_t x = 0;
    int d, braces = 0;
    if (p->pos == p->length)
      return mulib_microjs_parse_fail_ (p, "incomplete escape");
    c = (unsigned char)p->source[p->pos++];
    switch (c)
      {
      case 'n':
        *cp = 10;
        return 1;
      case 'r':
        *cp = 13;
        return 1;
      case 't':
        *cp = 9;
        return 1;
      case 'b':
        *cp = 8;
        return 1;
      case 'f':
        *cp = 12;
        return 1;
      case 'v':
        *cp = 11;
        return 1;
      case '0':
        if (p->pos < p->length
            && mulib_microjs_decimal_ ((unsigned char)p->source[p->pos]))
          return mulib_microjs_parse_fail_ (p,
                                            "legacy octal escape unsupported");
        *cp = 0;
        return 1;
      case '\r':
        if (p->pos < p->length && p->source[p->pos] == '\n')
          ++p->pos;
        /* fall through */
      case '\n':
        *cp = UINT32_MAX;
        return 1;
      case 'x':
      case 'u':
        k = c == 'x' ? 2 : 4;
        if (c == 'u' && p->pos < p->length && p->source[p->pos] == '{')
          {
            braces = 1;
            ++p->pos;
            k = 6;
          }
        while (count < k && p->pos < p->length)
          {
            d = mulib_microjs_digit_ ((unsigned char)p->source[p->pos]);
            if (d < 0)
              break;
            x = x * 16 + (unsigned)d;
            ++p->pos;
            ++count;
          }
        if (braces)
          {
            if (!count || x > 0x10ffff || p->pos == p->length
                || p->source[p->pos++] != '}')
              return mulib_microjs_parse_fail_ (p, "invalid Unicode escape");
          }
        else if (count != k)
          return mulib_microjs_parse_fail_ (p,
                                            "incomplete hexadecimal escape");
        *cp = x;
        return 1;
      default:
        if (c >= '1' && c <= '9')
          return mulib_microjs_parse_fail_ (
              p, "legacy numeric escape unsupported");
        --p->pos;
        if (!mulib_microjs_decode_ (p->source, p->length, &p->pos, cp))
          return mulib_microjs_parse_fail_ (p, "invalid UTF-8 escape");
        if (*cp == 0x2028 || *cp == 0x2029)
          *cp = UINT32_MAX;
        return 1;
      }
  }
  static int
  mulib_microjs_next_ (mulib_microjs_parser_ *p)
  {
    const char *s = p->source;
    size_t i, n, bytes;
    uint32_t cp;
    unsigned c;
    uint16_t *scratch;
    int quote, closed, base, d, saw, previous;
    static const struct
    {
      const char *text;
      int token;
    } ops[]
        = { { ">>>", MULIB_MICROJS_USHR_ },  { "===", MULIB_MICROJS_SEQ_ },
            { "!==", MULIB_MICROJS_SNE_ },   { "==", MULIB_MICROJS_EQ_ },
            { "!=", MULIB_MICROJS_NE_ },     { "<=", MULIB_MICROJS_LE_ },
            { ">=", MULIB_MICROJS_GE_ },     { "&&", MULIB_MICROJS_AND_ },
            { "||", MULIB_MICROJS_OR_ },     { "??", MULIB_MICROJS_NULLISH_ },
            { "**", MULIB_MICROJS_POW_ },    { "<<", MULIB_MICROJS_SHL_ },
            { ">>", MULIB_MICROJS_SHR_ },    { "++", MULIB_MICROJS_INC_ },
            { "--", MULIB_MICROJS_DEC_ },    { "+=", MULIB_MICROJS_ADDSET_ },
            { "-=", MULIB_MICROJS_SUBSET_ }, { "*=", MULIB_MICROJS_MULSET_ },
            { "/=", MULIB_MICROJS_DIVSET_ }, { "%=", MULIB_MICROJS_MODSET_ } };
    if (p->vm->error.code || !mulib_microjs_parse_step_ (p))
      return 0;
    p->newline = 0;
    p->text = NULL;
    for (;;)
      {
        if (p->pos == p->length)
          break;
        i = p->pos;
        if (!mulib_microjs_decode_ (s, p->length, &i, &cp))
          {
            p->start = p->pos;
            return mulib_microjs_parse_fail_ (p, "invalid UTF-8 source");
          }
        if (mulib_microjs_space_ (cp))
          {
            if (cp == 10 || cp == 13 || cp == 0x2028 || cp == 0x2029)
              p->newline = 1;
            p->pos = i;
            continue;
          }
        if (p->length - p->pos >= 2 && s[p->pos] == '/'
            && s[p->pos + 1] == '/')
          {
            p->pos += 2;
            while (p->pos < p->length)
              {
                i = p->pos;
                if (!mulib_microjs_decode_ (s, p->length, &i, &cp))
                  break;
                if (cp == 10 || cp == 13 || cp == 0x2028 || cp == 0x2029)
                  break;
                p->pos = i;
              }
            continue;
          }
        if (p->length - p->pos >= 2 && s[p->pos] == '/'
            && s[p->pos + 1] == '*')
          {
            p->start = p->pos;
            p->pos += 2;
            closed = 0;
            while (p->pos < p->length)
              {
                if (p->length - p->pos >= 2 && s[p->pos] == '*'
                    && s[p->pos + 1] == '/')
                  {
                    p->pos += 2;
                    closed = 1;
                    break;
                  }
                if (!mulib_microjs_decode_ (s, p->length, &p->pos, &cp))
                  return mulib_microjs_parse_fail_ (
                      p, "invalid comment encoding");
                if (cp == 10 || cp == 13 || cp == 0x2028 || cp == 0x2029)
                  p->newline = 1;
              }
            if (!closed)
              return mulib_microjs_parse_fail_ (p, "unterminated comment");
            continue;
          }
        break;
      }
    p->start = p->pos;
    p->token = 0;
    if (p->pos == p->length)
      return 1;
    c = (unsigned char)s[p->pos++];
    p->token = (int)c;
    if (mulib_microjs_alpha_ (c))
      {
        while (p->pos < p->length
               && (mulib_microjs_alpha_ ((unsigned char)s[p->pos])
                   || mulib_microjs_decimal_ ((unsigned char)s[p->pos])))
          ++p->pos;
        p->text = mulib_microjs_utf8_ (p->vm, s + p->start, p->pos - p->start);
        p->token = MULIB_MICROJS_ID_;
        return p->text != NULL;
      }
    if (mulib_microjs_decimal_ (c)
        || (c == '.' && p->pos < p->length
            && mulib_microjs_decimal_ ((unsigned char)s[p->pos])))
      {
        base = 10;
        p->pos = p->start;
        if (p->length - p->pos >= 2 && s[p->pos] == '0')
          {
            c = (unsigned char)s[p->pos + 1];
            if (c == 'x' || c == 'X')
              base = 16;
            if (c == 'o' || c == 'O')
              base = 8;
            if (c == 'b' || c == 'B')
              base = 2;
          }
        if (base != 10)
          p->pos += 2;
        saw = previous = 0;
        while (p->pos < p->length)
          {
            c = (unsigned char)s[p->pos];
            d = mulib_microjs_digit_ (c);
            if (d >= 0 && d < base)
              {
                ++p->pos;
                saw = previous = 1;
              }
            else if (c == '_' && previous && p->pos + 1 < p->length
                     && (d
                         = mulib_microjs_digit_ ((unsigned char)s[p->pos + 1]))
                            >= 0
                     && d < base)
              {
                ++p->pos;
                previous = 0;
              }
            else
              break;
          }
        if (base == 10 && p->pos < p->length && s[p->pos] == '.')
          {
            ++p->pos;
            previous = 0;
            while (p->pos < p->length)
              {
                c = (unsigned char)s[p->pos];
                if (mulib_microjs_decimal_ (c))
                  {
                    ++p->pos;
                    saw = previous = 1;
                  }
                else if (c == '_' && previous && p->pos + 1 < p->length
                         && mulib_microjs_decimal_ (
                             (unsigned char)s[p->pos + 1]))
                  {
                    ++p->pos;
                    previous = 0;
                  }
                else
                  break;
              }
          }
        if (!saw)
          return mulib_microjs_parse_fail_ (p, "missing numeric digits");
        if (base == 10 && p->pos < p->length
            && (s[p->pos] == 'e' || s[p->pos] == 'E'))
          {
            ++p->pos;
            if (p->pos < p->length && (s[p->pos] == '+' || s[p->pos] == '-'))
              ++p->pos;
            saw = previous = 0;
            while (p->pos < p->length)
              {
                c = (unsigned char)s[p->pos];
                if (mulib_microjs_decimal_ (c))
                  {
                    ++p->pos;
                    saw = previous = 1;
                  }
                else if (c == '_' && previous && p->pos + 1 < p->length
                         && mulib_microjs_decimal_ (
                             (unsigned char)s[p->pos + 1]))
                  {
                    ++p->pos;
                    previous = 0;
                  }
                else
                  break;
              }
            if (!saw)
              return mulib_microjs_parse_fail_ (p, "missing exponent digits");
          }
        if (p->pos < p->length
            && (mulib_microjs_alpha_ ((unsigned char)s[p->pos])
                || mulib_microjs_decimal_ ((unsigned char)s[p->pos])))
          return mulib_microjs_parse_fail_ (p, "invalid numeric literal");
        n = p->pos - p->start;
        if (n > SIZE_MAX / sizeof *scratch)
          return mulib_microjs_parse_fail_ (p, "numeric literal too long");
        bytes = n * sizeof *scratch;
        scratch = (uint16_t *)mulib_microjs_alloc_ (p->vm, bytes);
        if (!scratch)
          return 0;
        n = 0;
        for (i = p->start; i < p->pos; ++i)
          if (s[i] != '_')
            scratch[n++] = (unsigned char)s[i];
        p->number = mulib_microjs_numeric_ (scratch, n);
        mulib_microjs_release_ (p->vm, scratch, bytes);
        p->token = MULIB_MICROJS_LITERAL_;
        return 1;
      }
    if (c == '\'' || c == '"')
      {
        quote = (int)c;
        n = 0;
        closed = 0;
        if (p->length - p->pos > SIZE_MAX / sizeof *scratch)
          return mulib_microjs_parse_fail_ (p, "string too long");
        bytes = (p->length - p->pos) * sizeof *scratch;
        scratch = (uint16_t *)mulib_microjs_alloc_ (p->vm, bytes);
        if (!scratch)
          return 0;
        while (p->pos < p->length)
          {
            c = (unsigned char)s[p->pos];
            if (c == (unsigned)quote)
              {
                ++p->pos;
                closed = 1;
                break;
              }
            if (c == '\\')
              {
                ++p->pos;
                if (!mulib_microjs_escape_ (p, &cp))
                  break;
                if (cp == UINT32_MAX)
                  continue;
              }
            else
              {
                if (!mulib_microjs_decode_ (s, p->length, &p->pos, &cp))
                  {
                    mulib_microjs_parse_fail_ (p, "invalid string encoding");
                    break;
                  }
                if (cp == 10 || cp == 13)
                  {
                    mulib_microjs_parse_fail_ (p, "newline in string");
                    break;
                  }
              }
            if (cp > 65535)
              {
                cp -= 65536;
                scratch[n++] = (uint16_t)(0xd800 + (cp >> 10));
                cp = 0xdc00 + (cp & 1023);
              }
            scratch[n++] = (uint16_t)cp;
          }
        if (!closed)
          mulib_microjs_parse_fail_ (p, "unterminated string");
        if (!p->vm->error.code)
          p->text = mulib_microjs_string_ (p->vm, scratch, n);
        mulib_microjs_release_ (p->vm, scratch, bytes);
        p->token = MULIB_MICROJS_LITERAL_;
        return p->text != NULL;
      }
    --p->pos;
    for (i = 0; i < sizeof ops / sizeof *ops; ++i)
      {
        n = strlen (ops[i].text);
        if (n <= p->length - p->pos && !memcmp (s + p->pos, ops[i].text, n))
          {
            p->pos += n;
            p->token = ops[i].token;
            return 1;
          }
      }
    ++p->pos;
    if (!c || !strchr ("(){}[];:,.?+-*/%!=<>&|^~", (int)c))
      return mulib_microjs_parse_fail_ (p, "unsupported token");
    return 1;
  }
  static int
  mulib_microjs_word_ (mulib_microjs_parser_ *p, const char *s)
  {
    return p->token == MULIB_MICROJS_ID_ && mulib_microjs_ascii_ (p->text, s);
  }
  static int
  mulib_microjs_reserved_ (mulib_microjs_object_t *s)
  {
    static const char *const names[] = {
      "break",    "case",      "catch",  "class",      "const",     "continue",
      "debugger", "default",   "delete", "do",         "else",      "enum",
      "export",   "extends",   "false",  "finally",    "for",       "function",
      "if",       "import",    "in",     "instanceof", "let",       "new",
      "null",     "return",    "super",  "switch",     "this",      "throw",
      "true",     "try",       "typeof", "var",        "void",      "while",
      "with",     "yield",     "await",  "implements", "interface", "package",
      "private",  "protected", "public", "static"
    };
    size_t i;
    for (i = 0; i < sizeof names / sizeof *names; ++i)
      if (mulib_microjs_ascii_ (s, names[i]))
        return 1;
    return 0;
  }
  static int
  mulib_microjs_expect_ (mulib_microjs_parser_ *p, int token)
  {
    if (p->token != token)
      return mulib_microjs_parse_fail_ (p, "unexpected token");
    return mulib_microjs_next_ (p);
  }
  static int
  mulib_microjs_semi_ (mulib_microjs_parser_ *p)
  {
    if (p->token == ';')
      return mulib_microjs_next_ (p);
    if (!p->token || p->token == '}' || p->newline)
      return 1;
    return mulib_microjs_parse_fail_ (p, "expected semicolon");
  }
  static mulib_microjs_object_t *
  mulib_microjs_node_ (mulib_microjs_parser_ *p, int op)
  {
    mulib_microjs_object_t *o;
    if (!mulib_microjs_parse_step_ (p))
      return NULL;
    o = mulib_microjs_cell_ (p->vm, MULIB_MICROJS_N_);
    if (o)
      {
        o->op = op;
        o->offset = p->start;
      }
    return o;
  }
  static int
  mulib_microjs_parse_enter_ (mulib_microjs_parser_ *p)
  {
    if (p->depth >= p->vm->limits.depth)
      return mulib_microjs_fail_ (p->vm, MULIB_MICROJS_LIMIT, p->start,
                                  "parser depth limit");
    ++p->depth;
    return 1;
  }
  static mulib_microjs_object_t *
  mulib_microjs_expression_ (mulib_microjs_parser_ *, int);
  static mulib_microjs_object_t *
  mulib_microjs_statement_ (mulib_microjs_parser_ *);
  static mulib_microjs_object_t *
  mulib_microjs_block_ (mulib_microjs_parser_ *p, int braces)
  {
    mulib_microjs_object_t *o = mulib_microjs_node_ (p, MULIB_MICROJS_BLOCK_),
                           **tail;
    if (!o)
      return NULL;
    if (braces && !mulib_microjs_expect_ (p, '{'))
      return NULL;
    tail = &o->a;
    while (p->token && (!braces || p->token != '}'))
      {
        *tail = mulib_microjs_statement_ (p);
        if (!*tail)
          return NULL;
        tail = &(*tail)->next;
      }
    if (braces && !mulib_microjs_expect_ (p, '}'))
      return NULL;
    return o;
  }
  static mulib_microjs_object_t *
  mulib_microjs_function_ (mulib_microjs_parser_ *p, int named)
  {
    mulib_microjs_object_t *o
        = mulib_microjs_node_ (p, MULIB_MICROJS_FUNCTION_),
        **tail, *q;
    int loops = p->loops;
    if (!o || !mulib_microjs_next_ (p))
      return NULL;
    if (p->token == MULIB_MICROJS_ID_ && !mulib_microjs_reserved_ (p->text))
      {
        o->key = p->text;
        if (!mulib_microjs_next_ (p))
          return NULL;
      }
    else if (named)
      {
        mulib_microjs_parse_fail_ (p, "function name required");
        return NULL;
      }
    if (!mulib_microjs_expect_ (p, '('))
      return NULL;
    tail = &o->a;
    while (p->token != ')')
      {
        if (p->token != MULIB_MICROJS_ID_ || mulib_microjs_reserved_ (p->text))
          {
            mulib_microjs_parse_fail_ (p, "parameter name required");
            return NULL;
          }
        for (q = o->a; q; q = q->next)
          {
            if (!mulib_microjs_parse_step_ (p))
              return NULL;
            if (mulib_microjs_string_equal_ (q->key, p->text))
              {
                mulib_microjs_parse_fail_ (p, "duplicate parameter");
                return NULL;
              }
          }
        *tail = mulib_microjs_node_ (p, MULIB_MICROJS_ARG_);
        if (!*tail)
          return NULL;
        (*tail)->key = p->text;
        tail = &(*tail)->next;
        ++o->count;
        if (!mulib_microjs_next_ (p))
          return NULL;
        if (p->token != ',')
          break;
        if (!mulib_microjs_next_ (p))
          return NULL;
      }
    if (!mulib_microjs_expect_ (p, ')'))
      return NULL;
    ++p->functions;
    p->loops = 0;
    o->b = mulib_microjs_block_ (p, 1);
    --p->functions;
    p->loops = loops;
    return o->b ? o : NULL;
  }
  static mulib_microjs_object_t *
  mulib_microjs_primary_ (mulib_microjs_parser_ *p)
  {
    mulib_microjs_object_t *o, *n, **tail;
    int token = p->token;
    if (token == '!' || token == '~' || token == '+' || token == '-'
        || token == MULIB_MICROJS_INC_ || token == MULIB_MICROJS_DEC_
        || mulib_microjs_word_ (p, "typeof")
        || mulib_microjs_word_ (p, "void"))
      {
        if (mulib_microjs_word_ (p, "typeof"))
          token = MULIB_MICROJS_TYPEOF_;
        if (mulib_microjs_word_ (p, "void"))
          token = MULIB_MICROJS_VOID_;
        if (token == '+')
          token = MULIB_MICROJS_POS_;
        if (token == '-')
          token = MULIB_MICROJS_NEG_;
        o = mulib_microjs_node_ (p, token);
        if (!o || !mulib_microjs_next_ (p)
            || !(o->a = mulib_microjs_expression_ (p, 15)))
          return NULL;
        if ((token == MULIB_MICROJS_INC_ || token == MULIB_MICROJS_DEC_)
            && o->a->op != MULIB_MICROJS_ID_
            && o->a->op != MULIB_MICROJS_MEMBER_)
          {
            mulib_microjs_parse_fail_ (p, "invalid update target");
            return NULL;
          }
        return o;
      }
    if (token == '(')
      {
        if (!mulib_microjs_next_ (p) || !(o = mulib_microjs_expression_ (p, 1))
            || !mulib_microjs_expect_ (p, ')'))
          return NULL;
        o->flags |= 16;
      }
    else if (mulib_microjs_word_ (p, "function"))
      o = mulib_microjs_function_ (p, 0);
    else if (token == '[' || token == '{')
      {
        o = mulib_microjs_node_ (p, token == '[' ? MULIB_MICROJS_ARRAY_
                                                 : MULIB_MICROJS_OBJECT_);
        if (!o || !mulib_microjs_next_ (p))
          return NULL;
        tail = &o->a;
        while (p->token != (token == '[' ? ']' : '}'))
          {
            n = mulib_microjs_node_ (p, MULIB_MICROJS_PAIR_);
            if (!n)
              return NULL;
            *tail = n;
            tail = &n->next;
            if (token == '{')
              {
                if (p->token != MULIB_MICROJS_ID_
                    && p->token != MULIB_MICROJS_LITERAL_)
                  {
                    mulib_microjs_parse_fail_ (p, "object key required");
                    return NULL;
                  }
                n->key = p->text ? p->text
                                 : mulib_microjs_text_ (
                                       p->vm,
                                       mulib_microjs_scalar_ (
                                           MULIB_MICROJS_NUMBER, p->number));
                if (!n->key || !mulib_microjs_next_ (p)
                    || !mulib_microjs_expect_ (p, ':'))
                  return NULL;
              }
            if (token == '[' && p->token == ',')
              n->flags = 1;
            else if (!(n->a = mulib_microjs_expression_ (p, 2)))
              return NULL;
            ++o->count;
            if (p->token != ',')
              break;
            if (!mulib_microjs_next_ (p))
              return NULL;
          }
        if (!mulib_microjs_expect_ (p, token == '[' ? ']' : '}'))
          return NULL;
      }
    else
      {
        o = mulib_microjs_node_ (p, MULIB_MICROJS_LITERAL_);
        if (!o)
          return NULL;
        if (token == MULIB_MICROJS_LITERAL_)
          o->value = p->text ? mulib_microjs_object_value_ (p->text)
                             : mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER,
                                                      p->number);
        else if (mulib_microjs_word_ (p, "true")
                 || mulib_microjs_word_ (p, "false"))
          o->value = mulib_microjs_scalar_ (MULIB_MICROJS_BOOLEAN,
                                            mulib_microjs_word_ (p, "true"));
        else if (mulib_microjs_word_ (p, "null"))
          o->value = mulib_microjs_scalar_ (MULIB_MICROJS_NULL, 0);
        else if (mulib_microjs_word_ (p, "this"))
          o->op = MULIB_MICROJS_THIS_;
        else if (token == MULIB_MICROJS_ID_
                 && !mulib_microjs_reserved_ (p->text))
          {
            o->op = MULIB_MICROJS_ID_;
            o->key = p->text;
          }
        else
          {
            mulib_microjs_parse_fail_ (
                p, "expression required or unsupported keyword");
            return NULL;
          }
        if (!mulib_microjs_next_ (p))
          return NULL;
      }
    if (!o)
      return NULL;
    for (;;)
      {
        token = p->token;
        if (token == '.' || token == '[')
          {
            n = mulib_microjs_node_ (p, MULIB_MICROJS_MEMBER_);
            if (!n)
              return NULL;
            n->a = o;
            if (!mulib_microjs_next_ (p))
              return NULL;
            if (token == '.')
              {
                if (p->token != MULIB_MICROJS_ID_)
                  {
                    mulib_microjs_parse_fail_ (p, "property name required");
                    return NULL;
                  }
                n->b = mulib_microjs_node_ (p, MULIB_MICROJS_LITERAL_);
                if (!n->b)
                  return NULL;
                n->b->value = mulib_microjs_object_value_ (p->text);
                if (!mulib_microjs_next_ (p))
                  return NULL;
              }
            else if (!(n->b = mulib_microjs_expression_ (p, 1))
                     || !mulib_microjs_expect_ (p, ']'))
              return NULL;
            o = n;
          }
        else if (token == '(')
          {
            n = mulib_microjs_node_ (p, MULIB_MICROJS_INVOKE_);
            if (!n)
              return NULL;
            n->a = o;
            tail = &n->b;
            if (!mulib_microjs_next_ (p))
              return NULL;
            while (p->token != ')')
              {
                *tail = mulib_microjs_expression_ (p, 2);
                if (!*tail)
                  return NULL;
                tail = &(*tail)->next;
                ++n->count;
                if (p->token != ',')
                  break;
                if (!mulib_microjs_next_ (p))
                  return NULL;
              }
            if (!mulib_microjs_expect_ (p, ')'))
              return NULL;
            o = n;
          }
        else if (!p->newline
                 && (token == MULIB_MICROJS_INC_
                     || token == MULIB_MICROJS_DEC_))
          {
            if (o->op != MULIB_MICROJS_ID_ && o->op != MULIB_MICROJS_MEMBER_)
              {
                mulib_microjs_parse_fail_ (p, "invalid update target");
                return NULL;
              }
            n = mulib_microjs_node_ (p, token == MULIB_MICROJS_INC_
                                            ? MULIB_MICROJS_POSTINC_
                                            : MULIB_MICROJS_POSTDEC_);
            if (!n || !mulib_microjs_next_ (p))
              return NULL;
            n->a = o;
            o = n;
            break;
          }
        else
          break;
      }
    return o;
  }
  static int
  mulib_microjs_precedence_ (int op)
  {
    switch (op)
      {
      case ',':
        return 1;
      case '=':
      case MULIB_MICROJS_ADDSET_:
      case MULIB_MICROJS_SUBSET_:
      case MULIB_MICROJS_MULSET_:
      case MULIB_MICROJS_DIVSET_:
      case MULIB_MICROJS_MODSET_:
        return 2;
      case '?':
        return 3;
      case MULIB_MICROJS_OR_:
      case MULIB_MICROJS_NULLISH_:
        return 4;
      case MULIB_MICROJS_AND_:
        return 5;
      case '|':
        return 6;
      case '^':
        return 7;
      case '&':
        return 8;
      case MULIB_MICROJS_EQ_:
      case MULIB_MICROJS_NE_:
      case MULIB_MICROJS_SEQ_:
      case MULIB_MICROJS_SNE_:
        return 9;
      case '<':
      case '>':
      case MULIB_MICROJS_LE_:
      case MULIB_MICROJS_GE_:
        return 10;
      case MULIB_MICROJS_SHL_:
      case MULIB_MICROJS_SHR_:
      case MULIB_MICROJS_USHR_:
        return 11;
      case '+':
      case '-':
        return 12;
      case '*':
      case '/':
      case '%':
        return 13;
      case MULIB_MICROJS_POW_:
        return 14;
      default:
        return 0;
      }
  }
  static mulib_microjs_object_t *
  mulib_microjs_expression_ (mulib_microjs_parser_ *p, int minimum)
  {
    mulib_microjs_object_t *a = NULL, *n;
    int op, prec;
    if (!mulib_microjs_parse_enter_ (p))
      return NULL;
    a = mulib_microjs_primary_ (p);
    while (a && (prec = mulib_microjs_precedence_ (p->token)) >= minimum)
      {
        op = p->token;
        n = mulib_microjs_node_ (p, op);
        if (!n)
          {
            a = NULL;
            break;
          }
        n->a = a;
        if (prec == 2 && a->op != MULIB_MICROJS_ID_
            && a->op != MULIB_MICROJS_MEMBER_)
          {
            mulib_microjs_parse_fail_ (p, "invalid assignment target");
            a = NULL;
            break;
          }
        if (op == MULIB_MICROJS_POW_ && !(a->flags & 16)
            && (a->op == MULIB_MICROJS_NEG_ || a->op == MULIB_MICROJS_POS_
                || a->op == '!' || a->op == '~'
                || a->op == MULIB_MICROJS_TYPEOF_
                || a->op == MULIB_MICROJS_VOID_))
          {
            mulib_microjs_parse_fail_ (
                p, "unparenthesized unary exponentiation operand");
            a = NULL;
            break;
          }
        if (!mulib_microjs_next_ (p))
          {
            a = NULL;
            break;
          }
        if (op == '?')
          {
            n->b = mulib_microjs_expression_ (p, 2);
            if (!n->b || !mulib_microjs_expect_ (p, ':')
                || !(n->c = mulib_microjs_expression_ (p, 2)))
              {
                a = NULL;
                break;
              }
          }
        else
          {
            n->b = mulib_microjs_expression_ (
                p, prec + (prec == 2 || op == MULIB_MICROJS_POW_ ? 0 : 1));
            if (!n->b)
              {
                a = NULL;
                break;
              }
          }
        if (op == MULIB_MICROJS_NULLISH_ || op == MULIB_MICROJS_OR_
            || op == MULIB_MICROJS_AND_)
          {
            int bad_a = !(n->a->flags & 16)
                        && (op == MULIB_MICROJS_NULLISH_
                                ? n->a->op == MULIB_MICROJS_OR_
                                      || n->a->op == MULIB_MICROJS_AND_
                                : n->a->op == MULIB_MICROJS_NULLISH_);
            int bad_b = !(n->b->flags & 16)
                        && (op == MULIB_MICROJS_NULLISH_
                                ? n->b->op == MULIB_MICROJS_OR_
                                      || n->b->op == MULIB_MICROJS_AND_
                                : n->b->op == MULIB_MICROJS_NULLISH_);
            if (bad_a || bad_b)
              {
                mulib_microjs_parse_fail_ (
                    p, "nullish/logical operators require parentheses");
                a = NULL;
                break;
              }
          }
        a = n;
      }
    --p->depth;
    return a;
  }
  static mulib_microjs_object_t *
  mulib_microjs_declaration_ (mulib_microjs_parser_ *p)
  {
    int flags = mulib_microjs_word_ (p, "const") ? 1
                : mulib_microjs_word_ (p, "var") ? 2
                                                 : 0;
    mulib_microjs_object_t *o
        = mulib_microjs_node_ (p, MULIB_MICROJS_DECLARE_),
        **tail, *b;
    if (!o || !mulib_microjs_next_ (p))
      return NULL;
    tail = &o->a;
    for (;;)
      {
        if (p->token != MULIB_MICROJS_ID_ || mulib_microjs_reserved_ (p->text))
          {
            mulib_microjs_parse_fail_ (p, "binding identifier required");
            return NULL;
          }
        b = mulib_microjs_node_ (p, MULIB_MICROJS_INIT_);
        if (!b)
          return NULL;
        b->key = p->text;
        b->flags = flags;
        *tail = b;
        tail = &b->next;
        if (!mulib_microjs_next_ (p))
          return NULL;
        if (p->token == '=')
          {
            if (!mulib_microjs_next_ (p)
                || !(b->a = mulib_microjs_expression_ (p, 2)))
              return NULL;
          }
        else if (flags == 1)
          {
            mulib_microjs_parse_fail_ (p, "const requires initializer");
            return NULL;
          }
        if (p->token != ',')
          break;
        if (!mulib_microjs_next_ (p))
          return NULL;
      }
    return o;
  }
  static mulib_microjs_object_t *
  mulib_microjs_statement_ (mulib_microjs_parser_ *p)
  {
    mulib_microjs_object_t *o = NULL, *f;
    int is_for, is_return;
    if (!mulib_microjs_parse_enter_ (p))
      return NULL;
    if (p->token == '{')
      o = mulib_microjs_block_ (p, 1);
    else if (p->token == ';')
      {
        o = mulib_microjs_node_ (p, MULIB_MICROJS_EMPTY_);
        if (!mulib_microjs_next_ (p))
          o = NULL;
      }
    else if (mulib_microjs_word_ (p, "let") || mulib_microjs_word_ (p, "const")
             || mulib_microjs_word_ (p, "var"))
      {
        o = mulib_microjs_declaration_ (p);
        if (o && !mulib_microjs_semi_ (p))
          o = NULL;
      }
    else if (mulib_microjs_word_ (p, "function"))
      {
        o = mulib_microjs_node_ (p, MULIB_MICROJS_DECLARE_);
        f = o ? mulib_microjs_function_ (p, 1) : NULL;
        if (f)
          {
            o->a = mulib_microjs_node_ (p, MULIB_MICROJS_INIT_);
            if (!o->a)
              o = NULL;
            else
              {
                o->a->key = f->key;
                o->a->a = f;
                o->a->flags = 3;
              }
          }
        else
          o = NULL;
      }
    else if (mulib_microjs_word_ (p, "if"))
      {
        o = mulib_microjs_node_ (p, MULIB_MICROJS_IF_);
        if (!o || !mulib_microjs_next_ (p) || !mulib_microjs_expect_ (p, '(')
            || !(o->a = mulib_microjs_expression_ (p, 1))
            || !mulib_microjs_expect_ (p, ')')
            || !(o->b = mulib_microjs_statement_ (p)))
          o = NULL;
        if (o && mulib_microjs_word_ (p, "else"))
          {
            if (!mulib_microjs_next_ (p)
                || !(o->c = mulib_microjs_statement_ (p)))
              o = NULL;
          }
      }
    else if (mulib_microjs_word_ (p, "while")
             || mulib_microjs_word_ (p, "for"))
      {
        is_for = mulib_microjs_word_ (p, "for");
        o = mulib_microjs_node_ (p, is_for ? MULIB_MICROJS_FOR_
                                           : MULIB_MICROJS_WHILE_);
        if (!o || !mulib_microjs_next_ (p) || !mulib_microjs_expect_ (p, '('))
          o = NULL;
        if (o && is_for)
          {
            if (mulib_microjs_word_ (p, "let")
                || mulib_microjs_word_ (p, "const")
                || mulib_microjs_word_ (p, "var"))
              o->a = mulib_microjs_declaration_ (p);
            else if (p->token != ';')
              {
                o->a = mulib_microjs_node_ (p, MULIB_MICROJS_EXPR_);
                if (o->a)
                  o->a->a = mulib_microjs_expression_ (p, 1);
              }
            if (p->vm->error.code || !mulib_microjs_expect_ (p, ';'))
              o = NULL;
          }
        if (o)
          {
            if (!is_for || p->token != ';')
              o->b = mulib_microjs_expression_ (p, 1);
            if (p->vm->error.code)
              o = NULL;
          }
        if (o && is_for)
          {
            if (!mulib_microjs_expect_ (p, ';'))
              o = NULL;
            else if (p->token != ')')
              o->c = mulib_microjs_expression_ (p, 1);
          }
        if (o)
          {
            if (!mulib_microjs_expect_ (p, ')'))
              o = NULL;
            else
              {
                ++p->loops;
                o->d = mulib_microjs_statement_ (p);
                --p->loops;
                if (!o->d)
                  o = NULL;
              }
          }
      }
    else if (mulib_microjs_word_ (p, "return")
             || mulib_microjs_word_ (p, "throw"))
      {
        is_return = mulib_microjs_word_ (p, "return");
        o = mulib_microjs_node_ (p, is_return ? MULIB_MICROJS_RETURN_
                                              : MULIB_MICROJS_THROW_);
        if (is_return && !p->functions)
          {
            mulib_microjs_parse_fail_ (p, "return outside function");
            o = NULL;
          }
        if (o && !mulib_microjs_next_ (p))
          o = NULL;
        if (o && !is_return && p->newline)
          {
            mulib_microjs_parse_fail_ (p, "newline after throw");
            o = NULL;
          }
        if (o && !p->newline && p->token && p->token != ';' && p->token != '}')
          o->a = mulib_microjs_expression_ (p, 1);
        if (o
            && ((!is_return && !o->a) || p->vm->error.code
                || !mulib_microjs_semi_ (p)))
          {
            mulib_microjs_parse_fail_ (p, "throw requires expression");
            o = NULL;
          }
      }
    else if (mulib_microjs_word_ (p, "break")
             || mulib_microjs_word_ (p, "continue"))
      {
        o = mulib_microjs_node_ (p, mulib_microjs_word_ (p, "break")
                                        ? MULIB_MICROJS_BREAK_
                                        : MULIB_MICROJS_CONTINUE_);
        if (!p->loops)
          {
            mulib_microjs_parse_fail_ (p, "loop control outside loop");
            o = NULL;
          }
        if (o && (!mulib_microjs_next_ (p) || !mulib_microjs_semi_ (p)))
          o = NULL;
      }
    else
      {
        o = mulib_microjs_node_ (p, MULIB_MICROJS_EXPR_);
        if (!o || !(o->a = mulib_microjs_expression_ (p, 1))
            || !mulib_microjs_semi_ (p))
          o = NULL;
      }
    --p->depth;
    return o;
  }

  /* Bytecode lowering. Every expression leaves one reference/value handle;
   * GetValue is emitted at the specification's evaluation sequencing points.
   */
  typedef struct mulib_microjs_compile_
  {
    mulib_microjs_parser_ *parser;
    mulib_microjs_object_t *program;
    size_t depth, scope, loop_scope, breaks, continues;
    int in_loop;
  } mulib_microjs_compile_;
  static size_t
  mulib_microjs_emit_ (mulib_microjs_compile_ *c, mulib_minivm_opcode_t op,
                       int64_t arg, mulib_microjs_object_t *node)
  {
    mulib_microjs_object_t *p = c->program;
    mulib_microjs_t *v = c->parser->vm;
    size_t n, old, bytes;
    mulib_minivm_instruction_t *code;
    mulib_microjs_object_t **nodes;
    if (!mulib_microjs_parse_step_ (c->parser))
      return SIZE_MAX;
    if (p->count == p->capacity)
      {
        n = p->capacity ? p->capacity * 2 : 32;
        if (n < p->capacity || n > SIZE_MAX / (sizeof *code + sizeof *nodes)
            || n > INT64_MAX)
          {
            mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0, "bytecode limit");
            return SIZE_MAX;
          }
        bytes = n * sizeof *code;
        code = (mulib_minivm_instruction_t *)mulib_microjs_alloc_ (v, bytes);
        if (!code)
          return SIZE_MAX;
        nodes = (mulib_microjs_object_t **)mulib_microjs_alloc_ (
            v, n * sizeof *nodes);
        if (!nodes)
          {
            mulib_microjs_release_ (v, code, bytes);
            return SIZE_MAX;
          }
        if (p->count)
          {
            memcpy (code, p->code, p->count * sizeof *code);
            memcpy (nodes, p->operations, p->count * sizeof *nodes);
          }
        old = p->capacity * (sizeof *code + sizeof *nodes);
        mulib_microjs_release_ (v, p->code, p->capacity * sizeof *code);
        mulib_microjs_release_ (v, p->operations, p->capacity * sizeof *nodes);
        p->bytes += n * (sizeof *code + sizeof *nodes) - old;
        p->code = code;
        p->operations = nodes;
        p->capacity = n;
      }
    n = p->count++;
    p->code[n].opcode = op;
    p->code[n].operand = op == MULIB_MINIVM_CALL ? (int64_t)n : arg;
    p->operations[n] = node;
    return n;
  }
  static int
  mulib_microjs_host_emit_ (mulib_microjs_compile_ *c,
                            mulib_microjs_object_t *n)
  {
    return mulib_microjs_emit_ (c, MULIB_MINIVM_CALL, 0, n) != SIZE_MAX;
  }
  static mulib_microjs_object_t *
  mulib_microjs_operation_ (mulib_microjs_compile_ *c, int op,
                            mulib_microjs_object_t *a)
  {
    mulib_microjs_object_t *o = mulib_microjs_node_ (c->parser, op);
    if (o)
      {
        o->a = a;
        if (a)
          o->offset = a->offset;
      }
    return o;
  }
  static int
  mulib_microjs_simple_ (mulib_microjs_compile_ *c, int op)
  {
    mulib_microjs_object_t *o = mulib_microjs_operation_ (c, op, NULL);
    return o && mulib_microjs_host_emit_ (c, o);
  }
  static void
  mulib_microjs_target_ (mulib_microjs_compile_ *c, size_t at)
  {
    if (at != SIZE_MAX)
      c->program->code[at].operand = (int64_t)c->program->count;
  }
  static int mulib_microjs_compile_expr_ (mulib_microjs_compile_ *,
                                          mulib_microjs_object_t *, int);
  static int mulib_microjs_compile_stmt_ (mulib_microjs_compile_ *,
                                          mulib_microjs_object_t *);
  static mulib_microjs_object_t *
  mulib_microjs_compile_program_ (mulib_microjs_parser_ *,
                                  mulib_microjs_object_t *, int);
  static int
  mulib_microjs_compile_expr_ (mulib_microjs_compile_ *c,
                               mulib_microjs_object_t *n, int get)
  {
    mulib_microjs_object_t *q;
    size_t branch, end;
    int op, ok = 0;
    if (!n)
      return mulib_microjs_simple_ (c, MULIB_MICROJS_LITERAL_);
    if (c->depth++ >= c->parser->vm->limits.depth)
      {
        --c->depth;
        return mulib_microjs_fail_ (c->parser->vm, MULIB_MICROJS_LIMIT,
                                    n->offset, "compiler depth limit");
      }
    op = n->op;
    switch (op)
      {
      case MULIB_MICROJS_LITERAL_:
      case MULIB_MICROJS_ID_:
      case MULIB_MICROJS_THIS_:
        break;
      case MULIB_MICROJS_FUNCTION_:
        if (!n->c)
          n->c = mulib_microjs_compile_program_ (c->parser, n->b, 1);
        if (!n->c)
          goto done;
        break;
      case MULIB_MICROJS_ARRAY_:
      case MULIB_MICROJS_OBJECT_:
        /* The object is allocated before any initializer executes. */
        if (!mulib_microjs_host_emit_ (c, n))
          goto done;
        for (q = n->a; q; q = q->next)
          {
            if (mulib_microjs_emit_ (c, MULIB_MINIVM_DUP, 0, NULL) == SIZE_MAX
                || !mulib_microjs_compile_expr_ (c, q->a, 1)
                || !mulib_microjs_host_emit_ (c, q)
                || mulib_microjs_emit_ (c, MULIB_MINIVM_DROP, 0, NULL)
                       == SIZE_MAX)
              goto done;
          }
        ok = 1;
        goto done;
      case MULIB_MICROJS_INVOKE_:
        if (!mulib_microjs_compile_expr_ (c, n->a, 0))
          goto done;
        /* Capture the function value before arguments can change its binding,
         * retaining the reference base for this. */
        if (!mulib_microjs_simple_ (c, MULIB_MICROJS_ARG_))
          goto done;
        for (q = n->b; q; q = q->next)
          if (!mulib_microjs_compile_expr_ (c, q, 1))
            goto done;
        break;
      case MULIB_MICROJS_MEMBER_:
        if (!mulib_microjs_compile_expr_ (c, n->a, 1)
            || !mulib_microjs_compile_expr_ (c, n->b, 1))
          goto done;
        break;
      case MULIB_MICROJS_AND_:
      case MULIB_MICROJS_OR_:
      case MULIB_MICROJS_NULLISH_:
        if (!mulib_microjs_compile_expr_ (c, n->a, 1)
            || mulib_microjs_emit_ (c, MULIB_MINIVM_DUP, 0, NULL) == SIZE_MAX
            || !mulib_microjs_simple_ (c, op == MULIB_MICROJS_NULLISH_
                                              ? MULIB_MICROJS_ISNULL_
                                              : MULIB_MICROJS_TRUTH_))
          goto done;
        branch = mulib_microjs_emit_ (
            c, op == MULIB_MICROJS_OR_ ? MULIB_MINIVM_JNZ : MULIB_MINIVM_JZ, 0,
            NULL);
        if (branch == SIZE_MAX
            || mulib_microjs_emit_ (c, MULIB_MINIVM_DROP, 0, NULL) == SIZE_MAX
            || !mulib_microjs_compile_expr_ (c, n->b, 1))
          goto done;
        mulib_microjs_target_ (c, branch);
        ok = 1;
        goto done;
      case '?':
        if (!mulib_microjs_compile_expr_ (c, n->a, 1)
            || !mulib_microjs_simple_ (c, MULIB_MICROJS_TRUTH_))
          goto done;
        branch = mulib_microjs_emit_ (c, MULIB_MINIVM_JZ, 0, NULL);
        if (branch == SIZE_MAX || !mulib_microjs_compile_expr_ (c, n->b, 1))
          goto done;
        end = mulib_microjs_emit_ (c, MULIB_MINIVM_JMP, 0, NULL);
        mulib_microjs_target_ (c, branch);
        if (end == SIZE_MAX || !mulib_microjs_compile_expr_ (c, n->c, 1))
          goto done;
        mulib_microjs_target_ (c, end);
        ok = 1;
        goto done;
      case ',':
        if (!mulib_microjs_compile_expr_ (c, n->a, 1)
            || mulib_microjs_emit_ (c, MULIB_MINIVM_DROP, 0, NULL) == SIZE_MAX
            || !mulib_microjs_compile_expr_ (c, n->b, 1))
          goto done;
        ok = 1;
        goto done;
      case '=':
      case MULIB_MICROJS_ADDSET_:
      case MULIB_MICROJS_SUBSET_:
      case MULIB_MICROJS_MULSET_:
      case MULIB_MICROJS_DIVSET_:
      case MULIB_MICROJS_MODSET_:
        if (!mulib_microjs_compile_expr_ (c, n->a, 0))
          goto done;
        if (op != '=' && !mulib_microjs_simple_ (c, MULIB_MICROJS_ARG_))
          goto done;
        if (!mulib_microjs_compile_expr_ (c, n->b, 1))
          goto done;
        break;
      case MULIB_MICROJS_TYPEOF_:
      case MULIB_MICROJS_INC_:
      case MULIB_MICROJS_DEC_:
      case MULIB_MICROJS_POSTINC_:
      case MULIB_MICROJS_POSTDEC_:
        if (!mulib_microjs_compile_expr_ (c, n->a, 0))
          goto done;
        break;
      default:
        if (!mulib_microjs_compile_expr_ (c, n->a, 1))
          goto done;
        if (n->b && !mulib_microjs_compile_expr_ (c, n->b, 1))
          goto done;
        break;
      }
    if (!mulib_microjs_host_emit_ (c, n))
      goto done;
    if (get && (op == MULIB_MICROJS_ID_ || op == MULIB_MICROJS_MEMBER_)
        && !mulib_microjs_simple_ (c, MULIB_MICROJS_GET_))
      goto done;
    ok = 1;
  done:
    --c->depth;
    return ok;
  }
  static int
  mulib_microjs_compile_enter_ (mulib_microjs_compile_ *c,
                                mulib_microjs_object_t *block, int flags)
  {
    mulib_microjs_object_t *o
        = mulib_microjs_operation_ (c, MULIB_MICROJS_ENTER_, block);
    if (!o)
      return 0;
    o->flags = flags;
    return mulib_microjs_host_emit_ (c, o)
           && mulib_microjs_emit_ (c, MULIB_MINIVM_DROP, 0, NULL) != SIZE_MAX;
  }
  static int
  mulib_microjs_compile_leave_ (mulib_microjs_compile_ *c)
  {
    return mulib_microjs_simple_ (c, MULIB_MICROJS_LEAVE_)
           && mulib_microjs_emit_ (c, MULIB_MINIVM_DROP, 0, NULL) != SIZE_MAX;
  }
  static void
  mulib_microjs_patch_chain_ (mulib_microjs_compile_ *c, size_t chain,
                              size_t target)
  {
    while (chain != SIZE_MAX)
      {
        size_t next = (size_t)c->program->code[chain].operand;
        c->program->code[chain].operand = (int64_t)target;
        chain = next;
      }
  }
  static int
  mulib_microjs_compile_stmt_ (mulib_microjs_compile_ *c,
                               mulib_microjs_object_t *n)
  {
    mulib_microjs_object_t *q;
    size_t branch, end, start, old_b, old_c, old_s, i;
    int old_loop;
    if (!n)
      return 1;
    if (c->depth++ >= c->parser->vm->limits.depth)
      {
        --c->depth;
        return mulib_microjs_fail_ (c->parser->vm, MULIB_MICROJS_LIMIT,
                                    n->offset,
                                    "statement compiler depth limit");
      }
    switch (n->op)
      {
      case MULIB_MICROJS_BLOCK_:
        if (!mulib_microjs_compile_enter_ (c, n, 0))
          goto fail;
        ++c->scope;
        for (q = n->a; q; q = q->next)
          if (!mulib_microjs_compile_stmt_ (c, q))
            goto fail;
        --c->scope;
        if (!mulib_microjs_compile_leave_ (c))
          goto fail;
        break;
      case MULIB_MICROJS_DECLARE_:
        for (q = n->a; q; q = q->next)
          {
            if (q->flags == 3)
              {
                if (!q->a->c)
                  q->a->c
                      = mulib_microjs_compile_program_ (c->parser, q->a->b, 1);
                if (!q->a->c)
                  goto fail;
                continue;
              }
            if (q->flags == 2 && !q->a)
              continue;
            if (!mulib_microjs_compile_expr_ (c, q->a, 1)
                || !mulib_microjs_host_emit_ (c, q)
                || mulib_microjs_emit_ (c, MULIB_MINIVM_DROP, 0, NULL)
                       == SIZE_MAX)
              goto fail;
          }
        break;
      case MULIB_MICROJS_EXPR_:
        if (!mulib_microjs_compile_expr_ (c, n->a, 1)
            || !mulib_microjs_simple_ (c, MULIB_MICROJS_LAST_)
            || mulib_microjs_emit_ (c, MULIB_MINIVM_DROP, 0, NULL) == SIZE_MAX)
          goto fail;
        break;
      case MULIB_MICROJS_RETURN_:
      case MULIB_MICROJS_THROW_:
        if (!mulib_microjs_compile_expr_ (c, n->a, 1))
          goto fail;
        if (n->op == MULIB_MICROJS_THROW_)
          {
            if (!mulib_microjs_host_emit_ (c, n))
              goto fail;
          }
        else if (mulib_microjs_emit_ (c, MULIB_MINIVM_HALT, 0, NULL)
                 == SIZE_MAX)
          goto fail;
        break;
      case MULIB_MICROJS_IF_:
        if (!mulib_microjs_compile_expr_ (c, n->a, 1)
            || !mulib_microjs_simple_ (c, MULIB_MICROJS_TRUTH_))
          goto fail;
        branch = mulib_microjs_emit_ (c, MULIB_MINIVM_JZ, 0, NULL);
        if (branch == SIZE_MAX || !mulib_microjs_compile_stmt_ (c, n->b))
          goto fail;
        end = mulib_microjs_emit_ (c, MULIB_MINIVM_JMP, 0, NULL);
        mulib_microjs_target_ (c, branch);
        if (end == SIZE_MAX || !mulib_microjs_compile_stmt_ (c, n->c))
          goto fail;
        mulib_microjs_target_ (c, end);
        break;
      case MULIB_MICROJS_WHILE_:
      case MULIB_MICROJS_FOR_:
        old_b = c->breaks;
        old_c = c->continues;
        old_s = c->loop_scope;
        old_loop = c->in_loop;
        c->breaks = c->continues = SIZE_MAX;
        c->in_loop = 1;
        /* A for environment owns lexical initializers. Each iteration is
         * cloned before the increment, so closures retain the correct
         * iteration binding. */
        if (n->op == MULIB_MICROJS_FOR_)
          {
            if (!mulib_microjs_compile_enter_ (c, n, 2))
              goto fail;
            ++c->scope;
            if (!mulib_microjs_compile_stmt_ (c, n->a))
              goto fail;
            if (!mulib_microjs_compile_enter_ (c, n, 3))
              goto fail;
          }
        c->loop_scope = c->scope;
        start = c->program->count;
        if (n->b)
          {
            if (!mulib_microjs_compile_expr_ (c, n->b, 1)
                || !mulib_microjs_simple_ (c, MULIB_MICROJS_TRUTH_))
              goto fail;
          }
        else if (mulib_microjs_emit_ (c, MULIB_MINIVM_PUSH, 1, NULL)
                 == SIZE_MAX)
          goto fail;
        branch = mulib_microjs_emit_ (c, MULIB_MINIVM_JZ, 0, NULL);
        if (branch == SIZE_MAX || !mulib_microjs_compile_stmt_ (c, n->d))
          goto fail;
        mulib_microjs_patch_chain_ (c, c->continues, c->program->count);
        if (n->op == MULIB_MICROJS_FOR_)
          {
            if (!mulib_microjs_compile_enter_ (c, n, 3))
              goto fail;
            if (n->c
                && (!mulib_microjs_compile_expr_ (c, n->c, 1)
                    || mulib_microjs_emit_ (c, MULIB_MINIVM_DROP, 0, NULL)
                           == SIZE_MAX))
              goto fail;
          }
        if (mulib_microjs_emit_ (c, MULIB_MINIVM_JMP, (int64_t)start, NULL)
            == SIZE_MAX)
          goto fail;
        mulib_microjs_target_ (c, branch);
        mulib_microjs_patch_chain_ (c, c->breaks, c->program->count);
        if (n->op == MULIB_MICROJS_FOR_)
          {
            --c->scope;
            if (!mulib_microjs_compile_leave_ (c))
              goto fail;
          }
        c->breaks = old_b;
        c->continues = old_c;
        c->loop_scope = old_s;
        c->in_loop = old_loop;
        break;
      case MULIB_MICROJS_BREAK_:
      case MULIB_MICROJS_CONTINUE_:
        for (i = c->scope; i > c->loop_scope; --i)
          if (!mulib_microjs_compile_leave_ (c))
            goto fail;
        branch = mulib_microjs_emit_ (c, MULIB_MINIVM_JMP,
                                      (int64_t)(n->op == MULIB_MICROJS_BREAK_
                                                    ? c->breaks
                                                    : c->continues),
                                      NULL);
        if (branch == SIZE_MAX)
          goto fail;
        if (n->op == MULIB_MICROJS_BREAK_)
          c->breaks = branch;
        else
          c->continues = branch;
        break;
      case MULIB_MICROJS_EMPTY_:
        break;
      default:
        mulib_microjs_fail_ (c->parser->vm, MULIB_MICROJS_SYNTAX, n->offset,
                             "unsupported statement");
        goto fail;
      }
    --c->depth;
    return 1;
  fail:
    --c->depth;
    return 0;
  }
  static mulib_microjs_object_t *
  mulib_microjs_compile_program_ (mulib_microjs_parser_ *p,
                                  mulib_microjs_object_t *block, int function)
  {
    mulib_microjs_compile_ c;
    mulib_microjs_object_t *q, *last;
    memset (&c, 0, sizeof c);
    c.parser = p;
    c.breaks = c.continues = SIZE_MAX;
    c.program = mulib_microjs_cell_ (p->vm, MULIB_MICROJS_P_);
    if (!c.program)
      return NULL;
    c.program->a = block;
    if (!mulib_microjs_compile_enter_ (&c, block, 1))
      return NULL;
    for (q = block->a; q; q = q->next)
      if (!mulib_microjs_compile_stmt_ (&c, q))
        return NULL;
    last = mulib_microjs_operation_ (
        &c, function ? MULIB_MICROJS_LITERAL_ : MULIB_MICROJS_LAST_, NULL);
    if (!last)
      return NULL;
    last->flags = 1;
    if (!mulib_microjs_host_emit_ (&c, last)
        || mulib_microjs_emit_ (&c, MULIB_MINIVM_HALT, 0, NULL) == SIZE_MAX)
      return NULL;
    return c.program;
  }

  /* Static semantics are checked for the entire source, including untaken
   * branches, before any bytecode runs. Each scan consumes compiler fuel. */
  static int
  mulib_microjs_has_var_ (mulib_microjs_parser_ *p, mulib_microjs_object_t *n,
                          mulib_microjs_object_t *key)
  {
    mulib_microjs_object_t *q;
    if (!n || !mulib_microjs_parse_step_ (p))
      return 0;
    if (n->op == MULIB_MICROJS_DECLARE_)
      {
        for (q = n->a; q; q = q->next)
          if (q->flags == 2 && mulib_microjs_string_equal_ (q->key, key))
            return 1;
      }
    else if (n->op == MULIB_MICROJS_BLOCK_)
      {
        for (q = n->a; q; q = q->next)
          if (mulib_microjs_has_var_ (p, q, key))
            return 1;
      }
    else if (n->op == MULIB_MICROJS_IF_)
      return mulib_microjs_has_var_ (p, n->b, key)
             || mulib_microjs_has_var_ (p, n->c, key);
    else if (n->op == MULIB_MICROJS_WHILE_ || n->op == MULIB_MICROJS_FOR_)
      return mulib_microjs_has_var_ (p, n->a, key)
             || mulib_microjs_has_var_ (p, n->d, key);
    return 0;
  }
  static int
  mulib_microjs_lexical_statement_ (mulib_microjs_object_t *n)
  {
    return n && n->op == MULIB_MICROJS_DECLARE_ && n->a && n->a->flags != 2;
  }
  static int
  mulib_microjs_validate_ (mulib_microjs_parser_ *p, mulib_microjs_object_t *n,
                           size_t depth)
  {
    mulib_microjs_object_t *s, *a, *t, *b;
    if (!n)
      return 1;
    if (depth > p->vm->limits.depth)
      return mulib_microjs_fail_ (p->vm, MULIB_MICROJS_LIMIT, n->offset,
                                  "static semantics depth limit");
    for (; n; n = n->next)
      {
        if (!mulib_microjs_parse_step_ (p))
          return 0;
        /* A declaration list also occurs in a for header, outside the block
         * statement lists checked below. Reject duplicates before execution. */
        if (n->op == MULIB_MICROJS_DECLARE_)
          for (a = n->a; a; a = a->next)
            if (a->flags != 2)
              for (b = a->next; b; b = b->next)
                {
                  if (!mulib_microjs_parse_step_ (p))
                    return 0;
                  if (mulib_microjs_string_equal_ (a->key, b->key))
                    return mulib_microjs_fail_ (
                        p->vm, MULIB_MICROJS_SYNTAX, b->offset,
                        "duplicate lexical declaration");
                }
        if (n->op == MULIB_MICROJS_BLOCK_)
          {
            for (s = n->a; s; s = s->next)
              if (s->op == MULIB_MICROJS_DECLARE_)
                {
                  for (a = s->a; a; a = a->next)
                    if (a->flags != 2)
                      {
                        if (mulib_microjs_has_var_ (p, n, a->key))
                          return mulib_microjs_fail_ (
                              p->vm, MULIB_MICROJS_SYNTAX, a->offset,
                              "lexical/var declaration conflict");
                        for (t = s; t; t = t->next)
                          if (t->op == MULIB_MICROJS_DECLARE_)
                            {
                              for (b = t == s ? a->next : t->a; b; b = b->next)
                                {
                                  if (!mulib_microjs_parse_step_ (p))
                                    return 0;
                                  if (mulib_microjs_string_equal_ (a->key,
                                                                   b->key))
                                    return mulib_microjs_fail_ (
                                        p->vm, MULIB_MICROJS_SYNTAX, b->offset,
                                        "duplicate lexical declaration");
                                }
                            }
                      }
                }
          }
        if (n->op == MULIB_MICROJS_FUNCTION_)
          {
            for (a = n->a; a; a = a->next)
              for (s = n->b->a; s; s = s->next)
                if (s->op == MULIB_MICROJS_DECLARE_)
                  for (b = s->a; b; b = b->next)
                    {
                      if (!mulib_microjs_parse_step_ (p))
                        return 0;
                      if (b->flags != 2
                          && mulib_microjs_string_equal_ (a->key, b->key))
                        return mulib_microjs_fail_ (
                            p->vm, MULIB_MICROJS_SYNTAX, b->offset,
                            "parameter/lexical declaration conflict");
                    }
          }
        if ((n->op == MULIB_MICROJS_IF_
             && (mulib_microjs_lexical_statement_ (n->b)
                 || mulib_microjs_lexical_statement_ (n->c)))
            || ((n->op == MULIB_MICROJS_WHILE_ || n->op == MULIB_MICROJS_FOR_)
                && mulib_microjs_lexical_statement_ (n->d)))
          return mulib_microjs_fail_ (p->vm, MULIB_MICROJS_SYNTAX, n->offset,
                                      "lexical declaration requires block");
        if (n->op == MULIB_MICROJS_FOR_
            && mulib_microjs_lexical_statement_ (n->a))
          for (a = n->a->a; a; a = a->next)
            if (mulib_microjs_has_var_ (p, n->d, a->key))
              return mulib_microjs_fail_ (p->vm, MULIB_MICROJS_SYNTAX,
                                          a->offset,
                                          "for binding conflicts with var");
        if (!mulib_microjs_validate_ (p, n->a, depth + 1)
            || !mulib_microjs_validate_ (p, n->b, depth + 1)
            || !mulib_microjs_validate_ (p, n->c, depth + 1)
            || !mulib_microjs_validate_ (p, n->d, depth + 1))
          return 0;
      }
    return !p->vm->error.code;
  }
  static int
  mulib_microjs_global_check_ (mulib_microjs_parser_ *p,
                               mulib_microjs_object_t *block)
  {
    mulib_microjs_object_t *s, *a, *b;
    for (s = block->a; s; s = s->next)
      if (s->op == MULIB_MICROJS_DECLARE_)
        for (a = s->a; a; a = a->next)
          {
            b = mulib_microjs_find_ (p->vm->global, a->key, 0);
            if (b && (a->flags != 2 || b->flags != 2))
              return mulib_microjs_fail_ (p->vm, MULIB_MICROJS_SYNTAX,
                                          a->offset, "global redeclaration");
          }
    for (b = p->vm->global->a; b; b = b->next)
      if (b->flags != 2 && mulib_microjs_has_var_ (p, block, b->key))
        return mulib_microjs_fail_ (p->vm, MULIB_MICROJS_SYNTAX, 0,
                                    "global var/lexical conflict");
    return !p->vm->error.code;
  }

  static int
  mulib_microjs_get_ (mulib_microjs_t *v, mulib_microjs_ref_ r,
                      mulib_microjs_v_ *out)
  {
    if (r.reference == 1)
      {
        if (!r.base)
          return mulib_microjs_fail_ (v, MULIB_MICROJS_NAME, 0,
                                      "unresolvable reference");
        if (!r.base->initialized)
          return mulib_microjs_fail_ (v, MULIB_MICROJS_NAME, 0,
                                      "binding in temporal dead zone");
        *out = r.base->value;
        return 1;
      }
    if (r.reference == 2)
      return mulib_microjs_property_ (v, r.value, r.key, out);
    *out = r.value;
    return 1;
  }
  static int
  mulib_microjs_put_ (mulib_microjs_run_ *r, mulib_microjs_ref_ ref,
                      mulib_microjs_v_ x)
  {
    mulib_microjs_object_t *b = ref.base;
    if (ref.reference == 1 || ref.reference == 3)
      {
        if (!b)
          return mulib_microjs_fail_ (r->vm, MULIB_MICROJS_NAME, 0,
                                      "assignment to unresolvable reference");
        if (!b->initialized)
          return mulib_microjs_fail_ (r->vm, MULIB_MICROJS_NAME, 0,
                                      "binding in temporal dead zone");
        if (b->flags & 1)
          return mulib_microjs_fail_ (r->vm, MULIB_MICROJS_TYPE, 0,
                                      "assignment to constant binding");
        b->value = x;
        return 1;
      }
    if (ref.reference == 2 || ref.reference == 4)
      return mulib_microjs_put_property_ (r->vm, b, ref.key, x);
    return mulib_microjs_fail_ (r->vm, MULIB_MICROJS_NAME, 0,
                                "invalid reference");
  }
  static int
  mulib_microjs_strict_equal_ (mulib_microjs_v_ a, mulib_microjs_v_ b)
  {
    if (a.type != b.type)
      return 0;
    switch (a.type)
      {
      case MULIB_MICROJS_NULL:
      case MULIB_MICROJS_UNDEFINED:
        return 1;
      case MULIB_MICROJS_NUMBER:
      case MULIB_MICROJS_BOOLEAN:
        return a.number == b.number;
      case MULIB_MICROJS_STRING:
        return mulib_microjs_string_equal_ (a.object, b.object);
      default:
        return a.object == b.object;
      }
  }
  static int
  mulib_microjs_loose_equal_ (mulib_microjs_v_ a, mulib_microjs_v_ b)
  {
    if (a.type == b.type)
      return mulib_microjs_strict_equal_ (a, b);
    if ((a.type == MULIB_MICROJS_NULL || a.type == MULIB_MICROJS_UNDEFINED)
        && (b.type == MULIB_MICROJS_NULL || b.type == MULIB_MICROJS_UNDEFINED))
      return 1;
    if (a.type == MULIB_MICROJS_BOOLEAN)
      a = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER, a.number);
    if (b.type == MULIB_MICROJS_BOOLEAN)
      b = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER, b.number);
    if ((a.type == MULIB_MICROJS_NUMBER && b.type == MULIB_MICROJS_STRING)
        || (a.type == MULIB_MICROJS_STRING && b.type == MULIB_MICROJS_NUMBER)
        || (a.type == MULIB_MICROJS_NUMBER && b.type == MULIB_MICROJS_NUMBER))
      return mulib_microjs_number_ (a) == mulib_microjs_number_ (b);
    return 0;
  }
  static uint32_t
  mulib_microjs_uint32_ (double d)
  {
    double n;
    if (!isfinite (d) || d == 0)
      return 0;
    n = fmod (trunc (d), 4294967296.0);
    if (n < 0)
      n += 4294967296.0;
    return (uint32_t)n;
  }
  static double
  mulib_microjs_int32_ (uint32_t u)
  {
    return u >= UINT32_C (2147483648) ? (double)u - 4294967296.0 : (double)u;
  }
  static int
  mulib_microjs_binary_ (mulib_microjs_t *v, int op, mulib_microjs_v_ a,
                         mulib_microjs_v_ b, mulib_microjs_v_ *out)
  {
    double x, y, z = 0;
    uint32_t u, w, shift;
    int eq, cmp;
    size_t i, n;
    mulib_microjs_object_t *s, *t, *joined;
    if (op == MULIB_MICROJS_SEQ_ || op == MULIB_MICROJS_SNE_
        || op == MULIB_MICROJS_EQ_ || op == MULIB_MICROJS_NE_)
      {
        eq = op == MULIB_MICROJS_SEQ_ || op == MULIB_MICROJS_SNE_
                 ? mulib_microjs_strict_equal_ (a, b)
                 : mulib_microjs_loose_equal_ (a, b);
        *out = mulib_microjs_scalar_ (
            MULIB_MICROJS_BOOLEAN,
            op == MULIB_MICROJS_SNE_ || op == MULIB_MICROJS_NE_ ? !eq : eq);
        return 1;
      }
    if (op == '+'
        && (a.type == MULIB_MICROJS_STRING || b.type == MULIB_MICROJS_STRING))
      {
        s = mulib_microjs_text_ (v, a);
        t = mulib_microjs_text_ (v, b);
        if (!s || !t)
          return 0;
        if (t->length > SIZE_MAX - s->length)
          return mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0,
                                      "string concatenation overflow");
        joined = mulib_microjs_string_ (v, NULL, s->length + t->length);
        if (!joined)
          return 0;
        if (s->length)
          memcpy (joined->units, s->units, s->length * sizeof *s->units);
        if (t->length)
          memcpy (joined->units + s->length, t->units,
                  t->length * sizeof *t->units);
        *out = mulib_microjs_object_value_ (joined);
        return 1;
      }
    if (op == '<' || op == '>' || op == MULIB_MICROJS_LE_
        || op == MULIB_MICROJS_GE_)
      {
        if (a.type == MULIB_MICROJS_STRING && b.type == MULIB_MICROJS_STRING)
          {
            s = a.object;
            t = b.object;
            n = s->length < t->length ? s->length : t->length;
            cmp = 0;
            for (i = 0; i < n; ++i)
              if (s->units[i] != t->units[i])
                {
                  cmp = s->units[i] < t->units[i] ? -1 : 1;
                  break;
                }
            if (!cmp)
              cmp = s->length < t->length ? -1 : s->length > t->length;
            eq = op == '<'                 ? cmp < 0
                 : op == '>'               ? cmp > 0
                 : op == MULIB_MICROJS_LE_ ? cmp <= 0
                                           : cmp >= 0;
          }
        else
          {
            x = mulib_microjs_number_ (a);
            y = mulib_microjs_number_ (b);
            eq = op == '<'                 ? x < y
                 : op == '>'               ? x > y
                 : op == MULIB_MICROJS_LE_ ? x <= y
                                           : x >= y;
          }
        *out = mulib_microjs_scalar_ (MULIB_MICROJS_BOOLEAN, eq);
        return 1;
      }
    x = mulib_microjs_number_ (a);
    y = mulib_microjs_number_ (b);
    switch (op)
      {
      case '+':
        z = x + y;
        break;
      case '-':
        z = x - y;
        break;
      case '*':
        z = x * y;
        break;
      case '/':
        if (isnan (x) || isnan (y) || (isinf (x) && isinf (y))
            || (x == 0 && y == 0))
          z = NAN;
        else if (y == 0)
          z = signbit (x) != signbit (y) ? -INFINITY : INFINITY;
        else
          z = x / y;
        break;
      case '%':
        z = !isfinite (x) || isnan (y) || y == 0 ? NAN
            : isinf (y)                          ? x
                                                 : fmod (x, y);
        break;
      case MULIB_MICROJS_POW_:
        if (y == 0)
          z = 1;
        else if (isnan (y) || (isinf (y) && fabs (x) == 1))
          z = NAN;
        else
          z = pow (x, y);
        break;
      case '&':
      case '|':
      case '^':
      case MULIB_MICROJS_SHL_:
      case MULIB_MICROJS_SHR_:
      case MULIB_MICROJS_USHR_:
        u = mulib_microjs_uint32_ (x);
        w = mulib_microjs_uint32_ (y);
        shift = w & 31;
        if (op == '&')
          u &= w;
        else if (op == '|')
          u |= w;
        else if (op == '^')
          u ^= w;
        else if (op == MULIB_MICROJS_SHL_)
          u <<= shift;
        else if (op == MULIB_MICROJS_SHR_)
          {
            if (shift)
              u = (u >> shift)
                  | (u & UINT32_C (0x80000000) ? UINT32_MAX << (32 - shift)
                                               : 0);
          }
        else
          u >>= shift;
        z = op == MULIB_MICROJS_USHR_ ? (double)u : mulib_microjs_int32_ (u);
        break;
      default:
        return mulib_microjs_fail_ (v, MULIB_MICROJS_TYPE, 0,
                                    "unsupported operation");
      }
    *out = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER, z);
    return 1;
  }
  static int
  mulib_microjs_slot_ (mulib_microjs_run_ *r, mulib_microjs_ref_ value,
                       int64_t *out)
  {
    size_t n;
    mulib_microjs_ref_ *p;
    if (r->count == r->capacity)
      {
        n = r->capacity ? r->capacity * 2 : 32;
        if (n < r->capacity || n > INT64_MAX || n > SIZE_MAX / sizeof *p)
          return mulib_microjs_fail_ (r->vm, MULIB_MICROJS_LIMIT, 0,
                                      "value table limit");
        p = (mulib_microjs_ref_ *)mulib_microjs_alloc_ (r->vm, n * sizeof *p);
        if (!p)
          return 0;
        if (r->count)
          memcpy (p, r->values, r->count * sizeof *p);
        mulib_microjs_release_ (r->vm, r->values, r->capacity * sizeof *p);
        r->values = p;
        r->capacity = n;
      }
    *out = (int64_t)r->count;
    r->values[r->count++] = value;
    return 1;
  }
  static mulib_microjs_object_t *
  mulib_microjs_closure_ (mulib_microjs_t *v, mulib_microjs_object_t *node,
                          mulib_microjs_object_t *env, int self)
  {
    mulib_microjs_object_t *f = mulib_microjs_cell_ (v, MULIB_MICROJS_F_),
                           *scope, *b;
    if (!f)
      return NULL;
    f->c = node;
    f->b = env;
    if (self && node->key)
      {
        scope = mulib_microjs_cell_ (v, MULIB_MICROJS_E_);
        if (!scope)
          return NULL;
        scope->b = env;
        f->b = scope;
        b = mulib_microjs_binding_ (v, scope, node->key, 1, 1);
        if (!b)
          return NULL;
        b->value = mulib_microjs_object_value_ (f);
      }
    return f;
  }
  /* VarScopedDeclarations, excluding nested function bodies. */
  static int
  mulib_microjs_hoist_ (mulib_microjs_t *v, mulib_microjs_object_t *env,
                        mulib_microjs_object_t *n, size_t depth)
  {
    mulib_microjs_object_t *q, *b;
    if (!n)
      return 1;
    if (depth > v->limits.depth)
      return mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, n->offset,
                                  "scope depth limit");
    switch (n->op)
      {
      case MULIB_MICROJS_DECLARE_:
        for (q = n->a; q; q = q->next)
          if (q->flags == 2)
            {
              b = mulib_microjs_find_ (env, q->key, 0);
              if (b && b->flags != 2)
                return mulib_microjs_fail_ (
                    v, MULIB_MICROJS_SYNTAX, q->offset,
                    "var conflicts with lexical binding");
              if (!b && !mulib_microjs_binding_ (v, env, q->key, 2, 1))
                return 0;
            }
        break;
      case MULIB_MICROJS_BLOCK_:
        for (q = n->a; q; q = q->next)
          if (!mulib_microjs_hoist_ (v, env, q, depth + 1))
            return 0;
        break;
      case MULIB_MICROJS_IF_:
        return mulib_microjs_hoist_ (v, env, n->b, depth + 1)
               && mulib_microjs_hoist_ (v, env, n->c, depth + 1);
      case MULIB_MICROJS_WHILE_:
      case MULIB_MICROJS_FOR_:
        return mulib_microjs_hoist_ (v, env, n->a, depth + 1)
               && mulib_microjs_hoist_ (v, env, n->d, depth + 1);
      default:
        break;
      }
    return 1;
  }
  static int
  mulib_microjs_declare_ (mulib_microjs_t *v, mulib_microjs_object_t *env,
                          mulib_microjs_object_t *stmt)
  {
    mulib_microjs_object_t *q, *b, *f;
    if (!stmt || stmt->op != MULIB_MICROJS_DECLARE_)
      return 1;
    for (q = stmt->a; q; q = q->next)
      if (q->flags != 2)
        {
          if (mulib_microjs_find_ (env, q->key, 0))
            return mulib_microjs_fail_ (v, MULIB_MICROJS_SYNTAX, q->offset,
                                        "duplicate lexical declaration");
          b = mulib_microjs_binding_ (v, env, q->key, q->flags == 1 ? 1 : 0,
                                      q->flags == 3);
          if (!b)
            return 0;
          if (q->flags == 3)
            {
              f = mulib_microjs_closure_ (v, q->a, env, 0);
              if (!f)
                return 0;
              b->value = mulib_microjs_object_value_ (f);
            }
        }
    return 1;
  }
  static int
  mulib_microjs_enter_ (mulib_microjs_run_ *r, mulib_microjs_object_t *n)
  {
    mulib_microjs_t *v = r->vm;
    mulib_microjs_object_t *env, *q, *b, *copy;
    if (n->flags == 3)
      {
        if (!n->a->a || n->a->a->op != MULIB_MICROJS_DECLARE_ || !n->a->a->a
            || n->a->a->a->flags == 2)
          return 1;
        env = mulib_microjs_cell_ (v, MULIB_MICROJS_E_);
        if (!env)
          return 0;
        env->b = r->environment->b;
        for (b = r->environment->a; b; b = b->next)
          {
            copy = mulib_microjs_binding_ (v, env, b->key, b->flags,
                                           b->initialized);
            if (!copy)
              return 0;
            copy->value = b->value;
          }
        r->environment = env;
        return 1;
      }
    if (n->flags == 1)
      env = r->environment;
    else
      {
        env = mulib_microjs_cell_ (v, MULIB_MICROJS_E_);
        if (!env)
          return 0;
        env->b = r->environment;
        r->environment = env;
      }
    if (n->flags == 2)
      return mulib_microjs_declare_ (v, env, n->a->a);
    if (n->flags == 1 && !mulib_microjs_hoist_ (v, env, n->a, 0))
      return 0;
    for (q = n->a->a; q; q = q->next)
      if (!mulib_microjs_declare_ (v, env, q))
        return 0;
    return 1;
  }
  static int mulib_microjs_execute_ (mulib_microjs_t *,
                                     mulib_microjs_object_t *,
                                     mulib_microjs_object_t *,
                                     mulib_microjs_v_, mulib_microjs_v_ *);
  static int
  mulib_microjs_invoke_ (mulib_microjs_t *v, mulib_microjs_v_ f,
                         mulib_microjs_v_ this_value,
                         const mulib_microjs_v_ *args, size_t count,
                         mulib_microjs_v_ *out)
  {
    mulib_microjs_object_t *env, *param, *b;
    size_t i, bytes = 0;
    mulib_microjs_value_t *public_args = NULL, result = { 0 };
    mulib_microjs_status_t status;
    int ok = 0;
    if (f.type != MULIB_MICROJS_FUNCTION)
      return mulib_microjs_fail_ (v, MULIB_MICROJS_TYPE, 0,
                                  "value is not callable");
    if (v->calls >= v->limits.depth)
      return mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0,
                                  "call depth limit");
    ++v->calls;
    if (f.object->native)
      {
        if (count > SIZE_MAX / sizeof *public_args)
          {
            mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0, "argument limit");
            goto done;
          }
        bytes = count * sizeof *public_args;
        if (count)
          {
            public_args
                = (mulib_microjs_value_t *)mulib_microjs_alloc_ (v, bytes);
            if (!public_args)
              goto done;
          }
        for (i = 0; i < count; ++i)
          if (!mulib_microjs_export_ (v, args[i], &public_args[i]))
            goto done;
        status = f.object->native (v, public_args, count, &result,
                                   f.object->user);
        if (status != MULIB_MICROJS_OK)
          {
            mulib_microjs_fail_ (v, status, 0, "native callback failed");
            goto done;
          }
        ok = mulib_microjs_import_ (v, &result, out);
      }
    else
      {
        env = mulib_microjs_cell_ (v, MULIB_MICROJS_E_);
        if (!env)
          goto done;
        env->b = f.object->b;
        env->flags = 1;
        i = 0;
        for (param = f.object->c->a; param; param = param->next)
          {
            b = mulib_microjs_binding_ (v, env, param->key, 2, 1);
            if (!b)
              goto done;
            b->value = i < count ? args[i] : mulib_microjs_undefined_ ();
            ++i;
          }
        ok = mulib_microjs_execute_ (v, f.object->c->c, env, this_value, out);
      }
  done:
    if (public_args)
      for (i = 0; i < count; ++i)
        mulib_microjs_value_free (&public_args[i]);
    mulib_microjs_release_ (v, public_args, bytes);
    mulib_microjs_value_free (&result);
    --v->calls;
    return ok;
  }
  static size_t
  mulib_microjs_arity_ (mulib_microjs_object_t *n)
  {
    switch (n->op)
      {
      case MULIB_MICROJS_LITERAL_:
      case MULIB_MICROJS_ID_:
      case MULIB_MICROJS_THIS_:
      case MULIB_MICROJS_FUNCTION_:
      case MULIB_MICROJS_ARRAY_:
      case MULIB_MICROJS_OBJECT_:
      case MULIB_MICROJS_ENTER_:
      case MULIB_MICROJS_LEAVE_:
        return 0;
      case MULIB_MICROJS_LAST_:
        return n->flags ? 0 : 1;
      case MULIB_MICROJS_INVOKE_:
        return n->count + 1;
      case MULIB_MICROJS_MEMBER_:
      case MULIB_MICROJS_PAIR_:
        return 2;
      case MULIB_MICROJS_GET_:
      case MULIB_MICROJS_TRUTH_:
      case MULIB_MICROJS_ISNULL_:
      case MULIB_MICROJS_ARG_:
      case MULIB_MICROJS_INIT_:
      case MULIB_MICROJS_THROW_:
      case MULIB_MICROJS_TYPEOF_:
      case MULIB_MICROJS_VOID_:
      case MULIB_MICROJS_NEG_:
      case MULIB_MICROJS_POS_:
      case MULIB_MICROJS_INC_:
      case MULIB_MICROJS_DEC_:
      case MULIB_MICROJS_POSTINC_:
      case MULIB_MICROJS_POSTDEC_:
      case '!':
      case '~':
        return 1;
      default:
        return 2;
      }
  }
  /* A deliberately enumerated intrinsic surface, rather than silently exposing
   * host C/library functions as if they were ECMAScript built-ins. */
  enum
  {
    MULIB_MICROJS_I_NUMBER_,
    MULIB_MICROJS_I_STRING_,
    MULIB_MICROJS_I_BOOLEAN_,
    MULIB_MICROJS_I_OBJECT_,
    MULIB_MICROJS_I_ARRAY_,
    MULIB_MICROJS_I_ISNAN_,
    MULIB_MICROJS_I_ISFINITE_,
    MULIB_MICROJS_I_NUMBER_ISNAN_,
    MULIB_MICROJS_I_NUMBER_ISFINITE_,
    MULIB_MICROJS_I_OBJECT_IS_,
    MULIB_MICROJS_I_ARRAY_ISARRAY_,
    MULIB_MICROJS_I_ABS_,
    MULIB_MICROJS_I_FLOOR_,
    MULIB_MICROJS_I_CEIL_,
    MULIB_MICROJS_I_TRUNC_,
    MULIB_MICROJS_I_ROUND_,
    MULIB_MICROJS_I_SQRT_,
    MULIB_MICROJS_I_POW_,
    MULIB_MICROJS_I_MIN_,
    MULIB_MICROJS_I_MAX_,
    MULIB_MICROJS_I_SIGN_
  };
  static mulib_microjs_status_t
  mulib_microjs_intrinsic_call_ (mulib_microjs_t *v,
                                 const mulib_microjs_value_t *args,
                                 size_t count, mulib_microjs_value_t *out,
                                 void *user)
  {
    mulib_microjs_object_t *f = (mulib_microjs_object_t *)user, *o, *key;
    mulib_microjs_v_ x = mulib_microjs_undefined_ (), y = x, r = x;
    double a, b, z = 0;
    size_t i;
    char name[32];
    int truth;
    if (count && !mulib_microjs_import_ (v, &args[0], &x))
      return v->error.code;
    if (count > 1 && !mulib_microjs_import_ (v, &args[1], &y))
      return v->error.code;
    a = mulib_microjs_number_ (x);
    b = mulib_microjs_number_ (y);
    switch (f->op)
      {
      case MULIB_MICROJS_I_NUMBER_:
        r = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER, count ? a : 0);
        break;
      case MULIB_MICROJS_I_STRING_:
        o = count ? mulib_microjs_text_ (v, x)
                  : mulib_microjs_string_ (v, NULL, 0);
        if (!o)
          return v->error.code;
        r = mulib_microjs_object_value_ (o);
        break;
      case MULIB_MICROJS_I_BOOLEAN_:
        r = mulib_microjs_scalar_ (MULIB_MICROJS_BOOLEAN,
                                   mulib_microjs_truth_ (x));
        break;
      case MULIB_MICROJS_I_OBJECT_:
      case MULIB_MICROJS_I_ARRAY_:
        if (f->op == MULIB_MICROJS_I_OBJECT_ && x.object
            && x.type != MULIB_MICROJS_STRING)
          {
            r = x;
            break;
          }
        if (f->op == MULIB_MICROJS_I_OBJECT_ && x.type != MULIB_MICROJS_NULL
            && x.type != MULIB_MICROJS_UNDEFINED)
          return MULIB_MICROJS_TYPE; /* Boxed primitives are not implemented
                                        yet. */
        o = mulib_microjs_cell_ (v, MULIB_MICROJS_O_);
        if (!o)
          return v->error.code;
        o->flags = f->op == MULIB_MICROJS_I_ARRAY_;
        if (o->flags && count == 1 && x.type == MULIB_MICROJS_NUMBER)
          {
            if (!isfinite (a) || a < 0 || a > UINT32_MAX || floor (a) != a)
              return MULIB_MICROJS_ARITHMETIC;
            o->length = (size_t)a;
          }
        else if (o->flags)
          for (i = 0; i < count; ++i)
            {
              (void)snprintf (name, sizeof name, "%zu", i);
              key = mulib_microjs_utf8_ (v, name, strlen (name));
              if (!key || !mulib_microjs_import_ (v, &args[i], &x)
                  || !mulib_microjs_put_property_ (v, o, key, x))
                return v->error.code;
            }
        r = mulib_microjs_object_value_ (o);
        break;
      case MULIB_MICROJS_I_ISNAN_:
      case MULIB_MICROJS_I_ISFINITE_:
      case MULIB_MICROJS_I_NUMBER_ISNAN_:
      case MULIB_MICROJS_I_NUMBER_ISFINITE_:
        truth = f->op == MULIB_MICROJS_I_ISNAN_
                        || f->op == MULIB_MICROJS_I_NUMBER_ISNAN_
                    ? isnan (a)
                    : isfinite (a);
        if (f->op == MULIB_MICROJS_I_NUMBER_ISNAN_
            || f->op == MULIB_MICROJS_I_NUMBER_ISFINITE_)
          truth = x.type == MULIB_MICROJS_NUMBER && truth;
        r = mulib_microjs_scalar_ (MULIB_MICROJS_BOOLEAN, truth);
        break;
      case MULIB_MICROJS_I_OBJECT_IS_:
        truth = mulib_microjs_strict_equal_ (x, y);
        if (x.type == MULIB_MICROJS_NUMBER && y.type == MULIB_MICROJS_NUMBER)
          truth = (isnan (a) && isnan (b))
                  || (a == b && (a != 0 || !!signbit (a) == !!signbit (b)));
        r = mulib_microjs_scalar_ (MULIB_MICROJS_BOOLEAN, truth);
        break;
      case MULIB_MICROJS_I_ARRAY_ISARRAY_:
        r = mulib_microjs_scalar_ (MULIB_MICROJS_BOOLEAN,
                                   x.type == MULIB_MICROJS_OBJECT
                                       && x.object->flags);
        break;
      default:
        switch (f->op)
          {
          case MULIB_MICROJS_I_ABS_:
            z = fabs (a);
            break;
          case MULIB_MICROJS_I_FLOOR_:
            z = floor (a);
            break;
          case MULIB_MICROJS_I_CEIL_:
            z = ceil (a);
            break;
          case MULIB_MICROJS_I_TRUNC_:
            z = trunc (a);
            break;
          case MULIB_MICROJS_I_ROUND_:
            z = a == 0                ? a
                : a >= -0.5 && a < 0  ? -0.0
                : a - floor (a) < 0.5 ? floor (a)
                                      : ceil (a);
            break;
          case MULIB_MICROJS_I_SQRT_:
            z = sqrt (a);
            break;
          case MULIB_MICROJS_I_POW_:
            if (!mulib_microjs_binary_ (v, MULIB_MICROJS_POW_, x, y, &r))
              return v->error.code;
            z = r.number;
            break;
          case MULIB_MICROJS_I_SIGN_:
            z = isnan (a) || a == 0 ? a : a < 0 ? -1 : 1;
            break;
          case MULIB_MICROJS_I_MIN_:
          case MULIB_MICROJS_I_MAX_:
            z = f->op == MULIB_MICROJS_I_MIN_ ? INFINITY : -INFINITY;
            for (i = 0; i < count; ++i)
              {
                if (!mulib_microjs_import_ (v, &args[i], &x))
                  return v->error.code;
                a = mulib_microjs_number_ (x);
                if (isnan (a) || isnan (z))
                  z = NAN;
                else if (f->op == MULIB_MICROJS_I_MIN_)
                  {
                    if (a < z || (a == 0 && z == 0 && signbit (a)))
                      z = a;
                  }
                else if (a > z || (a == 0 && z == 0 && !signbit (a)))
                  z = a;
              }
            break;
          default:
            return MULIB_MICROJS_TYPE;
          }
        r = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER, z);
        break;
      }
    (void)mulib_microjs_export_ (v, r, out);
    return v->error.code;
  }
  static mulib_microjs_object_t *
  mulib_microjs_intrinsic_function_ (mulib_microjs_t *v, int op)
  {
    mulib_microjs_object_t *f = mulib_microjs_cell_ (v, MULIB_MICROJS_F_);
    if (f)
      {
        f->native = mulib_microjs_intrinsic_call_;
        f->user = f;
        f->op = op;
      }
    return f;
  }
  static int
  mulib_microjs_intrinsic_property_ (mulib_microjs_t *v,
                                     mulib_microjs_object_t *o,
                                     const char *name, int op, double number)
  {
    mulib_microjs_object_t *key = mulib_microjs_utf8_ (v, name, strlen (name)),
                           *b, *f = NULL;
    if (!key)
      return 0;
    if (op >= 0)
      {
        f = mulib_microjs_intrinsic_function_ (v, op);
        if (!f)
          return 0;
      }
    b = mulib_microjs_binding_ (v, o, key, 4, 1);
    if (!b)
      return 0;
    b->value = f ? mulib_microjs_object_value_ (f)
                 : mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER, number);
    return 1;
  }
  static mulib_microjs_object_t *
  mulib_microjs_intrinsic_ (mulib_microjs_t *v, mulib_microjs_object_t *key)
  {
    static const char *const names[]
        = { "Number", "String", "Boolean",  "Object",
            "Array",  "isNaN",  "isFinite", "Math" };
    static const char *const math_names[]
        = { "abs",  "floor", "ceil", "trunc", "round",
            "sqrt", "pow",   "min",  "max",   "sign" };
    size_t i, j;
    mulib_microjs_object_t *o, *b;
    if (v->intrinsics
        && (b = mulib_microjs_find_ (v->intrinsics, key, 0)) != NULL)
      return b;
    for (i = 0; i < sizeof names / sizeof *names; ++i)
      if (mulib_microjs_ascii_ (key, names[i]))
        break;
    if (i == sizeof names / sizeof *names)
      return NULL;
    if (!v->intrinsics)
      {
        v->intrinsics = mulib_microjs_cell_ (v, MULIB_MICROJS_E_);
        if (!v->intrinsics)
          return NULL;
      }
    o = i == 7 ? mulib_microjs_cell_ (v, MULIB_MICROJS_O_)
               : mulib_microjs_intrinsic_function_ (v, (int)i);
    if (!o)
      return NULL;
    if (i == 0)
      {
        if (!mulib_microjs_intrinsic_property_ (
                v, o, "isNaN", MULIB_MICROJS_I_NUMBER_ISNAN_, 0)
            || !mulib_microjs_intrinsic_property_ (
                v, o, "isFinite", MULIB_MICROJS_I_NUMBER_ISFINITE_, 0))
          return NULL;
      }
    else if (i == 3)
      {
        if (!mulib_microjs_intrinsic_property_ (v, o, "is",
                                                MULIB_MICROJS_I_OBJECT_IS_, 0))
          return NULL;
      }
    else if (i == 4)
      {
        if (!mulib_microjs_intrinsic_property_ (
                v, o, "isArray", MULIB_MICROJS_I_ARRAY_ISARRAY_, 0))
          return NULL;
      }
    else if (i == 7)
      {
        for (j = 0; j < sizeof math_names / sizeof *math_names; ++j)
          if (!mulib_microjs_intrinsic_property_ (
                  v, o, math_names[j], MULIB_MICROJS_I_ABS_ + (int)j, 0))
            return NULL;
        if (!mulib_microjs_intrinsic_property_ (v, o, "PI", -1,
                                                3.14159265358979323846)
            || !mulib_microjs_intrinsic_property_ (v, o, "E", -1,
                                                   2.71828182845904523536))
          return NULL;
      }
    b = mulib_microjs_binding_ (v, v->intrinsics, key, 4, 1);
    if (b)
      b->value = mulib_microjs_object_value_ (o);
    return b;
  }
  static int
  mulib_microjs_dispatch_ (void *user, const int64_t *handles, size_t count,
                           int64_t *out)
  {
    mulib_microjs_host_ *h = (mulib_microjs_host_ *)user;
    mulib_microjs_run_ *r = h->run;
    mulib_microjs_t *v = r->vm;
    mulib_microjs_object_t *n = h->node, *o, *b, *key;
    mulib_microjs_ref_ a, b_ref, result;
    mulib_microjs_v_ x, y, *args = NULL;
    size_t i, bytes;
    int op = n->op, ok = 0;
    const char *type;
    char index[32];
    memset (&a, 0, sizeof a);
    memset (&b_ref, 0, sizeof b_ref);
    memset (&result, 0, sizeof result);
    result.value = x = y = mulib_microjs_undefined_ ();
    if (v->steps++ >= v->limits.steps)
      {
        mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, n->offset,
                             "execution step limit");
        goto done;
      }
    for (i = 0; i < count; ++i)
      if (handles[i] < 0 || (uint64_t)handles[i] >= r->count)
        {
          mulib_microjs_fail_ (v, MULIB_MICROJS_INVALID, n->offset,
                               "invalid bytecode value handle");
          goto done;
        }
    if (count)
      a = r->values[(size_t)handles[0]];
    if (count > 1)
      b_ref = r->values[(size_t)handles[1]];
    switch (op)
      {
      case MULIB_MICROJS_LITERAL_:
        result.value = n->value;
        break;
      case MULIB_MICROJS_THIS_:
        result.value = r->this_value;
        break;
      case MULIB_MICROJS_ID_:
        result.reference = 1;
        result.key = n->key;
        result.base = mulib_microjs_find_ (r->environment, n->key, 1);
        if (!result.base)
          result.base = mulib_microjs_intrinsic_ (v, n->key);
        if (v->error.code)
          goto done;
        if (!result.base)
          {
            if (mulib_microjs_ascii_ (n->key, "undefined"))
              {
                result.reference = 0;
                result.value = mulib_microjs_undefined_ ();
              }
            else if (mulib_microjs_ascii_ (n->key, "NaN"))
              {
                result.reference = 0;
                result.value
                    = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER, NAN);
              }
            else if (mulib_microjs_ascii_ (n->key, "Infinity"))
              {
                result.reference = 0;
                result.value
                    = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER, INFINITY);
              }
          }
        break;
      case MULIB_MICROJS_GET_:
        if (!mulib_microjs_get_ (v, a, &result.value))
          goto done;
        break;
      case MULIB_MICROJS_ARG_:
        result = a;
        if (!mulib_microjs_get_ (v, a, &result.value))
          goto done;
        if (result.reference == 1 || result.reference == 2)
          result.reference += 2;
        break;
      case MULIB_MICROJS_TRUTH_:
      case MULIB_MICROJS_ISNULL_:
        if (!mulib_microjs_get_ (v, a, &x))
          goto done;
        *out = op == MULIB_MICROJS_TRUTH_
                   ? mulib_microjs_truth_ (x)
                   : x.type == MULIB_MICROJS_NULL
                         || x.type == MULIB_MICROJS_UNDEFINED;
        ok = 1;
        goto done;
      case MULIB_MICROJS_LAST_:
        if (count)
          {
            if (!mulib_microjs_get_ (v, a, &r->last))
              goto done;
          }
        result.value = r->last;
        break;
      case MULIB_MICROJS_ENTER_:
        if (!mulib_microjs_enter_ (r, n))
          goto done;
        break;
      case MULIB_MICROJS_LEAVE_:
        r->environment = r->environment->b;
        break;
      case MULIB_MICROJS_INIT_:
        if (!mulib_microjs_get_ (v, a, &x))
          goto done;
        o = r->environment;
        if (n->flags == 2)
          while (o && !o->flags)
            o = o->b;
        b = mulib_microjs_find_ (o, n->key, 0);
        if (!b)
          {
            mulib_microjs_fail_ (v, MULIB_MICROJS_NAME, n->offset,
                                 "missing declaration binding");
            goto done;
          }
        b->initialized = 1;
        b->value = x;
        break;
      case MULIB_MICROJS_FUNCTION_:
        o = mulib_microjs_closure_ (v, n, r->environment, 1);
        if (!o)
          goto done;
        result.value = mulib_microjs_object_value_ (o);
        break;
      case MULIB_MICROJS_ARRAY_:
      case MULIB_MICROJS_OBJECT_:
        o = mulib_microjs_cell_ (v, MULIB_MICROJS_O_);
        if (!o)
          goto done;
        o->flags = op == MULIB_MICROJS_ARRAY_;
        result.value = mulib_microjs_object_value_ (o);
        break;
      case MULIB_MICROJS_PAIR_:
        if (!mulib_microjs_get_ (v, a, &x)
            || !mulib_microjs_get_ (v, b_ref, &y))
          goto done;
        o = x.object;
        key = n->key;
        if (!key)
          {
            (void)snprintf (index, sizeof index, "%zu", o->length);
            key = mulib_microjs_utf8_ (v, index, strlen (index));
            if (!key)
              goto done;
            if (n->flags)
              {
                ++o->length;
                break;
              }
          }
        if (!mulib_microjs_put_property_ (v, o, key, y))
          goto done;
        break;
      case MULIB_MICROJS_MEMBER_:
        if (!mulib_microjs_get_ (v, a, &x)
            || !mulib_microjs_get_ (v, b_ref, &y))
          goto done;
        if (x.type == MULIB_MICROJS_NULL || x.type == MULIB_MICROJS_UNDEFINED)
          {
            mulib_microjs_fail_ (v, MULIB_MICROJS_TYPE, n->offset,
                                 "property of null or undefined");
            goto done;
          }
        result.reference = 2;
        result.base = x.object;
        result.value = x;
        result.key = mulib_microjs_text_ (v, y);
        if (!result.key)
          goto done;
        break;
      case MULIB_MICROJS_INVOKE_:
        if (!mulib_microjs_get_ (v, a, &x))
          goto done;
        if (count - 1 > SIZE_MAX / sizeof *args)
          {
            mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, n->offset,
                                 "argument storage limit");
            goto done;
          }
        bytes = (count - 1) * sizeof *args;
        if (bytes)
          {
            args = (mulib_microjs_v_ *)mulib_microjs_alloc_ (v, bytes);
            if (!args)
              goto done;
          }
        for (i = 1; i < count; ++i)
          if (!mulib_microjs_get_ (v, r->values[(size_t)handles[i]],
                                   &args[i - 1]))
            {
              mulib_microjs_release_ (v, args, bytes);
              goto done;
            }
        y = a.reference == 4 && a.base ? mulib_microjs_object_value_ (a.base)
                                       : mulib_microjs_undefined_ ();
        ok = mulib_microjs_invoke_ (v, x, y, args, count - 1, &result.value);
        mulib_microjs_release_ (v, args, bytes);
        if (!ok)
          goto done;
        ok = 0;
        break;
      case '=':
      case MULIB_MICROJS_ADDSET_:
      case MULIB_MICROJS_SUBSET_:
      case MULIB_MICROJS_MULSET_:
      case MULIB_MICROJS_DIVSET_:
      case MULIB_MICROJS_MODSET_:
        if (!mulib_microjs_get_ (v, b_ref, &y))
          goto done;
        if (op != '=')
          {
            if (!mulib_microjs_get_ (v, a, &x))
              goto done;
            op = op == MULIB_MICROJS_ADDSET_   ? '+'
                 : op == MULIB_MICROJS_SUBSET_ ? '-'
                 : op == MULIB_MICROJS_MULSET_ ? '*'
                 : op == MULIB_MICROJS_DIVSET_ ? '/'
                                               : '%';
            if (!mulib_microjs_binary_ (v, op, x, y, &y))
              goto done;
          }
        if (!mulib_microjs_put_ (r, a, y))
          goto done;
        result.value = y;
        break;
      case MULIB_MICROJS_TYPEOF_:
        if (a.reference == 1 && !a.base)
          x = mulib_microjs_undefined_ ();
        else if (!mulib_microjs_get_ (v, a, &x))
          goto done;
        type = x.type == MULIB_MICROJS_UNDEFINED  ? "undefined"
               : x.type == MULIB_MICROJS_NUMBER   ? "number"
               : x.type == MULIB_MICROJS_BOOLEAN  ? "boolean"
               : x.type == MULIB_MICROJS_STRING   ? "string"
               : x.type == MULIB_MICROJS_FUNCTION ? "function"
                                                  : "object";
        o = mulib_microjs_utf8_ (v, type, strlen (type));
        if (!o)
          goto done;
        result.value = mulib_microjs_object_value_ (o);
        break;
      case MULIB_MICROJS_VOID_:
        if (!mulib_microjs_get_ (v, a, &x))
          goto done;
        break;
      case MULIB_MICROJS_INC_:
      case MULIB_MICROJS_DEC_:
      case MULIB_MICROJS_POSTINC_:
      case MULIB_MICROJS_POSTDEC_:
        if (!mulib_microjs_get_ (v, a, &x))
          goto done;
        x = mulib_microjs_scalar_ (MULIB_MICROJS_NUMBER,
                                   mulib_microjs_number_ (x));
        y = mulib_microjs_scalar_ (
            MULIB_MICROJS_NUMBER,
            x.number
                + (op == MULIB_MICROJS_INC_ || op == MULIB_MICROJS_POSTINC_
                       ? 1
                       : -1));
        if (!mulib_microjs_put_ (r, a, y))
          goto done;
        result.value
            = op == MULIB_MICROJS_POSTINC_ || op == MULIB_MICROJS_POSTDEC_ ? x
                                                                           : y;
        break;
      case '!':
      case '~':
      case MULIB_MICROJS_NEG_:
      case MULIB_MICROJS_POS_:
        if (!mulib_microjs_get_ (v, a, &x))
          goto done;
        if (op == '!')
          result.value = mulib_microjs_scalar_ (MULIB_MICROJS_BOOLEAN,
                                                !mulib_microjs_truth_ (x));
        else
          result.value = mulib_microjs_scalar_ (
              MULIB_MICROJS_NUMBER,
              op == '~' ? mulib_microjs_int32_ (~mulib_microjs_uint32_ (
                              mulib_microjs_number_ (x)))
              : op == MULIB_MICROJS_NEG_ ? -mulib_microjs_number_ (x)
                                         : mulib_microjs_number_ (x));
        break;
      case MULIB_MICROJS_THROW_:
        mulib_microjs_fail_ (v, MULIB_MICROJS_EXCEPTION, n->offset,
                             "uncaught JavaScript exception");
        goto done;
      default:
        if (!mulib_microjs_get_ (v, a, &x)
            || !mulib_microjs_get_ (v, b_ref, &y)
            || !mulib_microjs_binary_ (v, op, x, y, &result.value))
          goto done;
        break;
      }
    ok = mulib_microjs_slot_ (r, result, out);
  done:
    if (!ok && v->error.code && !v->error.offset)
      v->error.offset = n->offset;
    return ok ? 0 : 1;
  }
  static int
  mulib_microjs_execute_ (mulib_microjs_t *v, mulib_microjs_object_t *program,
                          mulib_microjs_object_t *environment,
                          mulib_microjs_v_ this_value, mulib_microjs_v_ *out)
  {
    mulib_microjs_run_ r;
    mulib_minivm_program_t p;
    mulib_minivm_limits_t limits;
    mulib_minivm_result_t result;
    mulib_minivm_host_t *hosts = NULL;
    mulib_microjs_host_ *contexts = NULL;
    size_t i, hb, cb, stack_bytes;
    int ok = 0;
    memset (&r, 0, sizeof r);
    memset (&p, 0, sizeof p);
    r.vm = v;
    r.program = program;
    r.environment = environment;
    r.this_value = this_value;
    r.last = mulib_microjs_undefined_ ();
    if (!program || program->count > SIZE_MAX / sizeof *hosts
        || program->count > SIZE_MAX / sizeof *contexts
        || program->count > SIZE_MAX / sizeof (int64_t))
      return mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0,
                                  "execution image limit");
    hb = program->count * sizeof *hosts;
    cb = program->count * sizeof *contexts;
    stack_bytes = program->count * sizeof (int64_t);
    hosts = (mulib_minivm_host_t *)mulib_microjs_alloc_ (v, hb);
    if (!hosts)
      goto done;
    contexts = (mulib_microjs_host_ *)mulib_microjs_alloc_ (v, cb);
    if (!contexts)
      goto done;
    if (stack_bytes > v->heap_limit - v->heap_bytes)
      {
        mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0,
                             "backend stack budget");
        goto done;
      }
    for (i = 0; i < program->count; ++i)
      if (program->operations[i])
        {
          contexts[i].run = &r;
          contexts[i].node = program->operations[i];
          hosts[i].function = mulib_microjs_dispatch_;
          hosts[i].user = &contexts[i];
          hosts[i].argument_count = mulib_microjs_arity_ (contexts[i].node);
        }
    p.code = program->code;
    p.code_count = program->count;
    p.hosts = hosts;
    p.host_count = program->count;
    limits.stack = program->count;
    limits.locals = 0;
    limits.steps = v->limits.steps;
    v->heap_bytes += stack_bytes;
    (void)mulib_minivm_run (&p, &limits, &result);
    v->heap_bytes -= stack_bytes;
    if (result.status != MULIB_MINIVM_OK)
      {
        if (!v->error.code)
          mulib_microjs_fail_ (v,
                               result.status == MULIB_MINIVM_NO_MEMORY
                                   ? MULIB_MICROJS_MEMORY
                               : result.status == MULIB_MINIVM_STEP_LIMIT
                                   ? MULIB_MICROJS_LIMIT
                                   : MULIB_MICROJS_INVALID,
                               0, mulib_minivm_status_string (result.status));
        goto done;
      }
    if (!result.stack_size || result.value < 0
        || (uint64_t)result.value >= r.count)
      {
        mulib_microjs_fail_ (v, MULIB_MICROJS_INVALID, 0,
                             "invalid backend result");
        goto done;
      }
    ok = mulib_microjs_get_ (v, r.values[(size_t)result.value], out);
  done:
    mulib_microjs_release_ (v, r.values, r.capacity * sizeof *r.values);
    mulib_microjs_release_ (v, hosts, hb);
    mulib_microjs_release_ (v, contexts, cb);
    return ok;
  }

  static int
  mulib_microjs_empty_output_ (const mulib_microjs_value_t *out)
  {
    return out && out->type == MULIB_MICROJS_NULL && !out->string
           && !out->object;
  }
  static void
  mulib_microjs_diagnostic_ (mulib_microjs_t *v, const char *s, size_t n,
                             mulib_microjs_error_t *out)
  {
    size_t i = 0;
    uint32_t cp;
    if (v->error.code)
      {
        if (v->error.offset > n)
          v->error.offset = n;
        v->error.line = v->error.column = 1;
        while (i < v->error.offset)
          {
            if (!mulib_microjs_decode_ (s, v->error.offset, &i, &cp))
              {
                ++v->error.column;
                continue;
              }
            if (cp == 13 || cp == 10 || cp == 0x2028 || cp == 0x2029)
              {
                if (cp == 13 && i < v->error.offset && s[i] == '\n')
                  ++i;
                ++v->error.line;
                v->error.column = 1;
              }
            else
              ++v->error.column;
          }
      }
    if (out)
      *out = v->error;
  }
  static mulib_microjs_status_t
  mulib_microjs_invalid_ (mulib_microjs_error_t *error)
  {
    if (error)
      {
        memset (error, 0, sizeof *error);
        error->code = MULIB_MICROJS_INVALID;
        error->line = error->column = 1;
        error->message = "invalid arguments or interpreter reentry";
      }
    return MULIB_MICROJS_INVALID;
  }
  mulib_microjs_status_t
  mulib_microjs_eval (mulib_microjs_t *v, const char *s, size_t n,
                      mulib_microjs_value_t *out, mulib_microjs_error_t *error)
  {
    mulib_microjs_parser_ p;
    mulib_microjs_object_t *block, *program;
    mulib_microjs_v_ result;
    if (!v || v->destroyed || v->running || (!s && n)
        || !mulib_microjs_empty_output_ (out))
      return mulib_microjs_invalid_ (error);
    memset (&v->error, 0, sizeof v->error);
    memset (&p, 0, sizeof p);
    (void)mulib_microjs_collect (v);
    v->running = 1;
    v->steps = v->calls = 0;
    p.vm = v;
    p.source = s;
    p.length = n;
    if (n > v->limits.source_bytes)
      {
        mulib_microjs_fail_ (v, MULIB_MICROJS_LIMIT, 0, "source byte limit");
        goto done;
      }
    if (!mulib_microjs_next_ (&p))
      goto done;
    block = mulib_microjs_block_ (&p, 0);
    if (!block)
      goto done;
    if (!mulib_microjs_validate_ (&p, block, 0)
        || !mulib_microjs_global_check_ (&p, block))
      goto done;
    program = mulib_microjs_compile_program_ (&p, block, 0);
    if (!program)
      goto done;
    if (mulib_microjs_execute_ (v, program, v->global,
                                mulib_microjs_undefined_ (), &result))
      (void)mulib_microjs_export_ (v, result, out);
  done:
    v->running = 0;
    mulib_microjs_diagnostic_ (v, s, n, error);
    (void)mulib_microjs_collect (v);
    return v->error.code;
  }
  static int
  mulib_microjs_api_ (mulib_microjs_t *v)
  {
    if (!v || v->destroyed)
      return 0;
    if (!v->running)
      {
        memset (&v->error, 0, sizeof v->error);
        (void)mulib_microjs_collect (v);
      }
    return !v->error.code;
  }
  mulib_microjs_status_t
  mulib_microjs_value_copy (mulib_microjs_t *v,
                            const mulib_microjs_value_t *value,
                            mulib_microjs_value_t *out)
  {
    mulib_microjs_v_ x;
    if (!value || !mulib_microjs_empty_output_ (out)
        || !mulib_microjs_api_ (v))
      return MULIB_MICROJS_INVALID;
    if (mulib_microjs_import_ (v, value, &x))
      (void)mulib_microjs_export_ (v, x, out);
    return v->error.code;
  }
  mulib_microjs_status_t
  mulib_microjs_make_object (mulib_microjs_t *v, int array,
                             mulib_microjs_value_t *out)
  {
    mulib_microjs_object_t *o;
    if (!mulib_microjs_empty_output_ (out) || !mulib_microjs_api_ (v))
      return MULIB_MICROJS_INVALID;
    o = mulib_microjs_cell_ (v, MULIB_MICROJS_O_);
    if (o)
      {
        o->flags = !!array;
        (void)mulib_microjs_export_ (v, mulib_microjs_object_value_ (o), out);
      }
    return v->error.code;
  }
  mulib_microjs_status_t
  mulib_microjs_get_global (mulib_microjs_t *v, const char *name,
                            mulib_microjs_value_t *out)
  {
    mulib_microjs_object_t *key, *b;
    if (!name || !mulib_microjs_empty_output_ (out) || !mulib_microjs_api_ (v))
      return MULIB_MICROJS_INVALID;
    key = mulib_microjs_utf8_ (v, name, strlen (name));
    if (!key)
      return v->error.code;
    b = mulib_microjs_find_ (v->global, key, 1);
    if (!b)
      b = mulib_microjs_intrinsic_ (v, key);
    if (!b
        && (mulib_microjs_ascii_ (key, "undefined")
            || mulib_microjs_ascii_ (key, "NaN")
            || mulib_microjs_ascii_ (key, "Infinity")))
      {
        (void)mulib_microjs_export_ (
            v,
            mulib_microjs_ascii_ (key, "undefined")
                ? mulib_microjs_undefined_ ()
                : mulib_microjs_scalar_ (
                      MULIB_MICROJS_NUMBER,
                      mulib_microjs_ascii_ (key, "NaN") ? NAN : INFINITY),
            out);
        return v->error.code;
      }
    if (!b || !b->initialized)
      mulib_microjs_fail_ (v, MULIB_MICROJS_NAME, 0,
                           "unknown or uninitialized global");
    else
      (void)mulib_microjs_export_ (v, b->value, out);
    return v->error.code;
  }
  mulib_microjs_status_t
  mulib_microjs_set_global (mulib_microjs_t *v, const char *name,
                            const mulib_microjs_value_t *value)
  {
    mulib_microjs_object_t *key, *b;
    mulib_microjs_v_ x;
    if (!name || !*name || !value || !mulib_microjs_api_ (v))
      return MULIB_MICROJS_INVALID;
    key = mulib_microjs_utf8_ (v, name, strlen (name));
    if (!key || !mulib_microjs_import_ (v, value, &x))
      return v->error.code;
    b = mulib_microjs_find_ (v->global, key, 0);
    if (b && ((b->flags & 1) || !b->initialized))
      return MULIB_MICROJS_TYPE;
    if (!b)
      b = mulib_microjs_binding_ (v, v->global, key, 2, 1);
    if (b)
      b->value = x;
    return v->error.code;
  }
  mulib_microjs_status_t
  mulib_microjs_bind (mulib_microjs_t *v, const char *name,
                      mulib_microjs_native_fn fn, void *user)
  {
    mulib_microjs_object_t *key, *b, *f;
    if (!name || !*name || !fn || !mulib_microjs_api_ (v))
      return MULIB_MICROJS_INVALID;
    key = mulib_microjs_utf8_ (v, name, strlen (name));
    if (!key)
      return v->error.code;
    b = mulib_microjs_find_ (v->global, key, 0);
    if (b && ((b->flags & 1) || !b->initialized))
      return MULIB_MICROJS_TYPE;
    f = mulib_microjs_cell_ (v, MULIB_MICROJS_F_);
    if (!f)
      return v->error.code;
    f->native = fn;
    f->user = user;
    if (!b)
      b = mulib_microjs_binding_ (v, v->global, key, 2, 1);
    if (b)
      b->value = mulib_microjs_object_value_ (f);
    return v->error.code;
  }
  mulib_microjs_status_t
  mulib_microjs_call (mulib_microjs_t *v,
                      const mulib_microjs_value_t *function,
                      const mulib_microjs_value_t *arguments, size_t count,
                      mulib_microjs_value_t *out, mulib_microjs_error_t *error)
  {
    mulib_microjs_v_ f, result, *args = NULL;
    size_t i, bytes;
    if (!v || v->destroyed || v->running || !function || (!arguments && count)
        || !mulib_microjs_empty_output_ (out)
        || count > SIZE_MAX / sizeof *args)
      return mulib_microjs_invalid_ (error);
    (void)mulib_microjs_api_ (v);
    v->running = 1;
    v->steps = v->calls = 0;
    bytes = count * sizeof *args;
    if (!mulib_microjs_import_ (v, function, &f))
      goto done;
    if (count)
      {
        args = (mulib_microjs_v_ *)mulib_microjs_alloc_ (v, bytes);
        if (!args)
          goto done;
      }
    for (i = 0; i < count; ++i)
      if (!mulib_microjs_import_ (v, &arguments[i], &args[i]))
        goto done;
    if (mulib_microjs_invoke_ (v, f, mulib_microjs_undefined_ (), args, count,
                               &result))
      (void)mulib_microjs_export_ (v, result, out);
  done:
    mulib_microjs_release_ (v, args, bytes);
    v->running = 0;
    mulib_microjs_diagnostic_ (v, NULL, 0, error);
    (void)mulib_microjs_collect (v);
    return v->error.code;
  }
  mulib_microjs_status_t
  mulib_microjs_get_property (mulib_microjs_t *v,
                              const mulib_microjs_value_t *object,
                              const char *name, mulib_microjs_value_t *out)
  {
    mulib_microjs_v_ x, result;
    mulib_microjs_object_t *key;
    if (!object || !name || !mulib_microjs_empty_output_ (out)
        || !mulib_microjs_api_ (v))
      return MULIB_MICROJS_INVALID;
    if (!mulib_microjs_import_ (v, object, &x))
      return v->error.code;
    key = mulib_microjs_utf8_ (v, name, strlen (name));
    if (!key)
      return v->error.code;
    if (mulib_microjs_property_ (v, x, key, &result))
      (void)mulib_microjs_export_ (v, result, out);
    return v->error.code;
  }
  mulib_microjs_status_t
  mulib_microjs_set_property (mulib_microjs_t *v,
                              const mulib_microjs_value_t *object,
                              const char *name,
                              const mulib_microjs_value_t *value)
  {
    mulib_microjs_v_ x, y;
    mulib_microjs_object_t *key;
    if (!object || !value || !name || !mulib_microjs_api_ (v))
      return MULIB_MICROJS_INVALID;
    if (!mulib_microjs_import_ (v, object, &x)
        || !mulib_microjs_import_ (v, value, &y))
      return v->error.code;
    key = mulib_microjs_utf8_ (v, name, strlen (name));
    if (!key)
      return v->error.code;
    (void)mulib_microjs_put_property_ (v, x.object, key, y);
    return v->error.code;
  }

#ifdef __cplusplus
}
#endif
#endif
