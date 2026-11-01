/**
 * @file mulib-shaderx.h
 * @brief Typed shader expression graphs and a Parsec-backed GLSL/HLSL
 * compiler. Define MULIB_IMPL_SHADERX, MULIB_IMPL_PARSEC and
 * MULIB_IMPL_STRBUF. See the manual for the bounded, expression-oriented
 * ShaderX grammar.
 */
#ifndef MULIB_SHADERX_H
#define MULIB_SHADERX_H
#include "mulib-parsec.h"
#include "mulib-strbuf.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_shaderx mulib_shaderx_t;
  typedef uint32_t mulib_shaderx_expr_t;
  typedef enum mulib_shaderx_type
  {
    MULIB_SHADERX_FLOAT = 1,
    MULIB_SHADERX_VEC2,
    MULIB_SHADERX_VEC3,
    MULIB_SHADERX_VEC4
  } mulib_shaderx_type_t;
  typedef enum mulib_shaderx_stage
  {
    MULIB_SHADERX_VERTEX,
    MULIB_SHADERX_FRAGMENT
  } mulib_shaderx_stage_t;
  typedef enum mulib_shaderx_target
  {
    MULIB_SHADERX_GLSL330,
    MULIB_SHADERX_HLSL5
  } mulib_shaderx_target_t;
  typedef enum mulib_shaderx_op
  {
    MULIB_SHADERX_ADD,
    MULIB_SHADERX_SUB,
    MULIB_SHADERX_MUL,
    MULIB_SHADERX_DIV,
    MULIB_SHADERX_NEG,
    MULIB_SHADERX_DOT,
    MULIB_SHADERX_NORMALIZE,
    MULIB_SHADERX_SIN,
    MULIB_SHADERX_COS,
    MULIB_SHADERX_MIX,
    MULIB_SHADERX_MIN,
    MULIB_SHADERX_MAX
  } mulib_shaderx_op_t;
  typedef enum mulib_shaderx_status
  {
    MULIB_SHADERX_OK,
    MULIB_SHADERX_INVALID,
    MULIB_SHADERX_SYNTAX,
    MULIB_SHADERX_TYPE,
    MULIB_SHADERX_LIMIT,
    MULIB_SHADERX_MEMORY
  } mulib_shaderx_status_t;
  typedef struct mulib_shaderx_error
  {
    mulib_shaderx_status_t code;
    size_t offset;
    const char *message;
  } mulib_shaderx_error_t;
  /** @brief Allocate an owned graph, limited to 1024 nodes and 64
   * input/uniform bindings. */
  mulib_shaderx_t *mulib_shaderx_create (mulib_shaderx_stage_t stage);
  void mulib_shaderx_destroy (mulib_shaderx_t *program);
  /** @brief Add a finite binary32 constant; expression zero signals failure,
   * without mutation. */
  mulib_shaderx_expr_t mulib_shaderx_constant (mulib_shaderx_t *program,
                                               float value);
  /** @brief Add a unique ASCII identifier (1..63 bytes); uniform is exactly 0
   * or 1. Expression IDs belong to their creating graph; never mix graphs.
   */
  mulib_shaderx_expr_t mulib_shaderx_input (mulib_shaderx_t *program,
                                            mulib_shaderx_type_t type,
                                            const char *name, int uniform);
  /** @brief Find a copied input/uniform name; zero if absent. Emission names
   * it sx_in<ID>. */
  mulib_shaderx_expr_t
  mulib_shaderx_find_input (const mulib_shaderx_t *program, const char *name);
  /** @brief Construct vec2/3/4 by flattening 1..4 arguments, or splatting a
   * single scalar. */
  mulib_shaderx_expr_t
  mulib_shaderx_vector (mulib_shaderx_t *program, mulib_shaderx_type_t type,
                        const mulib_shaderx_expr_t *arguments, size_t count);
  /** @brief Checked typed operation; scalar arithmetic broadcasts across
   * vectors. */
  mulib_shaderx_expr_t
  mulib_shaderx_apply (mulib_shaderx_t *program, mulib_shaderx_op_t operation,
                       const mulib_shaderx_expr_t *arguments, size_t count);
  /** @brief Set the stage's vec4 result (clip position or RGBA); 0 leaves it
   * unchanged. */
  int mulib_shaderx_output (mulib_shaderx_t *program,
                            mulib_shaderx_expr_t expression);
  /** @brief Parse an owned graph; NULL on error with static diagnostic and
   * byte offset. Source is borrowed during the call. Maximum 64 KiB, 16384
   * tokens, depth 64.
   */
  mulib_shaderx_t *mulib_shaderx_parse (const char *source, size_t length,
                                        mulib_shaderx_error_t *error);
  /** @brief Append a complete shader to an initialized string builder
   * transactionally. Output uses generated identifiers, preserving user names
   * only in the graph.
   */
  int mulib_shaderx_emit (const mulib_shaderx_t *program,
                          mulib_shaderx_target_t target, mulib_strbuf_t *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SHADERX) && !defined(MULIB_SHADERX_IMPLEMENTED)
#define MULIB_SHADERX_IMPLEMENTED
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_shaderx_node_
  {
    int kind, uniform;
    mulib_shaderx_type_t type;
    mulib_shaderx_op_t op;
    mulib_shaderx_expr_t args[4];
    size_t count;
    float value;
    char name[64];
  } mulib_shaderx_node_;
  struct mulib_shaderx
  {
    mulib_shaderx_stage_t stage;
    size_t count, bindings;
    mulib_shaderx_expr_t output;
    mulib_shaderx_node_ nodes[1024];
  };
  static int
  mulib_shaderx_alpha_ (unsigned char c, void *user)
  {
    (void)user;
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }
  static int
  mulib_shaderx_alnum_ (unsigned char c, void *user)
  {
    return mulib_shaderx_alpha_ (c, user) || (c >= '0' && c <= '9');
  }
  static int
  mulib_shaderx_space_ (unsigned char c, void *user)
  {
    (void)user;
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }
  static int
  mulib_shaderx_comment_ (unsigned char c, void *user)
  {
    (void)user;
    return c != '\n' && c != '\r' && c != 0;
  }
  static int
  mulib_shaderx_punct_ (unsigned char c, void *user)
  {
    (void)user;
    return c && strchr ("{}();,=+-*/", c) != NULL;
  }
  static mulib_shaderx_expr_t
  mulib_shaderx_push_ (mulib_shaderx_t *p, mulib_shaderx_node_ n)
  {
    if (!p || p->count == 1024)
      return 0;
    p->nodes[p->count++] = n;
    return (mulib_shaderx_expr_t)p->count;
  }
  mulib_shaderx_t *
  mulib_shaderx_create (mulib_shaderx_stage_t stage)
  {
    mulib_shaderx_t *p;
    if (stage != MULIB_SHADERX_VERTEX && stage != MULIB_SHADERX_FRAGMENT)
      return NULL;
    p = (mulib_shaderx_t *)calloc (1, sizeof (*p));
    if (p)
      p->stage = stage;
    return p;
  }
  void
  mulib_shaderx_destroy (mulib_shaderx_t *p)
  {
    free (p);
  }
  mulib_shaderx_expr_t
  mulib_shaderx_constant (mulib_shaderx_t *p, float value)
  {
    mulib_shaderx_node_ n;
    if (!isfinite (value))
      return 0;
    memset (&n, 0, sizeof n);
    n.kind = 0;
    n.type = MULIB_SHADERX_FLOAT;
    n.value = value;
    return mulib_shaderx_push_ (p, n);
  }
  mulib_shaderx_expr_t
  mulib_shaderx_input (mulib_shaderx_t *p, mulib_shaderx_type_t type,
                       const char *name, int uniform)
  {
    mulib_shaderx_node_ n;
    size_t len = 0;
    mulib_shaderx_expr_t id;
    if (!p || !name || type < 1 || type > 4 || (uniform != 0 && uniform != 1)
        || p->bindings == 64
        || !mulib_shaderx_alpha_ ((unsigned char)name[0], NULL))
      return 0;
    while (name[len])
      {
        if (len == 63
            || !mulib_shaderx_alnum_ ((unsigned char)name[len], NULL))
          return 0;
        ++len;
      }
    for (size_t i = 0; i < p->count; ++i)
      if (p->nodes[i].kind == 1 && !strcmp (p->nodes[i].name, name))
        return 0;
    memset (&n, 0, sizeof n);
    n.kind = 1;
    n.type = type;
    n.uniform = uniform;
    memcpy (n.name, name, len + 1);
    id = mulib_shaderx_push_ (p, n);
    if (id)
      ++p->bindings;
    return id;
  }
  mulib_shaderx_expr_t
  mulib_shaderx_find_input (const mulib_shaderx_t *p, const char *name)
  {
    if (!p || !name)
      return 0;
    for (size_t i = 0; i < p->count; ++i)
      if (p->nodes[i].kind == 1 && !strcmp (p->nodes[i].name, name))
        return (mulib_shaderx_expr_t)i + 1;
    return 0;
  }
  static int
  mulib_shaderx_args_ (const mulib_shaderx_t *p, const mulib_shaderx_expr_t *a,
                       size_t n)
  {
    if (!p || !a || !n || n > 4)
      return 0;
    for (size_t i = 0; i < n; ++i)
      if (!a[i] || a[i] > p->count)
        return 0;
    return 1;
  }
  mulib_shaderx_expr_t
  mulib_shaderx_vector (mulib_shaderx_t *p, mulib_shaderx_type_t type,
                        const mulib_shaderx_expr_t *a, size_t count)
  {
    mulib_shaderx_node_ n;
    size_t width = 0;
    if (type < 2 || type > 4 || !mulib_shaderx_args_ (p, a, count))
      return 0;
    for (size_t i = 0; i < count; ++i)
      width += (size_t)p->nodes[a[i] - 1].type;
    if (width != (size_t)type && !(count == 1 && width == 1))
      return 0;
    memset (&n, 0, sizeof n);
    n.kind = 2;
    n.type = type;
    n.count = count;
    memcpy (n.args, a, count * sizeof (*a));
    return mulib_shaderx_push_ (p, n);
  }
  mulib_shaderx_expr_t
  mulib_shaderx_apply (mulib_shaderx_t *p, mulib_shaderx_op_t op,
                       const mulib_shaderx_expr_t *a, size_t count)
  {
    mulib_shaderx_node_ n;
    mulib_shaderx_type_t type, t0, t1 = (mulib_shaderx_type_t)0;
    if (!mulib_shaderx_args_ (p, a, count))
      return 0;
    if (!count)
      return 0;
    type = t0 = p->nodes[a[0] - 1].type;
    if (count > 1)
      t1 = p->nodes[a[1] - 1].type;
    switch (op)
      {
      case MULIB_SHADERX_ADD:
      case MULIB_SHADERX_SUB:
      case MULIB_SHADERX_MUL:
      case MULIB_SHADERX_DIV:
      case MULIB_SHADERX_MIN:
      case MULIB_SHADERX_MAX:
        if (count != 2 || (t0 != t1 && t0 != 1 && t1 != 1))
          return 0;
        type = t0 > t1 ? t0 : t1;
        break;
      case MULIB_SHADERX_NEG:
      case MULIB_SHADERX_SIN:
      case MULIB_SHADERX_COS:
        if (count != 1)
          return 0;
        break;
      case MULIB_SHADERX_NORMALIZE:
        if (count != 1 || t0 == 1)
          return 0;
        break;
      case MULIB_SHADERX_DOT:
        if (count != 2 || t0 != t1 || t0 == 1)
          return 0;
        type = 1;
        break;
      case MULIB_SHADERX_MIX:
        if (count != 3 || t0 != t1
            || (p->nodes[a[2] - 1].type != 1 && p->nodes[a[2] - 1].type != t0))
          return 0;
        break;
      default:
        return 0;
      }
    memset (&n, 0, sizeof n);
    n.kind = 3;
    n.op = op;
    n.type = (mulib_shaderx_type_t)type;
    n.count = count;
    memcpy (n.args, a, count * sizeof (*a));
    return mulib_shaderx_push_ (p, n);
  }
  int
  mulib_shaderx_output (mulib_shaderx_t *p, mulib_shaderx_expr_t id)
  {
    if (!p || !id || id > p->count || p->nodes[id - 1].type != 4)
      return 0;
    p->output = id;
    return 1;
  }
  static int
  mulib_shaderx_tokens_ (const char *s, size_t n, mulib_parsec_result_t *out)
  {
    mulib_parsec_parser_t first
        = mulib_parsec_satisfy (mulib_shaderx_alpha_, NULL, "identifier");
    mulib_parsec_parser_t tail
        = mulib_parsec_satisfy (mulib_shaderx_alnum_, NULL, "identifier");
    mulib_parsec_parser_t idparts[2]
        = { first, mulib_parsec_repeat (&tail, 0, SIZE_MAX) };
    mulib_parsec_parser_t id = mulib_parsec_sequence (idparts, 2),
                          digit = mulib_parsec_range ('0', '9');
    mulib_parsec_parser_t digits = mulib_parsec_repeat (&digit, 1, SIZE_MAX),
                          dot = mulib_parsec_byte ('.');
    mulib_parsec_parser_t fracparts[2]
        = { dot, mulib_parsec_repeat (&digit, 0, SIZE_MAX) };
    mulib_parsec_parser_t frac = mulib_parsec_sequence (fracparts, 2);
    mulib_parsec_parser_t wholeparts[2]
        = { digits, mulib_parsec_repeat (&frac, 0, 1) };
    mulib_parsec_parser_t shortparts[2] = { dot, digits };
    mulib_parsec_parser_t mantissas[2]
        = { mulib_parsec_sequence (wholeparts, 2),
            mulib_parsec_sequence (shortparts, 2) };
    mulib_parsec_parser_t signs[2]
        = { mulib_parsec_byte ('+'), mulib_parsec_byte ('-') },
        sign = mulib_parsec_choice (signs, 2);
    mulib_parsec_parser_t es[2]
        = { mulib_parsec_byte ('e'), mulib_parsec_byte ('E') };
    mulib_parsec_parser_t expparts[3]
        = { mulib_parsec_choice (es, 2), mulib_parsec_repeat (&sign, 0, 1),
            digits };
    mulib_parsec_parser_t exponent = mulib_parsec_sequence (expparts, 3);
    mulib_parsec_parser_t numparts[2]
        = { mulib_parsec_choice (mantissas, 2),
            mulib_parsec_repeat (&exponent, 0, 1) };
    mulib_parsec_parser_t number = mulib_parsec_sequence (numparts, 2);
    mulib_parsec_parser_t space
        = mulib_parsec_satisfy (mulib_shaderx_space_, NULL, "space");
    mulib_parsec_parser_t body
        = mulib_parsec_satisfy (mulib_shaderx_comment_, NULL, "comment");
    mulib_parsec_parser_t commentparts[2]
        = { mulib_parsec_literal ("//", 2),
            mulib_parsec_repeat (&body, 0, SIZE_MAX) };
    mulib_parsec_parser_t punct
        = mulib_parsec_satisfy (mulib_shaderx_punct_, NULL, "punctuation");
    mulib_parsec_parser_t choices[5]
        = { mulib_parsec_repeat (&space, 1, SIZE_MAX),
            mulib_parsec_sequence (commentparts, 2),
            mulib_parsec_capture (&id, 1), mulib_parsec_capture (&number, 2),
            mulib_parsec_capture (&punct, 3) };
    mulib_parsec_parser_t token = mulib_parsec_choice (choices, 5);
    mulib_parsec_parser_t all[2]
        = { mulib_parsec_repeat (&token, 0, SIZE_MAX), mulib_parsec_end () };
    mulib_parsec_parser_t parser = mulib_parsec_sequence (all, 2);
    mulib_parsec_options_t options = { 64, 2000000, 16384 };
    return mulib_parsec_run (&parser, s, n, &options, out);
  }
  typedef struct mulib_shaderx_parser_
  {
    const char *source;
    size_t length, at;
    unsigned depth;
    mulib_parsec_result_t tokens;
    mulib_shaderx_t *program;
    mulib_shaderx_error_t error;
    struct
    {
      char name[64];
      mulib_shaderx_expr_t id;
    } symbols[256];
    size_t count;
  } mulib_shaderx_parser_;
  static void
  mulib_shaderx_fail_ (mulib_shaderx_parser_ *p, mulib_shaderx_status_t code,
                       const char *message)
  {
    if (!p->error.code)
      {
        p->error.code = code;
        p->error.message = message;
        p->error.offset = p->at < p->tokens.count
                              ? p->tokens.captures[p->at].offset
                              : p->length;
      }
  }
  static int
  mulib_shaderx_is_ (mulib_shaderx_parser_ *p, const char *s)
  {
    mulib_parsec_capture_t *t;
    if (p->at == p->tokens.count)
      return 0;
    t = &p->tokens.captures[p->at];
    return strlen (s) == t->length
           && !memcmp (p->source + t->offset, s, t->length);
  }
  static int
  mulib_shaderx_take_ (mulib_shaderx_parser_ *p, const char *s)
  {
    if (mulib_shaderx_is_ (p, s))
      {
        ++p->at;
        return 1;
      }
    return 0;
  }
  static int
  mulib_shaderx_expect_ (mulib_shaderx_parser_ *p, const char *s)
  {
    if (mulib_shaderx_take_ (p, s))
      return 1;
    mulib_shaderx_fail_ (p, MULIB_SHADERX_SYNTAX, "unexpected token");
    return 0;
  }
  static int
  mulib_shaderx_name_ (mulib_shaderx_parser_ *p, char out[64])
  {
    mulib_parsec_capture_t *t;
    if (p->at == p->tokens.count || p->tokens.captures[p->at].tag != 1)
      {
        mulib_shaderx_fail_ (p, MULIB_SHADERX_SYNTAX, "expected identifier");
        return 0;
      }
    t = &p->tokens.captures[p->at];
    if (t->length > 63)
      {
        mulib_shaderx_fail_ (p, MULIB_SHADERX_LIMIT, "identifier too long");
        return 0;
      }
    memcpy (out, p->source + t->offset, t->length);
    out[t->length] = 0;
    ++p->at;
    return 1;
  }
  static mulib_shaderx_type_t
  mulib_shaderx_type_ (const char *s)
  {
    if (!strcmp (s, "float"))
      return MULIB_SHADERX_FLOAT;
    if (!strcmp (s, "vec2"))
      return MULIB_SHADERX_VEC2;
    if (!strcmp (s, "vec3"))
      return MULIB_SHADERX_VEC3;
    return !strcmp (s, "vec4") ? MULIB_SHADERX_VEC4 : (mulib_shaderx_type_t)0;
  }
  static int
  mulib_shaderx_symbol_ (mulib_shaderx_parser_ *p, const char *name,
                         mulib_shaderx_expr_t id)
  {
    if (p->count == 256)
      {
        mulib_shaderx_fail_ (p, MULIB_SHADERX_LIMIT, "symbol limit");
        return 0;
      }
    for (size_t i = 0; i < p->count; ++i)
      if (!strcmp (name, p->symbols[i].name))
        {
          mulib_shaderx_fail_ (p, MULIB_SHADERX_TYPE, "duplicate name");
          return 0;
        }
    strcpy (p->symbols[p->count].name, name);
    p->symbols[p->count++].id = id;
    return 1;
  }
  static int
  mulib_shaderx_number_ (const char *s, size_t n, float *out)
  {
    size_t i = 0;
    double v = 0, scale = 1;
    int exponent = 0, negative = 0;
    if (n > 63)
      return 0;
    while (i < n && s[i] >= '0' && s[i] <= '9')
      v = v * 10 + (s[i++] - '0');
    if (i < n && s[i] == '.')
      {
        ++i;
        while (i < n && s[i] >= '0' && s[i] <= '9')
          {
            scale *= 0.1;
            v += (s[i++] - '0') * scale;
          }
      }
    if (i < n && (s[i] == 'e' || s[i] == 'E'))
      {
        ++i;
        if (i < n && (s[i] == '+' || s[i] == '-'))
          negative = s[i++] == '-';
        while (i < n && s[i] >= '0' && s[i] <= '9')
          {
            if (exponent > 512)
              return 0;
            exponent = exponent * 10 + s[i++] - '0';
          }
        if (v != 0)
          v *= pow (10.0, negative ? -exponent : exponent);
      }
    if (i != n || !isfinite (v) || v > FLT_MAX)
      return 0;
    *out = (float)v;
    return 1;
  }
  static mulib_shaderx_expr_t mulib_shaderx_expr_ (mulib_shaderx_parser_ *p,
                                                   int precedence);
  static mulib_shaderx_expr_t
  mulib_shaderx_primary_ (mulib_shaderx_parser_ *p)
  {
    mulib_shaderx_expr_t id = 0, args[4];
    size_t count = 0;
    char name[64];
    if (mulib_shaderx_take_ (p, "-"))
      {
        args[0] = mulib_shaderx_expr_ (p, 3);
        return mulib_shaderx_apply (p->program, MULIB_SHADERX_NEG, args, 1);
      }
    if (mulib_shaderx_take_ (p, "+"))
      return mulib_shaderx_expr_ (p, 3);
    if (mulib_shaderx_take_ (p, "("))
      {
        id = mulib_shaderx_expr_ (p, 0);
        if (!mulib_shaderx_expect_ (p, ")"))
          return 0;
        return id;
      }
    if (p->at < p->tokens.count && p->tokens.captures[p->at].tag == 2)
      {
        mulib_parsec_capture_t *t = &p->tokens.captures[p->at];
        float v;
        if (!mulib_shaderx_number_ (p->source + t->offset, t->length, &v))
          {
            mulib_shaderx_fail_ (p, MULIB_SHADERX_LIMIT,
                                 "numeric literal out of range");
            return 0;
          }
        ++p->at;
        return mulib_shaderx_constant (p->program, v);
      }
    if (!mulib_shaderx_name_ (p, name))
      return 0;
    if (!mulib_shaderx_take_ (p, "("))
      {
        for (size_t i = 0; i < p->count; ++i)
          if (!strcmp (name, p->symbols[i].name))
            return p->symbols[i].id;
        mulib_shaderx_fail_ (p, MULIB_SHADERX_TYPE, "unknown name");
        return 0;
      }
    if (!mulib_shaderx_is_ (p, ")"))
      do
        {
          if (count == 4)
            {
              mulib_shaderx_fail_ (p, MULIB_SHADERX_LIMIT, "argument limit");
              return 0;
            }
          args[count] = mulib_shaderx_expr_ (p, 0);
          if (!args[count++])
            return 0;
        }
      while (mulib_shaderx_take_ (p, ","));
    if (!mulib_shaderx_expect_ (p, ")"))
      return 0;
    if (mulib_shaderx_type_ (name) >= 2)
      id = mulib_shaderx_vector (p->program, mulib_shaderx_type_ (name), args,
                                 count);
    else
      {
        static const char *const names[]
            = { "dot", "normalize", "sin", "cos", "mix", "min", "max" };
        static const mulib_shaderx_op_t ops[]
            = { MULIB_SHADERX_DOT, MULIB_SHADERX_NORMALIZE, MULIB_SHADERX_SIN,
                MULIB_SHADERX_COS, MULIB_SHADERX_MIX,       MULIB_SHADERX_MIN,
                MULIB_SHADERX_MAX };
        for (size_t i = 0; i < 7; ++i)
          if (!strcmp (name, names[i]))
            id = mulib_shaderx_apply (p->program, ops[i], args, count);
      }
    if (!id)
      mulib_shaderx_fail_ (p,
                           p->program->count == 1024 ? MULIB_SHADERX_LIMIT
                                                     : MULIB_SHADERX_TYPE,
                           "invalid call or argument types");
    return id;
  }
  static mulib_shaderx_expr_t
  mulib_shaderx_expr_ (mulib_shaderx_parser_ *p, int precedence)
  {
    mulib_shaderx_expr_t left;
    if (++p->depth > 64)
      {
        --p->depth;
        mulib_shaderx_fail_ (p, MULIB_SHADERX_LIMIT,
                             "expression nesting limit");
        return 0;
      }
    left = mulib_shaderx_primary_ (p);
    while (left && !p->error.code)
      {
        int level = 0;
        mulib_shaderx_op_t op = MULIB_SHADERX_ADD;
        mulib_shaderx_expr_t args[2];
        if (mulib_shaderx_is_ (p, "+"))
          level = 1;
        else if (mulib_shaderx_is_ (p, "-"))
          {
            level = 1;
            op = MULIB_SHADERX_SUB;
          }
        else if (mulib_shaderx_is_ (p, "*"))
          {
            level = 2;
            op = MULIB_SHADERX_MUL;
          }
        else if (mulib_shaderx_is_ (p, "/"))
          {
            level = 2;
            op = MULIB_SHADERX_DIV;
          }
        if (!level || level < precedence)
          break;
        ++p->at;
        args[0] = left;
        args[1] = mulib_shaderx_expr_ (p, level + 1);
        left = mulib_shaderx_apply (p->program, op, args, 2);
      }
    if (!left && !p->error.code)
      mulib_shaderx_fail_ (p,
                           p->program->count == 1024 ? MULIB_SHADERX_LIMIT
                                                     : MULIB_SHADERX_TYPE,
                           "invalid expression types");
    --p->depth;
    return left;
  }
  mulib_shaderx_t *
  mulib_shaderx_parse (const char *source, size_t length,
                       mulib_shaderx_error_t *error)
  {
    mulib_shaderx_parser_ p;
    mulib_shaderx_stage_t stage = MULIB_SHADERX_VERTEX;
    memset (&p, 0, sizeof p);
    p.source = source;
    p.length = length;
    if ((!source && length) || length > 65536)
      {
        p.error.code
            = length > 65536 ? MULIB_SHADERX_LIMIT : MULIB_SHADERX_INVALID;
        p.error.message = "invalid source length";
      }
    else if (mulib_shaderx_tokens_ (source, length, &p.tokens) != 1)
      {
        p.error.code = p.tokens.error.code == MULIB_PARSEC_MEMORY
                           ? MULIB_SHADERX_MEMORY
                       : p.tokens.error.code == MULIB_PARSEC_LIMIT
                           ? MULIB_SHADERX_LIMIT
                           : MULIB_SHADERX_SYNTAX;
        p.error.offset = p.tokens.error.offset;
        p.error.message = "invalid token";
      }
    if (!p.error.code)
      {
        if (mulib_shaderx_take_ (&p, "fragment"))
          stage = MULIB_SHADERX_FRAGMENT;
        else if (!mulib_shaderx_expect_ (&p, "vertex"))
          goto done;
        p.program = mulib_shaderx_create (stage);
        if (!p.program)
          {
            mulib_shaderx_fail_ (&p, MULIB_SHADERX_MEMORY,
                                 "allocation failed");
            goto done;
          }
        if (!mulib_shaderx_expect_ (&p, "{"))
          goto done;
        while (!p.error.code && !mulib_shaderx_is_ (&p, "return"))
          {
            char name[64], type[64];
            mulib_shaderx_expr_t id = 0;
            if (mulib_shaderx_is_ (&p, "input")
                || mulib_shaderx_is_ (&p, "uniform"))
              {
                int uniform = mulib_shaderx_is_ (&p, "uniform");
                ++p.at;
                if (!mulib_shaderx_name_ (&p, type)
                    || !mulib_shaderx_name_ (&p, name))
                  break;
                id = mulib_shaderx_input (
                    p.program, mulib_shaderx_type_ (type), name, uniform);
              }
            else if (mulib_shaderx_take_ (&p, "let"))
              {
                if (!mulib_shaderx_name_ (&p, name)
                    || !mulib_shaderx_expect_ (&p, "="))
                  break;
                id = mulib_shaderx_expr_ (&p, 0);
              }
            else
              {
                mulib_shaderx_fail_ (&p, MULIB_SHADERX_SYNTAX,
                                     "expected input, uniform, let or return");
                break;
              }
            if (!id)
              {
                mulib_shaderx_fail_ (&p, MULIB_SHADERX_TYPE,
                                     "invalid declaration");
                break;
              }
            if (!mulib_shaderx_symbol_ (&p, name, id)
                || !mulib_shaderx_expect_ (&p, ";"))
              break;
          }
        if (!p.error.code && mulib_shaderx_expect_ (&p, "return"))
          {
            mulib_shaderx_expr_t id = mulib_shaderx_expr_ (&p, 0);
            if (!mulib_shaderx_output (p.program, id))
              mulib_shaderx_fail_ (&p, MULIB_SHADERX_TYPE,
                                   "stage result must be vec4");
            if (!p.error.code && mulib_shaderx_expect_ (&p, ";")
                && mulib_shaderx_expect_ (&p, "}") && p.at != p.tokens.count)
              mulib_shaderx_fail_ (&p, MULIB_SHADERX_SYNTAX, "trailing input");
          }
      }
  done:
    mulib_parsec_result_free (&p.tokens);
    if (error)
      *error = p.error;
    if (p.error.code)
      {
        mulib_shaderx_destroy (p.program);
        return NULL;
      }
    return p.program;
  }
  static int
  mulib_shaderx_write_ (mulib_strbuf_t *b, const char *format, ...)
  {
    char text[256];
    int n;
    va_list args;
    va_start (args, format);
    n = vsnprintf (text, sizeof text, format, args);
    va_end (args);
    return n >= 0 && (size_t)n < sizeof text
           && mulib_strbuf_append_n (b, text, (size_t)n);
  }
  static const char *
  mulib_shaderx_spelling_ (int type, int hlsl)
  {
    static const char *const names[2][4]
        = { { "float", "vec2", "vec3", "vec4" },
            { "float", "float2", "float3", "float4" } };
    return names[hlsl][type - 1];
  }
  int
  mulib_shaderx_emit (const mulib_shaderx_t *p, mulib_shaderx_target_t target,
                      mulib_strbuf_t *out)
  {
    mulib_strbuf_t b;
    int ok = 1, hlsl = target == MULIB_SHADERX_HLSL5;
    unsigned location = 0;
    size_t uniforms = 0;
    if (!p || !out || !p->output || (target != MULIB_SHADERX_GLSL330 && !hlsl)
        || sizeof (float) != sizeof (uint32_t) || FLT_RADIX != 2
        || FLT_MANT_DIG != 24)
      return 0;
    mulib_strbuf_init (&b);
    if (!hlsl)
      ok = mulib_strbuf_append (&b, "#version 330 core\n");
    for (size_t i = 0; i < p->count; ++i)
      if (p->nodes[i].kind == 1 && p->nodes[i].uniform)
        ++uniforms;
    if (hlsl && uniforms)
      ok = ok
           && mulib_strbuf_append (&b,
                                   "cbuffer SXUniforms : register(b0) {\n");
    for (size_t i = 0; i < p->count; ++i)
      {
        const mulib_shaderx_node_ *n = &p->nodes[i];
        const char *type = mulib_shaderx_spelling_ (n->type, hlsl);
        if (n->kind != 1)
          continue;
        if (n->uniform)
          ok = ok
               && mulib_shaderx_write_ (&b, "%s%s sx_in%u;\n",
                                        hlsl ? "  " : "uniform ", type,
                                        (unsigned)i + 1);
        else if (!hlsl)
          {
            /* GLSL 330 explicit locations apply to vertex inputs, not fragment
             * varyings. */
            if (p->stage == MULIB_SHADERX_VERTEX)
              ok = ok
                   && mulib_shaderx_write_ (&b, "layout(location=%u) ",
                                            location);
            ok = ok
                 && mulib_shaderx_write_ (&b, "in %s sx_in%u;\n", type,
                                          (unsigned)i + 1);
            ++location;
          }
      }
    if (hlsl && uniforms)
      ok = ok && mulib_strbuf_append (&b, "};\n");
    if (hlsl)
      {
        ok = ok && mulib_strbuf_append (&b, "float4 main(");
        location = 0;
        for (size_t i = 0; i < p->count; ++i)
          if (p->nodes[i].kind == 1 && !p->nodes[i].uniform)
            {
              ok = ok
                   && mulib_shaderx_write_ (
                       &b, "%s%s sx_in%u : TEXCOORD%u", location ? ", " : "",
                       mulib_shaderx_spelling_ (p->nodes[i].type, 1),
                       (unsigned)i + 1, location);
              ++location;
            }
        ok = ok
             && mulib_shaderx_write_ (&b, ") : %s {\n",
                                      p->stage == MULIB_SHADERX_VERTEX
                                          ? "SV_Position"
                                          : "SV_Target0");
      }
    else
      {
        if (p->stage == MULIB_SHADERX_FRAGMENT)
          ok = ok
               && mulib_strbuf_append (
                   &b, "layout(location=0) out vec4 sx_out;\n");
        ok = ok && mulib_strbuf_append (&b, "void main() {\n");
      }
    for (size_t i = 0; i < p->count && ok; ++i)
      {
        const mulib_shaderx_node_ *n = &p->nodes[i];
        ok = mulib_shaderx_write_ (
            &b, "  %s sx_v%u = ", mulib_shaderx_spelling_ (n->type, hlsl),
            (unsigned)i + 1);
        if (n->kind == 0)
          {
            uint32_t bits;
            memcpy (&bits, &n->value, sizeof bits);
            ok = ok
                 && mulib_shaderx_write_ (&b, "%s(%uu)",
                                          hlsl ? "asfloat" : "uintBitsToFloat",
                                          (unsigned)bits);
          }
        else if (n->kind == 1)
          ok = ok && mulib_shaderx_write_ (&b, "sx_in%u", (unsigned)i + 1);
        else if (n->kind == 3 && n->op <= MULIB_SHADERX_DIV)
          ok = ok
               && mulib_shaderx_write_ (&b, "sx_v%u %c sx_v%u", n->args[0],
                                        "+-*/"[n->op], n->args[1]);
        else if (n->kind == 3 && n->op == MULIB_SHADERX_NEG)
          ok = ok && mulib_shaderx_write_ (&b, "-sx_v%u", n->args[0]);
        else
          {
            const char *name = mulib_shaderx_spelling_ (n->type, hlsl);
            if (n->kind == 3)
              {
                switch (n->op)
                  {
                  case MULIB_SHADERX_DOT:
                    name = "dot";
                    break;
                  case MULIB_SHADERX_NORMALIZE:
                    name = "normalize";
                    break;
                  case MULIB_SHADERX_SIN:
                    name = "sin";
                    break;
                  case MULIB_SHADERX_COS:
                    name = "cos";
                    break;
                  case MULIB_SHADERX_MIX:
                    name = hlsl ? "lerp" : "mix";
                    break;
                  case MULIB_SHADERX_MIN:
                    name = "min";
                    break;
                  default:
                    name = "max";
                    break;
                  }
              }
            ok = ok && mulib_shaderx_write_ (&b, "%s(", name);
            for (size_t k = 0; k < n->count; ++k)
              {
                /* GLSL min/max require the vector argument first, or explicit
                 * broadcast. */
                int broadcast = n->kind == 3
                                && (n->op == MULIB_SHADERX_MIN
                                    || n->op == MULIB_SHADERX_MAX)
                                && n->type > 1
                                && p->nodes[n->args[k] - 1].type == 1;
                ok = ok && mulib_shaderx_write_ (&b, "%s", k ? ", " : "");
                if (broadcast)
                  ok = ok
                       && mulib_shaderx_write_ (
                           &b, "%s(", mulib_shaderx_spelling_ (n->type, hlsl));
                ok = ok
                     && mulib_shaderx_write_ (&b, "sx_v%u%s", n->args[k],
                                              broadcast ? ")" : "");
              }
            ok = ok && mulib_strbuf_append (&b, ")");
          }
        ok = ok && mulib_strbuf_append (&b, ";\n");
      }
    ok = ok
         && mulib_shaderx_write_ (&b, "  %s sx_v%u;\n}\n",
                                  hlsl ? "return"
                                  : p->stage == MULIB_SHADERX_VERTEX
                                      ? "gl_Position ="
                                      : "sx_out =",
                                  p->output);
    if (ok)
      ok = mulib_strbuf_append_n (out, b.data, b.length);
    mulib_strbuf_free (&b);
    return ok;
  }
#ifdef __cplusplus
}
#endif
#endif
