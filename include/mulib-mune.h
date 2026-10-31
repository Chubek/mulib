/** @file mulib-mune.h
 * @brief Independent Lua runtime, AST frontend, VM/JIT/C AOT and module bundles.
 * Define MULIB_IMPL_MUNE and the dependencies listed in the manual.
 */
#ifndef MULIB_MUNE_H
#define MULIB_MUNE_H
#include "mulib-parsec.h"
#include "mulib-jit.h"
#include "mulib-dynload.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return API revision 1. */
  unsigned mulib_mune_version (void);
  typedef struct mulib_mune mulib_mune_t;
  typedef enum mulib_mune_status
  {
    MULIB_MUNE_OK,
    MULIB_MUNE_INVALID,
    MULIB_MUNE_SYNTAX,
    MULIB_MUNE_NAME,
    MULIB_MUNE_ARITHMETIC,
    MULIB_MUNE_LIMIT,
    MULIB_MUNE_MEMORY,
    MULIB_MUNE_IO,
    MULIB_MUNE_SEMANTIC,
    MULIB_MUNE_RUNTIME
  } mulib_mune_status_t;
  /** @brief Diagnostic and zero-based source byte offset. Parse messages are
   * static; execution messages borrow storage from the state. */
  typedef struct mulib_mune_error
  {
    mulib_mune_status_t code;
    size_t offset;
    const char *message;
  } mulib_mune_error_t;
  /** @brief Allocate an empty persistent numeric environment; NULL on OOM. */
  mulib_mune_t *mulib_mune_create (void);
  /** @brief Release environment; NULL safe. Objects require external
   * synchronization. */
  void mulib_mune_destroy (mulib_mune_t *interpreter);
  /** @brief Evaluate at most 64 KiB with bounded work/depth/storage. All
   * assignments commit together on success; output unchanged on error. Empty
   * script returns 0. Bytes are borrowed for the call only; NULL is allowed at
   * length zero.
   */
  mulib_mune_status_t mulib_mune_eval (mulib_mune_t *interpreter,
                                       const char *source, size_t length,
                                       double *out, mulib_mune_error_t *error);
  /** @brief Copy a variable value; return 0 for missing/invalid, output
   * unchanged. */
  int mulib_mune_get (const mulib_mune_t *interpreter, const char *name,
                       double *out);
  typedef struct mulib_mune_chunk mulib_mune_chunk_t;
  /** Frontend limits. Zero fields select defaults: 1 MiB source, 100000 tokens,
   * 100000 AST nodes, 100000 semantic records, and 10000000 parser steps.
   */
  typedef struct mulib_mune_parse_options
  {
    size_t source_bytes, tokens, nodes, symbols, steps;
  } mulib_mune_parse_options_t;
  /** Parse Lua 5.4 source into an owned production AST and perform semantic
   * checks. No source is executed. Copies all source bytes. Error messages are
   * static; error.offset is a zero-based byte offset in the original source.
   */
  mulib_mune_chunk_t *mulib_mune_parse (const char *source, size_t length,
                                       const mulib_mune_parse_options_t *options,
                                       mulib_mune_error_t *error);
  /** Parse a file; accepts the Lua loadfile UTF-8 BOM/first-line # convention. */
  mulib_mune_chunk_t *mulib_mune_parse_file (
      const char *path, const mulib_mune_parse_options_t *options,
      mulib_mune_error_t *error);
  /** Borrow the checked AST; text spans refer to copied original source, not
   * the internal token stream. Do not mutate it. Valid until chunk destruction.
   */
  const mulib_ast_t *mulib_mune_chunk_ast (const mulib_mune_chunk_t *chunk);
  void mulib_mune_chunk_destroy (mulib_mune_chunk_t *chunk);
  typedef struct mulib_mune_state mulib_mune_state_t;
  typedef struct mulib_mune_result mulib_mune_result_t;
  typedef struct mulib_mune_object mulib_mune_object_t;
  typedef enum mulib_mune_type
  {
    MULIB_MUNE_NIL, MULIB_MUNE_BOOLEAN, MULIB_MUNE_INTEGER,
    MULIB_MUNE_NUMBER, MULIB_MUNE_STRING, MULIB_MUNE_TABLE,
    MULIB_MUNE_FUNCTION, MULIB_MUNE_USERDATA, MULIB_MUNE_THREAD
  } mulib_mune_type_t;
  /** Scalar values are copied; object values borrow storage from their state.
   * Keep objects in a result or global while running code/collecting garbage.
   */
  typedef struct mulib_mune_value
  {
    mulib_mune_type_t type;
    union { int boolean; int64_t integer; double number; mulib_mune_object_t *object; } as;
  } mulib_mune_value_t;
  typedef struct mulib_mune_runtime_options
  {
    /** Zero selects 64 MiB runtime heap, 10000000 steps, 200 calls and 100000
     * values per list. ASTs, compiler images and native thread stacks are
     * separately owned and are not included in the runtime heap budget. */
    size_t memory_bytes, steps, call_depth, results;
  } mulib_mune_runtime_options_t;
  typedef enum mulib_mune_mode
  {
    MULIB_MUNE_TREE, MULIB_MUNE_VM, MULIB_MUNE_JIT, MULIB_MUNE_AOT
  } mulib_mune_mode_t;
  /** Select execution for subsequent calls. Native JIT requires a supported
   * host. AOT entries are supplied by generated C, through run_aot below. */
  mulib_mune_status_t mulib_mune_set_mode (mulib_mune_state_t *state, mulib_mune_mode_t mode);
  typedef struct mulib_mune_aot_entry
  {
    size_t block_offset;
    const mulib_mir_flow_program_t *(*program)(void);
    mulib_mir_status_t (*execute)(mulib_mir_frame_t *);
    const char *production; /**< Node production disambiguates shared offsets. */
  } mulib_mune_aot_entry_t;
  /** Generate C11 and a public function `symbol(state,result,error)`.
   * Compile with MuLIB, math, threads and the platform dynamic-loader library.
   * symbol uses MIR's naming rules. Entry arrays must outlive retained closures.
   * The generated translation unit embeds its Lua source and all block images.
   */
  mulib_mune_status_t mulib_mune_emit_c (mulib_mune_chunk_t *chunk, const char *symbol,
      mulib_mir_write_fn writer, void *user, mulib_mune_error_t *error);
  mulib_mune_status_t mulib_mune_run_aot (mulib_mune_state_t *state,
      const char *source, size_t length, const mulib_mune_aot_entry_t *entries,
      size_t entry_count, mulib_mune_result_t **result, mulib_mune_error_t *error);
  /** Native arguments and output builder are borrowed for the callback. */
  typedef mulib_mune_status_t (*mulib_mune_native_fn) (
      mulib_mune_state_t *, const mulib_mune_value_t *, size_t,
      mulib_mune_result_t *, void *);
  /** Create an independent Lua state. Zero options select documented defaults. */
  mulib_mune_state_t *mulib_mune_state_create (const mulib_mune_runtime_options_t *options);
  void mulib_mune_state_destroy (mulib_mune_state_t *state);
  /** Execute checked source. *result must be NULL; success transfers an owned,
   * GC-rooted result list. Runtime errors preserve preceding Lua side effects.
   * Runtime diagnostic text is borrowed until the state's next execution.
   */
  mulib_mune_status_t mulib_mune_run (mulib_mune_state_t *state,
      mulib_mune_chunk_t *chunk, mulib_mune_result_t **result, mulib_mune_error_t *error);
  mulib_mune_status_t mulib_mune_run_source (mulib_mune_state_t *state,
      const char *source, size_t length, mulib_mune_result_t **result, mulib_mune_error_t *error);
  mulib_mune_status_t mulib_mune_run_file (mulib_mune_state_t *state,
      const char *path, mulib_mune_result_t **result, mulib_mune_error_t *error);
  size_t mulib_mune_result_count (const mulib_mune_result_t *result);
  mulib_mune_value_t mulib_mune_result_get (const mulib_mune_result_t *result, size_t index);
  int mulib_mune_result_push (mulib_mune_result_t *result, mulib_mune_value_t value);
  void mulib_mune_result_destroy (mulib_mune_result_t *result);
  const char *mulib_mune_value_string (mulib_mune_value_t value, size_t *length);
  mulib_mune_status_t mulib_mune_make_string (mulib_mune_state_t *state,
      const char *bytes, size_t length, mulib_mune_value_t *value);
  mulib_mune_status_t mulib_mune_get_global (mulib_mune_state_t *state,
      const char *name, mulib_mune_value_t *value);
  mulib_mune_status_t mulib_mune_set_global (mulib_mune_state_t *state,
      const char *name, mulib_mune_value_t value);
  mulib_mune_status_t mulib_mune_bind (mulib_mune_state_t *state,
       const char *name, mulib_mune_native_fn function, void *user);
  mulib_mune_result_t *mulib_mune_result_create (mulib_mune_state_t *state);
  mulib_mune_status_t mulib_mune_call (mulib_mune_state_t *state, mulib_mune_value_t function,
      const mulib_mune_value_t *arguments, size_t count, mulib_mune_result_t **result,
      mulib_mune_error_t *error);
  mulib_mune_status_t mulib_mune_make_table (mulib_mune_state_t *, mulib_mune_value_t *);
  mulib_mune_status_t mulib_mune_raw_get (mulib_mune_state_t *, mulib_mune_value_t table,
      mulib_mune_value_t key, mulib_mune_value_t *value);
  mulib_mune_status_t mulib_mune_raw_set (mulib_mune_state_t *, mulib_mune_value_t table,
      mulib_mune_value_t key, mulib_mune_value_t value);
  mulib_mune_status_t mulib_mune_set_metatable (mulib_mune_state_t *, mulib_mune_value_t,
      mulib_mune_value_t metatable);
  /** State-owned, zero-initialized userdata with indexed Lua user values. */
  mulib_mune_status_t mulib_mune_make_userdata (mulib_mune_state_t *, size_t bytes,
      mulib_mune_value_t *value, void **data);
  void *mulib_mune_value_userdata (mulib_mune_value_t value, size_t *bytes);
  /** Collect unreachable runtime objects; return currently allocated bytes. */
  size_t mulib_mune_collect (mulib_mune_state_t *state);
  typedef struct mulib_mune_bundle_module
  {
    const char *name, *source;
    size_t length;
  } mulib_mune_bundle_module_t;
  /** Write a portable Mune module bundle, conventionally named .luac. It is an
   * independent versioned container of checked Lua source and compiled MIR.
   * The loader verifies images against source/helper bindings before execution.
   * Loading installs lazy module loaders in package.preload. */
  mulib_mune_status_t mulib_mune_bundle_write (const mulib_mune_bundle_module_t *modules,
      size_t count, mulib_mir_write_fn writer, void *user, mulib_mune_error_t *error);
  mulib_mune_status_t mulib_mune_load_bundle (mulib_mune_state_t *state,
      const void *bytes, size_t length, mulib_mune_error_t *error);
  mulib_mune_status_t mulib_mune_load_bundle_file (mulib_mune_state_t *state,
      const char *path, mulib_mune_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUNE) && !defined(MULIB_MUNE_IMPLEMENTED)
#define MULIB_MUNE_IMPLEMENTED
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>
#include <locale.h>
#include <limits.h>
#if !defined(__STDC_NO_THREADS__)
#include <threads.h>
#define MULIB_MUNE_THREADS_ 1
#endif
#if defined(__unix__)
#include <sys/wait.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  unsigned
  mulib_mune_version (void)
  {
    return 1;
  }
  struct mulib_mune
  {
    size_t count;
    struct
    {
      char name[64];
      double value;
    } vars[128];
  };
  typedef struct mulib_mune_parser_
  {
    const char *source;
    size_t length, at, token_at, steps;
    unsigned token, depth;
    char word[128];
    double number;
    mulib_mune_t *env;
    mulib_mune_error_t error;
  } mulib_mune_parser_;
  static int
  mulib_mune_fail_ (mulib_mune_parser_ *p, mulib_mune_status_t code,
                    const char *message)
  {
    if (!p->error.code)
      {
        p->error.code = code;
        p->error.offset = p->token_at;
        p->error.message = message;
      }
    return 0;
  }
  static int
  mulib_mune_alpha_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }
  static int
  mulib_mune_digit_ (unsigned char c)
  {
    return c >= '0' && c <= '9';
  }
  static int
  mulib_mune_next_ (mulib_mune_parser_ *p)
  {
    size_t start, n;
    unsigned char c;
    if (++p->steps > 10000)
      return mulib_mune_fail_ (p, MULIB_MUNE_LIMIT, "token limit");
    for (;;)
      {
        while (p->at < p->length
               && (p->source[p->at] == ' ' || p->source[p->at] == '\t'
                   || p->source[p->at] == '\r' || p->source[p->at] == '\n'))
          ++p->at;
        if (p->length - p->at >= 2 && p->source[p->at] == '-'
            && p->source[p->at + 1] == '-')
          {
            p->at += 2;
            if (p->length - p->at >= 2 && p->source[p->at] == '['
                && p->source[p->at + 1] == '[')
              return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                       "long comments unsupported");
            while (p->at < p->length && p->source[p->at] != '\n')
              ++p->at;
          }
        else
          break;
      }
    p->token_at = p->at;
    if (p->at == p->length)
      {
        p->token = 0;
        return 1;
      }
    c = (unsigned char)p->source[p->at++];
    start = p->token_at;
    if (mulib_mune_alpha_ (c))
      {
        while (p->at < p->length
               && (mulib_mune_alpha_ ((unsigned char)p->source[p->at])
                   || mulib_mune_digit_ ((unsigned char)p->source[p->at])))
          ++p->at;
        n = p->at - start;
        if (n > 63)
          return mulib_mune_fail_ (p, MULIB_MUNE_LIMIT, "identifier limit");
        memcpy (p->word, p->source + start, n);
        p->word[n] = 0;
        p->token = !strcmp (p->word, "return") ? 258 : 257;
        if (p->token == 257)
          {
            static const char *const reserved[]
                = { "and",   "break", "do",       "else", "elseif", "end",
                    "false", "for",   "function", "goto", "if",     "in",
                    "local", "nil",   "not",      "or",   "repeat", "then",
                    "true",  "until", "while" };
            for (size_t i = 0; i < sizeof (reserved) / sizeof (reserved[0]);
                 ++i)
              if (!strcmp (p->word, reserved[i]))
                return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                         "unsupported Lua keyword");
          }
        return 1;
      }
    if (mulib_mune_digit_ (c)
        || (c == '.' && p->at < p->length
            && mulib_mune_digit_ ((unsigned char)p->source[p->at])))
      {
        char *end;
        int saved_errno = errno;
        while (p->at < p->length
               && mulib_mune_digit_ ((unsigned char)p->source[p->at]))
          ++p->at;
        if (c != '.' && p->at < p->length && p->source[p->at] == '.')
          {
            ++p->at;
            while (p->at < p->length
                   && mulib_mune_digit_ ((unsigned char)p->source[p->at]))
              ++p->at;
          }
        if (p->at < p->length
            && (p->source[p->at] == 'e' || p->source[p->at] == 'E'))
          {
            ++p->at;
            if (p->at < p->length
                && (p->source[p->at] == '+' || p->source[p->at] == '-'))
              ++p->at;
            while (p->at < p->length
                   && mulib_mune_digit_ ((unsigned char)p->source[p->at]))
              ++p->at;
          }
        if (p->at < p->length
            && (mulib_mune_alpha_ ((unsigned char)p->source[p->at])
                || p->source[p->at] == '.'))
          return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                   "invalid numeric token boundary");
        n = p->at - start;
        if (n >= sizeof (p->word))
          return mulib_mune_fail_ (p, MULIB_MUNE_LIMIT,
                                   "numeric literal limit");
        memcpy (p->word, p->source + start, n);
        p->word[n] = 0;
        errno = 0;
        p->number = strtod (p->word, &end);
        {
          int bad = errno || !isfinite (p->number);
          errno = saved_errno;
          if (bad)
            return mulib_mune_fail_ (p, MULIB_MUNE_ARITHMETIC,
                                     "numeric literal out of range");
        }
        if (*end)
          return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                   "invalid numeric literal");
        p->token = 256;
        return 1;
      }
    if (!c || !strchr ("+-*/%()=;", c))
      return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX, "unsupported token");
    p->token = c;
    return 1;
  }
  int
  mulib_mune_get (const mulib_mune_t *m, const char *name, double *out)
  {
    if (!m || !name || !out)
      return 0;
    for (size_t i = 0; i < m->count; ++i)
      if (!strcmp (m->vars[i].name, name))
        {
          *out = m->vars[i].value;
          return 1;
        }
    return 0;
  }
  static int
  mulib_mune_expr_ (mulib_mune_parser_ *p, unsigned minimum, double *out)
  {
    double value = 0;
    if (++p->depth > 64)
      return mulib_mune_fail_ (p, MULIB_MUNE_LIMIT, "expression depth limit");
    if (p->token == '-')
      {
        unsigned sign = p->token;
        if (!mulib_mune_next_ (p) || !mulib_mune_expr_ (p, 30, &value))
          return 0;
        if (sign == '-')
          value = -value;
      }
    else if (p->token == '(')
      {
        if (!mulib_mune_next_ (p) || !mulib_mune_expr_ (p, 0, &value))
          return 0;
        if (p->token != ')')
          return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                   "expected closing parenthesis");
        if (!mulib_mune_next_ (p))
          return 0;
      }
    else if (p->token == 256)
      {
        value = p->number;
        if (!mulib_mune_next_ (p))
          return 0;
      }
    else if (p->token == 257)
      {
        if (!mulib_mune_get (p->env, p->word, &value))
          return mulib_mune_fail_ (p, MULIB_MUNE_NAME, "unknown variable");
        if (!mulib_mune_next_ (p))
          return 0;
      }
    else
      return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                               "expected numeric expression");
    for (;;)
      {
        unsigned op = p->token;
        unsigned prec = (op == '+' || op == '-')                ? 10
                        : (op == '*' || op == '/' || op == '%') ? 20
                                                                : 0;
        double right;
        if (!prec || prec < minimum)
          break;
        if (!mulib_mune_next_ (p) || !mulib_mune_expr_ (p, prec + 1, &right))
          return 0;
        if ((op == '/' || op == '%') && right == 0)
          return mulib_mune_fail_ (p, MULIB_MUNE_ARITHMETIC,
                                   "division by zero");
        switch (op)
          {
          case '+':
            value += right;
            break;
          case '-':
            value -= right;
            break;
          case '*':
            value *= right;
            break;
          case '/':
            value /= right;
            break;
          default:
            value = fmod (value, right);
            if (value && ((value < 0) != (right < 0)))
              value += right;
            break;
          }
        if (!isfinite (value))
          return mulib_mune_fail_ (p, MULIB_MUNE_ARITHMETIC,
                                   "nonfinite arithmetic result");
      }
    --p->depth;
    *out = value;
    return 1;
  }
  mulib_mune_t *
  mulib_mune_create (void)
  {
    return (mulib_mune_t *)calloc (1, sizeof (mulib_mune_t));
  }
  void
  mulib_mune_destroy (mulib_mune_t *m)
  {
    free (m);
  }
  mulib_mune_status_t
  mulib_mune_eval (mulib_mune_t *m, const char *source, size_t n, double *out,
                   mulib_mune_error_t *error)
  {
    mulib_mune_parser_ p;
    mulib_mune_t env;
    double result = 0;
    memset (&p, 0, sizeof (p));
    p.error.message = "ok";
    if (!m || !out || (!source && n))
      {
        mulib_mune_fail_ (&p, MULIB_MUNE_INVALID, "invalid arguments");
        goto done;
      }
    if (n > 65536)
      {
        mulib_mune_fail_ (&p, MULIB_MUNE_LIMIT, "source size limit");
        goto done;
      }
    env = *m;
    p.env = &env;
    p.source = source;
    p.length = n;
    if (!mulib_mune_next_ (&p))
      goto done;
    while (p.token)
      {
        char name[64];
        double value;
        size_t i;
        if (p.token == ';')
          {
            if (!mulib_mune_next_ (&p))
              goto done;
            continue;
          }
        if (p.token == 258)
          {
            if (!mulib_mune_next_ (&p) || !mulib_mune_expr_ (&p, 0, &result))
              goto done;
            if (p.token == ';' && !mulib_mune_next_ (&p))
              goto done;
            if (p.token)
              {
                mulib_mune_fail_ (&p, MULIB_MUNE_SYNTAX,
                                  "return must be final");
                goto done;
              }
            break;
          }
        if (p.token != 257)
          {
            mulib_mune_fail_ (&p, MULIB_MUNE_SYNTAX,
                              "expected assignment or return");
            goto done;
          }
        strcpy (name, p.word);
        if (!mulib_mune_next_ (&p))
          goto done;
        if (p.token != '=')
          {
            mulib_mune_fail_ (&p, MULIB_MUNE_SYNTAX, "expected assignment");
            goto done;
          }
        if (!mulib_mune_next_ (&p) || !mulib_mune_expr_ (&p, 0, &value))
          goto done;
        for (i = 0; i < env.count && strcmp (env.vars[i].name, name); ++i)
          {
          }
        if (i == 128)
          {
            mulib_mune_fail_ (&p, MULIB_MUNE_LIMIT, "variable limit");
            goto done;
          }
        if (i == env.count)
          {
            strcpy (env.vars[i].name, name);
            ++env.count;
          }
        env.vars[i].value = value;
      }
    *m = env;
    *out = result;
  done:
    if (error)
      *error = p.error;
    return p.error.code;
  }
  /* The lexer produces one byte per token. PARSEC's owned PEG frontend then
   * recognizes the complete grammar and constructs transactional captures.
   * Captured token offsets are translated back to original source spans. */
  typedef struct mulib_mune_span_
  {
    size_t offset, length;
  } mulib_mune_span_;
  typedef struct mulib_mune_plan_ mulib_mune_plan_;
  typedef struct mulib_mune_capture_name_
  {
    struct mulib_mune_capture_name_ *next;
    const mulib_ast_t *name;
  } mulib_mune_capture_name_;
  typedef struct mulib_mune_prototype_
  {
    struct mulib_mune_prototype_ *next, *parent;
    const mulib_ast_t *body;
    size_t id;
    mulib_mune_capture_name_ *captures, *tail;
  } mulib_mune_prototype_;
  static void mulib_mune_plans_free_ (mulib_mune_plan_ *);
  struct mulib_mune_chunk
  {
    mulib_ast_walker_t *walker;
    size_t references;
    mulib_mune_plan_ *plans;
    const mulib_mune_aot_entry_t *aot;
    size_t aot_count;
    mulib_mune_prototype_ *prototypes;
    char *source_name;
  };
  typedef struct mulib_mune_frontend_
  {
    const char *source;
    size_t length, at, count;
    char *tokens;
    mulib_mune_span_ *spans;
    mulib_mune_parse_options_t limits;
    mulib_mune_error_t error;
  } mulib_mune_frontend_;
  static int
  mulib_mune_frontend_fail_ (mulib_mune_frontend_ *p, mulib_mune_status_t s,
                            size_t at, const char *message)
  {
    if (!p->error.code)
      { p->error.code = s; p->error.offset = at; p->error.message = message; }
    return 0;
  }
  static int
  mulib_mune_space_ (unsigned char c)
  {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
  }
  static int
  mulib_mune_hex_ (unsigned char c)
  {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
  static int
  mulib_mune_long_ (mulib_mune_frontend_ *p, size_t at, size_t *after)
  {
    size_t eq, end, k;
    if (at >= p->length || p->source[at] != '[') return 0;
    end = at + 1;
    while (end < p->length && p->source[end] == '=') ++end;
    if (end == p->length || p->source[end] != '[') return 0;
    eq = end - at - 1;
    for (++end; end < p->length; ++end)
      if (p->source[end] == ']')
        {
          k = end + 1;
          while (k < p->length && p->source[k] == '=') ++k;
          if (k < p->length && p->source[k] == ']' && k - end - 1 == eq)
            { *after = k + 1; return 1; }
        }
    mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, at, "unfinished long string or comment");
    return -1;
  }
  static int
  mulib_mune_quoted_ (mulib_mune_frontend_ *p)
  {
    size_t start = p->at;
    unsigned char quote = (unsigned char)p->source[p->at++];
    while (p->at < p->length)
      {
        unsigned char c = (unsigned char)p->source[p->at++];
        if (c == quote) return 1;
        if (c == '\r' || c == '\n') break;
        if (c != '\\') continue;
        if (p->at == p->length) break;
        c = (unsigned char)p->source[p->at++];
        if (c == '\r' || c == '\n')
          {
            if (p->at < p->length && (p->source[p->at] == '\r' || p->source[p->at] == '\n')
                && p->source[p->at] != (char)c) ++p->at;
          }
        else if (c == 'z')
          {
            while (p->at < p->length && mulib_mune_space_ ((unsigned char)p->source[p->at])) ++p->at;
          }
        else if (c == 'x')
          {
            size_t j;
            for (j = 0; j < 2; ++j)
              if (p->at == p->length || mulib_mune_hex_ ((unsigned char)p->source[p->at++]) < 0)
                return mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, p->at, "expected two hexadecimal escape digits");
          }
        else if (c == 'u')
          {
            uint32_t value = 0;
            size_t digits = 0;
            if (p->at == p->length || p->source[p->at++] != '{')
              return mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, p->at, "expected Unicode escape opening brace");
            while (p->at < p->length && p->source[p->at] != '}')
              {
                int digit = mulib_mune_hex_ ((unsigned char)p->source[p->at++]);
                if (digit < 0 || value > (UINT32_C (0x7fffffff) - (unsigned)digit) / 16)
                  return mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, p->at - 1, "invalid Unicode escape");
                value = value * 16 + (unsigned)digit; ++digits;
              }
            if (!digits || p->at == p->length)
              return mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, p->at, "unfinished Unicode escape");
            ++p->at;
          }
        else if (mulib_mune_digit_ (c))
          {
            unsigned value = c - '0', j;
            for (j = 1; j < 3 && p->at < p->length
                        && mulib_mune_digit_ ((unsigned char)p->source[p->at]); ++j)
              value = value * 10 + (unsigned)(p->source[p->at++] - '0');
            if (value > 255)
              return mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, p->at - 1, "decimal escape exceeds one byte");
          }
        else if (!c || !strchr ("abfnrtv\\\"'", c))
          return mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, p->at - 1, "invalid string escape");
      }
    return mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, start, "unfinished quoted string");
  }
  static int
  mulib_mune_numeral_ (mulib_mune_frontend_ *p)
  {
    size_t start = p->at, digits = 0;
    int hex = p->length - p->at >= 2 && p->source[p->at] == '0'
              && (p->source[p->at + 1] == 'x' || p->source[p->at + 1] == 'X');
    if (hex) p->at += 2;
    while (p->at < p->length
           && (hex ? mulib_mune_hex_ ((unsigned char)p->source[p->at]) >= 0
                   : mulib_mune_digit_ ((unsigned char)p->source[p->at])))
      { ++p->at; ++digits; }
    if (p->at < p->length && p->source[p->at] == '.')
      {
        ++p->at;
        while (p->at < p->length
               && (hex ? mulib_mune_hex_ ((unsigned char)p->source[p->at]) >= 0
                       : mulib_mune_digit_ ((unsigned char)p->source[p->at])))
          { ++p->at; ++digits; }
      }
    if (!digits) goto bad;
    if (p->at < p->length
        && (p->source[p->at] == (hex ? 'p' : 'e') || p->source[p->at] == (hex ? 'P' : 'E')))
      {
        size_t exp;
        ++p->at;
        if (p->at < p->length && (p->source[p->at] == '+' || p->source[p->at] == '-')) ++p->at;
        exp = p->at;
        while (p->at < p->length && mulib_mune_digit_ ((unsigned char)p->source[p->at])) ++p->at;
        if (p->at == exp) goto bad;
      }
    if (p->at < p->length && (mulib_mune_alpha_ ((unsigned char)p->source[p->at])
                             || p->source[p->at] == '.')) goto bad;
    return 1;
  bad:
    return mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, start, "malformed numeral");
  }
  static int
  mulib_mune_lex_lua_ (mulib_mune_frontend_ *p)
  {
    static const char *const keywords[] = {
      "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
      "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then",
      "true", "until", "while"
    };
    static const char *const operators[] = { "==", "~=", "<=", ">=", "<<", ">>", "//", "..", "...", "::" };
    while (p->at < p->length)
      {
        size_t start, end, j;
        unsigned char c, token;
        int matched;
        c = (unsigned char)p->source[p->at];
        if (mulib_mune_space_ (c)) { ++p->at; continue; }
        if (c == '-' && p->length - p->at >= 2 && p->source[p->at + 1] == '-')
          {
            p->at += 2;
            matched = mulib_mune_long_ (p, p->at, &end);
            if (matched < 0) return 0;
            if (matched) p->at = end;
            else while (p->at < p->length && p->source[p->at] != '\r' && p->source[p->at] != '\n') ++p->at;
            continue;
          }
        start = p->at; token = c;
        if (mulib_mune_alpha_ (c))
          {
            ++p->at;
            while (p->at < p->length && (mulib_mune_alpha_ ((unsigned char)p->source[p->at])
                                         || mulib_mune_digit_ ((unsigned char)p->source[p->at]))) ++p->at;
            token = 0xc0;
            for (j = 0; j < sizeof (keywords) / sizeof (*keywords); ++j)
              if (strlen (keywords[j]) == p->at - start && !memcmp (keywords[j], p->source + start, p->at - start))
                { token = (unsigned char)(0x80 + j); break; }
          }
        else if (mulib_mune_digit_ (c)
                 || (c == '.' && p->length - p->at >= 2 && mulib_mune_digit_ ((unsigned char)p->source[p->at + 1])))
          { if (!mulib_mune_numeral_ (p)) return 0; token = 0xc1; }
        else if (c == '\'' || c == '"')
          { if (!mulib_mune_quoted_ (p)) return 0; token = 0xc2; }
        else if (c == '[' && (matched = mulib_mune_long_ (p, p->at, &end)) != 0)
          { if (matched < 0) return 0; p->at = end; token = 0xc2; }
        else
          {
            ++p->at;
            if (!c || !strchr ("+-*/%^#&~|<>=(){}[];:,.", c))
              return mulib_mune_frontend_fail_ (p, MULIB_MUNE_SYNTAX, start, "unexpected source byte");
            for (j = sizeof (operators) / sizeof (*operators); j-- > 0;)
              {
                size_t n = strlen (operators[j]);
                if (n <= p->length - start && !memcmp (p->source + start, operators[j], n))
                  { p->at = start + n; token = (unsigned char)(0xa0 + j); break; }
              }
          }
        if (p->count == p->limits.tokens)
          return mulib_mune_frontend_fail_ (p, MULIB_MUNE_LIMIT, start, "token limit");
        p->tokens[p->count] = (char)token;
        p->spans[p->count].offset = start; p->spans[p->count].length = p->at - start; ++p->count;
      }
    p->tokens[p->count] = 0;
    return 1;
  }
  static const char mulib_mune_lua_grammar_[] =
    "chunk <- block !.;\n"
    "block <- statement* return_statement?;\n"
    "statement <- empty_statement / label / break_statement / goto_statement / do_statement / while_statement / repeat_statement / if_statement / numeric_for / generic_for / local_function / function_statement / local_statement / assignment / call_statement;\n"
    "empty_statement <- ';'; label <- '\xa9' name '\xa9';\n"
    "break_statement <- '\x81'; goto_statement <- '\x89' name;\n"
    "do_statement <- '\x82' block '\x85';\n"
    "while_statement <- '\x95' expression '\x82' block '\x85';\n"
    "repeat_statement <- '\x90' block '\x94' expression;\n"
    "if_statement <- '\x8a' expression '\x92' block elseif_clause* else_clause? '\x85';\n"
    "elseif_clause <- '\x84' expression '\x92' block; else_clause <- '\x83' block;\n"
    "numeric_for <- '\x87' name '=' expression ',' expression (',' expression)? '\x82' block '\x85';\n"
    "generic_for <- '\x87' names '\x8b' expressions '\x82' block '\x85';\n"
    "function_statement <- '\x88' function_name function_body;\n"
    "function_name <- name ('.' name)* method_name?; method_name <- ':' name;\n"
    "local_function <- '\x8c' '\x88' name function_body;\n"
    "local_statement <- '\x8c' local_names ('=' expressions)?;\n"
    "local_names <- local_name (',' local_name)*; local_name <- name attribute?; attribute <- '<' name '>';\n"
    "assignment <- variables '=' expressions; variables <- prefix (',' prefix)*;\n"
    "call_statement <- prefix; return_statement <- '\x91' expressions? ';'?;\n"
    "function_body <- '(' parameters? ')' block '\x85';\n"
    "parameters <- names (',' vararg)? / vararg; names <- name (',' name)*;\n"
    "expressions <- expression (',' expression)*; expression <- logical_or;\n"
    "logical_or <- logical_and ('\x8f' logical_and)*; logical_and <- comparison ('\x80' comparison)*;\n"
    "comparison <- bit_or (compare_op bit_or)*; compare_op <- '\xa0' / '\xa1' / '\xa2' / '\xa3' / '<' / '>';\n"
    "bit_or <- bit_xor ('|' bit_xor)*; bit_xor <- bit_and ('~' bit_and)*;\n"
    "bit_and <- shift ('&' shift)*; shift <- concatenate (shift_op concatenate)*; shift_op <- '\xa4' / '\xa5';\n"
    "concatenate <- addition ('\xa7' concatenate)?; addition <- product (add_op product)*; add_op <- '+' / '-';\n"
    "product <- unary (multiply_op unary)*; multiply_op <- '*' / '/' / '\xa6' / '%';\n"
    "unary <- unary_op unary / power; unary_op <- '\x8e' / '#' / '-' / '~';\n"
    "power <- primary ('^' unary)?;\n"
    "primary <- nil / false / true / number / string / vararg / function_literal / table / prefix;\n"
    "nil <- '\x8d'; false <- '\x86'; true <- '\x93'; number <- '\xc1'; string <- '\xc2'; vararg <- '\xa8'; name <- '\xc0';\n"
    "function_literal <- '\x88' function_body;\n"
    "prefix <- (name / parenthesized) suffix*; parenthesized <- '(' expression ')';\n"
    "suffix <- index / field_name / call; index <- '[' expression ']'; field_name <- '.' name;\n"
    "call <- method_name? arguments; arguments <- '(' expressions? ')' / table / string;\n"
    "table <- '{' fields? '}'; fields <- field (field_separator field)* field_separator?; field_separator <- ',' / ';';\n"
    "field <- indexed_field / named_field / expression; indexed_field <- '[' expression ']' '=' expression; named_field <- name '=' expression;\n";
  static int
  mulib_mune_ast_source_ (mulib_mune_frontend_ *p, mulib_ast_t *n, size_t *count)
  {
    size_t begin = n->offset < p->count ? p->spans[n->offset].offset : p->length;
    size_t end = begin;
    char *text;
    mulib_ast_t *child;
    if (++*count > p->limits.nodes)
      return mulib_mune_frontend_fail_ (p, MULIB_MUNE_LIMIT, begin, "AST node limit");
    if (n->length)
      {
        size_t last = n->offset + n->length - 1;
        if (last >= p->count) return mulib_mune_frontend_fail_ (p, MULIB_MUNE_INVALID, begin, "invalid parser span");
        end = p->spans[last].offset + p->spans[last].length;
      }
    if (!strcmp (n->production, "chunk")) { begin = 0; end = p->length; }
    text = (char *)malloc (end - begin + 1);
    if (!text) return mulib_mune_frontend_fail_ (p, MULIB_MUNE_MEMORY, begin, "AST source allocation failed");
    if (end > begin) memcpy (text, p->source + begin, end - begin);
    text[end - begin] = 0;
    free (n->text); n->text = text; n->length = end - begin; n->offset = begin;
    for (child = n->child; child; child = child->next)
      if (!mulib_mune_ast_source_ (p, child, count)) return 0;
    return 1;
  }
  const mulib_ast_t *
  mulib_mune_chunk_ast (const mulib_mune_chunk_t *c)
  {
    return c && c->walker ? c->walker->root : NULL;
  }
  void
  mulib_mune_chunk_destroy (mulib_mune_chunk_t *c)
  {
    if (c && c->references > 1) { --c->references; return; }
    if (c)
      {
        mulib_mune_prototype_ *p;
        while ((p = c->prototypes) != NULL)
          {
            mulib_mune_capture_name_ *n;
            c->prototypes = p->next;
            while ((n = p->captures) != NULL) { p->captures = n->next; free (n); }
            free (p);
          }
        mulib_mune_plans_free_ (c->plans); mulib_ast_walker_destroy (c->walker); free (c->source_name); free (c);
      }
  }
  typedef struct mulib_mune_binding_
  {
    const mulib_ast_t *name;
    size_t previous, function;
    int readonly;
  } mulib_mune_binding_;
  typedef struct mulib_mune_scope_
  {
    size_t parent, function, entry;
  } mulib_mune_scope_;
  typedef struct mulib_mune_jump_
  {
    const mulib_ast_t *name;
    size_t scope, active;
    int label;
  } mulib_mune_jump_;
  typedef struct mulib_mune_semantic_
  {
    mulib_mune_frontend_ *frontend;
    mulib_mune_binding_ *bindings;
    mulib_mune_scope_ *scopes;
    mulib_mune_jump_ *jumps;
    size_t binding_count, binding_capacity, scope_count, scope_capacity;
    size_t jump_count, jump_capacity, scope, active, function, functions, work;
    unsigned loops;
    int vararg;
    mulib_mune_chunk_t *chunk;
    mulib_mune_prototype_ *prototype;
    size_t capture_count;
  } mulib_mune_semantic_;
  static int
  mulib_mune_is_ (const mulib_ast_t *n, const char *name)
  {
    return n && !strcmp (n->production, name);
  }
  static const mulib_ast_t *
  mulib_mune_child_ (const mulib_ast_t *n, const char *name)
  {
    const mulib_ast_t *c;
    for (c = n ? n->child : NULL; c; c = c->next)
      if (mulib_mune_is_ (c, name)) return c;
    return NULL;
  }
  static int
  mulib_mune_same_name_ (const mulib_ast_t *a, const mulib_ast_t *b)
  {
    return a->length == b->length && !memcmp (a->text, b->text, a->length);
  }
  static int
  mulib_mune_sem_fail_ (mulib_mune_semantic_ *s, const mulib_ast_t *n, const char *message)
  {
    return mulib_mune_frontend_fail_ (s->frontend, MULIB_MUNE_SEMANTIC, n->offset, message);
  }
  static int
  mulib_mune_sem_work_ (mulib_mune_semantic_ *s, const mulib_ast_t *n)
  {
    if (s->work == s->frontend->limits.steps)
      return mulib_mune_frontend_fail_ (s->frontend, MULIB_MUNE_LIMIT, n->offset, "semantic work limit");
    ++s->work; return 1;
  }
  static void *
  mulib_mune_sem_grow_ (mulib_mune_semantic_ *s, void *old, size_t count,
                        size_t *capacity, size_t stride, const mulib_ast_t *n)
  {
    size_t limit = s->frontend->limits.symbols, cap;
    void *next;
    if (count < *capacity) return old;
    if (count == limit || count == SIZE_MAX || count + 1 > SIZE_MAX / stride)
      {
        mulib_mune_frontend_fail_ (s->frontend, MULIB_MUNE_LIMIT, n->offset, "semantic record limit");
        return NULL;
      }
    cap = *capacity < 16 ? 16 : *capacity > SIZE_MAX / 2 ? SIZE_MAX : *capacity * 2;
    if (cap > limit) cap = limit;
    if (cap > SIZE_MAX / stride) cap = SIZE_MAX / stride;
    next = realloc (old, cap * stride);
    if (!next)
      { mulib_mune_frontend_fail_ (s->frontend, MULIB_MUNE_MEMORY, n->offset, "semantic allocation failed"); return NULL; }
    *capacity = cap; return next;
  }
  static int
  mulib_mune_bind_ (mulib_mune_semantic_ *s, const mulib_ast_t *n, int readonly)
  {
    mulib_mune_binding_ *p = (mulib_mune_binding_ *)mulib_mune_sem_grow_ (
        s, s->bindings, s->binding_count, &s->binding_capacity, sizeof (*p), n);
    if (!p) return 0;
    s->bindings = p; p[s->binding_count].name = n;
    p[s->binding_count].previous = s->active; p[s->binding_count].readonly = readonly;
    p[s->binding_count].function = s->function;
    s->active = s->binding_count++;
    return 1;
  }
  static int
  mulib_mune_enter_scope_ (mulib_mune_semantic_ *s, const mulib_ast_t *n)
  {
    mulib_mune_scope_ *p = (mulib_mune_scope_ *)mulib_mune_sem_grow_ (
        s, s->scopes, s->scope_count, &s->scope_capacity, sizeof (*p), n);
    if (!p) return 0;
    s->scopes = p; p[s->scope_count].parent = s->scope;
    p[s->scope_count].entry = s->active; p[s->scope_count].function = s->function;
    s->scope = s->scope_count++;
    return 1;
  }
  static void
  mulib_mune_leave_scope_ (mulib_mune_semantic_ *s)
  {
    s->active = s->scopes[s->scope].entry; s->scope = s->scopes[s->scope].parent;
  }
  static int
  mulib_mune_assign_name_ (mulib_mune_semantic_ *s, const mulib_ast_t *name)
  {
    size_t k;
    for (k = s->active; k != SIZE_MAX; k = s->bindings[k].previous)
      {
        if (!mulib_mune_sem_work_ (s, name)) return 0;
        if (mulib_mune_same_name_ (s->bindings[k].name, name))
          return !s->bindings[k].readonly || mulib_mune_sem_fail_ (s, name, "assignment to a const or to-be-closed local");
      }
    return 1;
  }
  static int mulib_mune_sem_walk_ (mulib_mune_semantic_ *, const mulib_ast_t *);
  static int
  mulib_mune_sem_use_ (mulib_mune_semantic_ *s, const mulib_ast_t *name)
  {
    static const mulib_ast_t env = { "name", "_ENV", NULL, 4, 0, NULL, NULL, NULL, NULL };
    size_t k;
    mulib_mune_prototype_ *p;
    for (k = s->active; k != SIZE_MAX; k = s->bindings[k].previous)
      { if (!mulib_mune_sem_work_ (s, name)) return 0; if (mulib_mune_same_name_ (s->bindings[k].name, name)) break; }
    if (k == SIZE_MAX)
      {
        name = &env;
        for (k = s->active; k != SIZE_MAX; k = s->bindings[k].previous)
          if (mulib_mune_same_name_ (s->bindings[k].name, name)) break;
      }
    for (p = s->prototype; p && p->id != (k == SIZE_MAX ? 0 : s->bindings[k].function); p = p->parent)
      {
        mulib_mune_capture_name_ *c;
        for (c = p->captures; c; c = c->next) if (mulib_mune_same_name_ (c->name, name)) break;
        if (c) continue;
        if (s->capture_count++ == s->frontend->limits.symbols)
          return mulib_mune_frontend_fail_ (s->frontend, MULIB_MUNE_LIMIT, name->offset, "capture limit");
        c = (mulib_mune_capture_name_ *)calloc (1, sizeof (*c));
        if (!c) return mulib_mune_frontend_fail_ (s->frontend, MULIB_MUNE_MEMORY, name->offset, "capture allocation failed");
        c->name = name;
        if (p->tail) p->tail->next = c; else p->captures = c;
        p->tail = c;
      }
    return 1;
  }
  static int
  mulib_mune_sem_children_ (mulib_mune_semantic_ *s, const mulib_ast_t *n)
  {
    const mulib_ast_t *c;
    for (c = n->child; c; c = c->next)
      if (!mulib_mune_sem_walk_ (s, c)) return 0;
    return 1;
  }
  static int
  mulib_mune_sem_function_ (mulib_mune_semantic_ *s, const mulib_ast_t *n)
  {
    static const mulib_ast_t self = { "name", "self", NULL, 4, 0, NULL, NULL, NULL, NULL };
    const mulib_ast_t *params = mulib_mune_child_ (n, "parameters"), *c;
    const mulib_ast_t *names = mulib_mune_child_ (params, "names");
    size_t function = s->function;
    unsigned loops = s->loops;
    int vararg = s->vararg, ok;
    mulib_mune_prototype_ *parent = s->prototype, *prototype = (mulib_mune_prototype_ *)calloc (1, sizeof (*prototype));
    if (!prototype) return mulib_mune_frontend_fail_ (s->frontend, MULIB_MUNE_MEMORY, n->offset, "function allocation failed");
    s->function = ++s->functions; s->loops = 0;
    prototype->body = n; prototype->id = s->function; prototype->parent = parent;
    prototype->next = s->chunk->prototypes; s->chunk->prototypes = s->prototype = prototype;
    s->vararg = mulib_mune_child_ (params, "vararg") != NULL;
    if (!mulib_mune_enter_scope_ (s, n)) return 0;
    if (mulib_mune_is_ (n->parent, "function_statement")
        && mulib_mune_child_ (mulib_mune_child_ (n->parent, "function_name"), "method_name")
        && !mulib_mune_bind_ (s, &self, 0)) return 0;
    for (c = names ? names->child : NULL; c; c = c->next)
      if (!mulib_mune_bind_ (s, c, 0)) return 0;
    ok = mulib_mune_sem_walk_ (s, mulib_mune_child_ (n, "block"));
    mulib_mune_leave_scope_ (s);
    s->function = function; s->loops = loops; s->vararg = vararg;
    s->prototype = parent;
    return ok;
  }
  static int
  mulib_mune_sem_jump_ (mulib_mune_semantic_ *s, const mulib_ast_t *n, int label)
  {
    const mulib_ast_t *name = mulib_mune_child_ (n, "name"), *next;
    mulib_mune_jump_ *p;
    size_t k, active = s->active;
    if (label)
      {
        for (k = 0; k < s->jump_count; ++k)
          {
            if (!mulib_mune_sem_work_ (s, n)) return 0;
            if (s->jumps[k].label && mulib_mune_same_name_ (s->jumps[k].name, name))
              {
                size_t scope;
                for (scope = s->scope; scope != SIZE_MAX && s->scopes[scope].function == s->function;
                     scope = s->scopes[scope].parent)
                  if (scope == s->jumps[k].scope)
                    return mulib_mune_sem_fail_ (s, name, "duplicate visible label");
              }
          }
        /* Labels followed only by empty statements/labels are outside the
         * block's local-variable scope (Lua's end-of-block label rule). */
        for (next = n->parent->next; next; next = next->next)
          if (!mulib_mune_is_ (next, "statement")
              || (!mulib_mune_is_ (next->child, "label")
                  && !mulib_mune_is_ (next->child, "empty_statement"))) break;
        if (!next && !mulib_mune_is_ (n->parent->parent->parent, "repeat_statement"))
          active = s->scopes[s->scope].entry;
      }
    p = (mulib_mune_jump_ *)mulib_mune_sem_grow_ (
        s, s->jumps, s->jump_count, &s->jump_capacity, sizeof (*p), n);
    if (!p) return 0;
    s->jumps = p; p[s->jump_count].name = name; p[s->jump_count].scope = s->scope;
    p[s->jump_count].active = active; p[s->jump_count].label = label; ++s->jump_count;
    return 1;
  }
  static int
  mulib_mune_sem_walk_ (mulib_mune_semantic_ *s, const mulib_ast_t *n)
  {
    const mulib_ast_t *c, *names, *block;
    int ok;
    if (!n) return 1;
    if (!mulib_mune_sem_work_ (s, n)) return 0;
    if (mulib_mune_is_ (n, "prefix") && mulib_mune_is_ (n->child, "name") && !mulib_mune_sem_use_ (s, n->child)) return 0;
    if (mulib_mune_is_ (n, "block"))
      {
        if (!mulib_mune_enter_scope_ (s, n)) return 0;
        ok = mulib_mune_sem_children_ (s, n); mulib_mune_leave_scope_ (s); return ok;
      }
    if (mulib_mune_is_ (n, "function_body")) return mulib_mune_sem_function_ (s, n);
    if (mulib_mune_is_ (n, "vararg"))
      return s->vararg || mulib_mune_sem_fail_ (s, n, "vararg expression outside a vararg function");
    if (mulib_mune_is_ (n, "break_statement"))
      return s->loops || mulib_mune_sem_fail_ (s, n, "break outside a loop");
    if (mulib_mune_is_ (n, "label") || mulib_mune_is_ (n, "goto_statement"))
      return mulib_mune_sem_jump_ (s, n, mulib_mune_is_ (n, "label"));
    if (mulib_mune_is_ (n, "local_statement"))
      {
        unsigned close_count = 0;
        if (!mulib_mune_sem_walk_ (s, mulib_mune_child_ (n, "expressions"))) return 0;
        names = mulib_mune_child_ (n, "local_names");
        for (c = names->child; c; c = c->next)
          {
            const mulib_ast_t *attribute = mulib_mune_child_ (c, "attribute");
            const mulib_ast_t *name = mulib_mune_child_ (c, "name");
            int readonly = 0;
            if (attribute)
              {
                const mulib_ast_t *a = mulib_mune_child_ (attribute, "name");
                if (a->length == 5 && !memcmp (a->text, "close", 5))
                  {
                    if (++close_count > 1) return mulib_mune_sem_fail_ (s, a, "multiple to-be-closed variables in one declaration");
                  }
                else if (a->length != 5 || memcmp (a->text, "const", 5))
                  return mulib_mune_sem_fail_ (s, a, "unknown local variable attribute");
                readonly = 1;
              }
            if (!mulib_mune_bind_ (s, name, readonly)) return 0;
          }
        return 1;
      }
    if (mulib_mune_is_ (n, "local_function"))
      {
        if (!mulib_mune_bind_ (s, mulib_mune_child_ (n, "name"), 0)) return 0;
        return mulib_mune_sem_walk_ (s, mulib_mune_child_ (n, "function_body"));
      }
    if (mulib_mune_is_ (n, "function_statement"))
      {
        names = mulib_mune_child_ (n, "function_name");
        if (!mulib_mune_sem_use_ (s, names->child)) return 0;
        if (!names->child->next && !mulib_mune_assign_name_ (s, names->child)) return 0;
        return mulib_mune_sem_walk_ (s, mulib_mune_child_ (n, "function_body"));
      }
    if (mulib_mune_is_ (n, "assignment"))
      {
        const mulib_ast_t *variables = mulib_mune_child_ (n, "variables");
        for (c = variables->child; c; c = c->next)
          {
            const mulib_ast_t *last = c->tail;
            if (last == c->child)
              {
                if (!mulib_mune_is_ (last, "name")) return mulib_mune_sem_fail_ (s, c, "invalid assignment target");
                if (!mulib_mune_assign_name_ (s, last)) return 0;
              }
            else if (!mulib_mune_is_ (last->child, "index") && !mulib_mune_is_ (last->child, "field_name"))
              return mulib_mune_sem_fail_ (s, c, "cannot assign to a function call");
          }
        return mulib_mune_sem_children_ (s, n);
      }
    if (mulib_mune_is_ (n, "call_statement"))
      {
        const mulib_ast_t *prefix = n->child;
        if (!mulib_mune_is_ (prefix->tail, "suffix") || !mulib_mune_is_ (prefix->tail->child, "call"))
          return mulib_mune_sem_fail_ (s, n, "statement must be an assignment or function call");
        return mulib_mune_sem_children_ (s, n);
      }
    if (mulib_mune_is_ (n, "numeric_for") || mulib_mune_is_ (n, "generic_for"))
      {
        for (c = n->child; c; c = c->next)
          if ((mulib_mune_is_ (c, "expression") || mulib_mune_is_ (c, "expressions"))
              && !mulib_mune_sem_walk_ (s, c)) return 0;
        if (!mulib_mune_enter_scope_ (s, n)) return 0;
        if (mulib_mune_is_ (n, "numeric_for"))
          { if (!mulib_mune_bind_ (s, mulib_mune_child_ (n, "name"), 0)) return 0; }
        else
          {
            names = mulib_mune_child_ (n, "names");
            for (c = names->child; c; c = c->next) if (!mulib_mune_bind_ (s, c, 0)) return 0;
          }
        ++s->loops; ok = mulib_mune_sem_walk_ (s, mulib_mune_child_ (n, "block")); --s->loops;
        mulib_mune_leave_scope_ (s); return ok;
      }
    if (mulib_mune_is_ (n, "while_statement"))
      {
        if (!mulib_mune_sem_walk_ (s, mulib_mune_child_ (n, "expression"))) return 0;
        ++s->loops; ok = mulib_mune_sem_walk_ (s, mulib_mune_child_ (n, "block")); --s->loops;
        return ok;
      }
    if (mulib_mune_is_ (n, "repeat_statement"))
      {
        block = mulib_mune_child_ (n, "block");
        if (!mulib_mune_enter_scope_ (s, block)) return 0;
        ++s->loops; ok = mulib_mune_sem_children_ (s, block); --s->loops;
        if (ok) ok = mulib_mune_sem_walk_ (s, mulib_mune_child_ (n, "expression"));
        mulib_mune_leave_scope_ (s); return ok;
      }
    return mulib_mune_sem_children_ (s, n);
  }
  static int
  mulib_mune_sem_resolve_ (mulib_mune_semantic_ *s)
  {
    size_t i, j, scope, binding, active;
    for (i = 0; i < s->jump_count; ++i)
      if (!s->jumps[i].label)
        {
          const mulib_mune_jump_ *g = &s->jumps[i], *label = NULL;
          size_t function = s->scopes[g->scope].function;
          for (scope = g->scope; scope != SIZE_MAX && s->scopes[scope].function == function;
               scope = s->scopes[scope].parent)
            {
              for (j = 0; j < s->jump_count; ++j)
                {
                  if (!mulib_mune_sem_work_ (s, g->name)) return 0;
                  if (s->jumps[j].label && s->jumps[j].scope == scope
                      && mulib_mune_same_name_ (s->jumps[j].name, g->name))
                    { label = &s->jumps[j]; break; }
                }
              if (label) break;
            }
          if (!label) return mulib_mune_sem_fail_ (s, g->name, "no visible label for goto");
          for (binding = label->active; binding != SIZE_MAX; binding = s->bindings[binding].previous)
            {
              for (active = g->active; active != SIZE_MAX && active != binding; active = s->bindings[active].previous)
                if (!mulib_mune_sem_work_ (s, g->name)) return 0;
              if (active == SIZE_MAX) return mulib_mune_sem_fail_ (s, g->name, "goto jumps into the scope of a local");
            }
        }
    return 1;
  }
  static int
  mulib_mune_analyze_ (mulib_mune_frontend_ *p, mulib_mune_chunk_t *chunk)
  {
    mulib_mune_semantic_ s;
    int ok;
    memset (&s, 0, sizeof (s)); s.frontend = p; s.scope = s.active = SIZE_MAX; s.vararg = 1;
    s.chunk = chunk;
    ok = mulib_mune_sem_walk_ (&s, chunk->walker->root) && mulib_mune_sem_resolve_ (&s);
    free (s.bindings); free (s.scopes); free (s.jumps);
    return ok;
  }
  static mulib_mune_parse_options_t
  mulib_mune_parse_limits_ (const mulib_mune_parse_options_t *requested)
  {
    mulib_mune_parse_options_t o;
    memset (&o, 0, sizeof (o)); if (requested) o = *requested;
    if (!o.source_bytes) o.source_bytes = 1024 * 1024;
    if (!o.tokens) o.tokens = 100000;
    if (!o.nodes) o.nodes = 100000;
    if (!o.symbols) o.symbols = 100000;
    if (!o.steps) o.steps = 10000000;
    return o;
  }
  mulib_mune_chunk_t *
  mulib_mune_parse (const char *source, size_t length,
                    const mulib_mune_parse_options_t *options, mulib_mune_error_t *error)
  {
    mulib_mune_frontend_ p;
    mulib_mune_chunk_t *chunk = NULL;
    mulib_parser_t *parser = NULL;
    mulib_parser_error_t pe;
    mulib_parsec_options_t po;
    size_t count = 0, capacity;
    memset (&p, 0, sizeof (p)); p.error.message = "ok";
    p.limits = mulib_mune_parse_limits_ (options); p.source = source; p.length = length;
    if ((!source && length) || length == SIZE_MAX)
      { mulib_mune_frontend_fail_ (&p, MULIB_MUNE_INVALID, 0, "invalid source span"); goto done; }
    if (length > p.limits.source_bytes)
      { mulib_mune_frontend_fail_ (&p, MULIB_MUNE_LIMIT, 0, "source byte limit"); goto done; }
    capacity = length < p.limits.tokens ? length : p.limits.tokens;
    if (capacity == SIZE_MAX || capacity > SIZE_MAX / sizeof (*p.spans))
      { mulib_mune_frontend_fail_ (&p, MULIB_MUNE_LIMIT, 0, "token allocation size limit"); goto done; }
    p.tokens = (char *)malloc (capacity + 1);
    if (capacity) p.spans = (mulib_mune_span_ *)malloc (capacity * sizeof (*p.spans));
    if (!p.tokens || (capacity && !p.spans))
      { mulib_mune_frontend_fail_ (&p, MULIB_MUNE_MEMORY, 0, "token allocation failed"); goto done; }
    if (!mulib_mune_lex_lua_ (&p)) goto done;
    parser = mulib_parser_from_peg_ex (mulib_mune_lua_grammar_, &pe);
    if (!parser)
      { mulib_mune_frontend_fail_ (&p, pe.code == MULIB_PARSER_MEMORY ? MULIB_MUNE_MEMORY : MULIB_MUNE_INVALID, 0, "Lua grammar construction failed"); goto done; }
    chunk = (mulib_mune_chunk_t *)calloc (1, sizeof (*chunk));
    if (!chunk)
      { mulib_mune_frontend_fail_ (&p, MULIB_MUNE_MEMORY, 0, "chunk allocation failed"); goto done; }
    chunk->references = 1;
    po.max_depth = 1024; po.max_steps = p.limits.steps; po.max_captures = p.limits.nodes;
    chunk->walker = mulib_parser_parse (parser, "chunk", p.tokens, p.count, NULL, 0, &po, &pe);
    if (!chunk->walker)
      {
        mulib_mune_status_t status = pe.code == MULIB_PARSER_MEMORY ? MULIB_MUNE_MEMORY
                                     : pe.code == MULIB_PARSER_LIMIT ? MULIB_MUNE_LIMIT
                                     : pe.code == MULIB_PARSER_INVALID ? MULIB_MUNE_INVALID : MULIB_MUNE_SYNTAX;
        mulib_mune_frontend_fail_ (&p, status, pe.offset < p.count ? p.spans[pe.offset].offset : length,
                                   pe.message ? pe.message : "Lua syntax error"); goto done;
      }
    if (!mulib_mune_ast_source_ (&p, chunk->walker->root, &count)
        || !mulib_mune_analyze_ (&p, chunk)) goto done;
  done:
    mulib_parser_destroy (parser); free (p.tokens); free (p.spans);
    if (p.error.code) { mulib_mune_chunk_destroy (chunk); chunk = NULL; }
    if (error) *error = p.error;
    return chunk;
  }
  mulib_mune_chunk_t *
  mulib_mune_parse_file (const char *path, const mulib_mune_parse_options_t *options,
                         mulib_mune_error_t *error)
  {
    mulib_mune_parse_options_t limits = mulib_mune_parse_limits_ (options);
    mulib_mune_error_t e = { MULIB_MUNE_OK, 0, "ok" };
    mulib_mune_chunk_t *chunk = NULL;
    FILE *file = NULL;
    char *text = NULL;
    size_t size = 0, capacity = 0, start = 0;
    int c;
    if (!path) { e.code = MULIB_MUNE_INVALID; e.message = "invalid file path"; goto done; }
    file = fopen (path, "rb");
    if (!file) { e.code = MULIB_MUNE_IO; e.message = "could not open Lua file"; goto done; }
    while ((c = fgetc (file)) != EOF)
      {
        if (size == limits.source_bytes) { e.code = MULIB_MUNE_LIMIT; e.message = "source byte limit"; goto done; }
        if (size == capacity)
          {
            size_t next = capacity ? capacity > SIZE_MAX / 2 ? SIZE_MAX : capacity * 2 : 4096;
            char *copy;
            if (next > limits.source_bytes) next = limits.source_bytes;
            copy = (char *)realloc (text, next);
            if (!copy) { e.code = MULIB_MUNE_MEMORY; e.message = "file allocation failed"; goto done; }
            text = copy; capacity = next;
          }
        text[size++] = (char)c;
      }
    if (ferror (file)) { e.code = MULIB_MUNE_IO; e.message = "could not read Lua file"; goto done; }
    if (fclose (file)) { file = NULL; e.code = MULIB_MUNE_IO; e.message = "could not close Lua file"; goto done; }
    file = NULL;
    /* Replace the loadfile prefix with spaces to preserve byte offsets. */
    if (size >= 3 && !memcmp (text, "\xef\xbb\xbf", 3)) { memset (text, ' ', 3); start = 3; }
    if (start < size && text[start] == '#')
      while (start < size && text[start] != '\n') text[start++] = ' ';
    chunk = mulib_mune_parse (text, size, &limits, &e);
    if (chunk)
      {
        size_t n = strlen (path);
        chunk->source_name = (char *)malloc (n + 2);
        if (!chunk->source_name) { e.code = MULIB_MUNE_MEMORY; e.message = "source name allocation failed"; mulib_mune_chunk_destroy (chunk); chunk = NULL; }
        else { chunk->source_name[0] = '@'; memcpy (chunk->source_name + 1, path, n + 1); }
      }
  done:
    if (file) (void)fclose (file);
    free (text); if (error) *error = e; return chunk;
  }
  typedef struct mulib_mune_entry_
  {
    mulib_mune_value_t key, value;
    unsigned used;
  } mulib_mune_entry_;
  typedef struct mulib_mune_coroutine_ mulib_mune_coroutine_;
  typedef struct mulib_mune_protected_
  {
    struct mulib_mune_protected_ *previous;
    mulib_mune_value_t handler;
    int handled;
  } mulib_mune_protected_;
  typedef struct mulib_mune_library_
  {
    struct mulib_mune_library_ *next;
    mulib_dynload_t handle;
  } mulib_mune_library_;
  struct mulib_mune_object
  {
    mulib_mune_state_t *owner;
    mulib_mune_object_t *next, *gray, *metatable, *finalize_next;
    size_t bytes;
    unsigned kind, marked, finalizable, finalized, userdata_tag, age;
    union
    {
      size_t length;
      struct { mulib_mune_entry_ *entries; size_t capacity, count, tombs; } table;
      struct
      {
        mulib_mune_native_fn native;
        void *user;
        mulib_mune_chunk_t *chunk;
        const mulib_ast_t *body;
        mulib_mune_object_t *environment;
        mulib_mune_value_t context[3];
        size_t cursor, last_match;
        int method;
      } function;
      struct
      {
        mulib_mune_object_t *parent, *cell;
        mulib_mune_value_t value;
        size_t length, offset;
        int close;
      } environment;
      struct { FILE *stream; int owned, closed; } file;
      struct { size_t length; mulib_mune_object_t *values; uintptr_t identity; } userdata;
      mulib_mune_coroutine_ *thread;
    } as;
  };
  struct mulib_mune_result
  {
    mulib_mune_state_t *owner;
    mulib_mune_result_t *previous, *next;
    mulib_mune_value_t *values;
    size_t count, capacity;
    mulib_mune_object_t *thread;
  };
  typedef struct mulib_mune_frame_
  {
    struct mulib_mune_frame_ *previous;
    mulib_mune_object_t *environment;
    mulib_mune_value_t function;
    mulib_mune_result_t *varargs;
    mulib_mune_chunk_t *chunk;
    size_t location;
    int tail_call;
  } mulib_mune_frame_;
  struct mulib_mune_coroutine_
  {
#ifdef MULIB_MUNE_THREADS_
    thrd_t worker;
    mtx_t mutex;
    cnd_t condition;
#endif
    mulib_mune_object_t *object, *parent;
    mulib_mune_value_t function, error;
    mulib_mune_frame_ *frame;
    mulib_mune_protected_ *protected_call;
    mulib_mune_result_t *incoming, *outgoing;
    size_t depth;
    int main, initialized, started, request, response, state, close_requested, discard, closing_callbacks;
    mulib_mune_status_t status;
    mulib_mune_value_t hook;
    size_t hook_count, hook_remaining, hook_line;
    unsigned hook_mask;
    int in_hook;
  };
  struct mulib_mune_state
  {
    mulib_mune_runtime_options_t limits;
    mulib_mune_object_t *objects, *globals, *environment, *type_metatables[9];
    mulib_mune_object_t *main_thread, *current_thread;
    mulib_mune_object_t *file_metatable, *registry, *finalizers;
    mulib_mune_library_ *libraries;
    mulib_mune_result_t *results;
    mulib_mune_frame_ *frame;
    mulib_mune_protected_ *protected_call;
    mulib_mune_value_t error_value;
    mulib_mune_value_t input, output;
    size_t bytes, threshold, steps, depth, location;
    mulib_mune_object_t **gc_sweep;
    size_t gc_major_bytes;
    uint64_t random_state;
    mulib_mune_status_t status;
    char message[512];
    int running, collecting, gc_stopped, has_error, warnings, gc_pause, gc_stepmul, gc_generation;
    int gc_stepsize, gc_minor, gc_major, gc_minor_cycles, gc_minor_cycle;
    mulib_mune_mode_t mode;
    const mulib_mune_aot_entry_t *aot;
    size_t aot_count;
  };
  enum { MULIB_MUNE_ENVIRONMENT_ = 100 };
  enum { MULIB_MUNE_CO_NEW_, MULIB_MUNE_CO_RUNNING_, MULIB_MUNE_CO_SUSPENDED_, MULIB_MUNE_CO_DEAD_, MULIB_MUNE_CO_FAILED_ };
  static mulib_mune_value_t
  mulib_mune_rt_nil_ (void)
  {
    mulib_mune_value_t v; memset (&v, 0, sizeof (v)); return v;
  }
  static mulib_mune_value_t
  mulib_mune_rt_integer_ (int64_t n)
  {
    mulib_mune_value_t v = mulib_mune_rt_nil_ (); v.type = MULIB_MUNE_INTEGER; v.as.integer = n; return v;
  }
  static mulib_mune_value_t
  mulib_mune_rt_number_ (double n)
  {
    mulib_mune_value_t v = mulib_mune_rt_nil_ (); v.type = MULIB_MUNE_NUMBER; v.as.number = n; return v;
  }
  static mulib_mune_value_t
  mulib_mune_rt_boolean_ (int n)
  {
    mulib_mune_value_t v = mulib_mune_rt_nil_ (); v.type = MULIB_MUNE_BOOLEAN; v.as.boolean = !!n; return v;
  }
  static mulib_mune_value_t
  mulib_mune_rt_object_ (mulib_mune_object_t *object)
  {
    mulib_mune_value_t v = mulib_mune_rt_nil_ ();
    if (object) { v.type = (mulib_mune_type_t)object->kind; v.as.object = object; }
    return v;
  }
  static int
  mulib_mune_rt_truth_ (mulib_mune_value_t v)
  {
    return v.type != MULIB_MUNE_NIL && (v.type != MULIB_MUNE_BOOLEAN || v.as.boolean);
  }
  static size_t mulib_mune_rt_line_ (const mulib_mune_chunk_t *, size_t);
  static void
  mulib_mune_rt_short_source_ (const mulib_mune_chunk_t *chunk, char text[80])
  {
    const char *source = chunk->source_name ? chunk->source_name : chunk->walker->root->text;
    size_t length = strlen (source);
    if (*source == '=' || *source == '@')
      {
        if (*source == '@' && length > 60) (void)snprintf (text, 80, "...%s", source + length - 56);
        else (void)snprintf (text, 80, "%.59s", source + 1);
      }
    else
      {
        size_t end = strcspn (source, "\r\n");
        if (end > 45) end = 45;
        (void)snprintf (text, 80, "[string \"%.*s%s\"]", (int)end, source, end < length ? "..." : "");
      }
  }
  static int
  mulib_mune_rt_error_ (mulib_mune_state_t *s, mulib_mune_status_t status, const char *format, ...)
  {
    if (s && !s->status)
      {
        va_list args;
        s->status = status;
        s->has_error = 0; s->error_value = mulib_mune_rt_nil_ ();
        va_start (args, format); (void)vsnprintf (s->message, sizeof (s->message), format, args); va_end (args);
        if (strcmp (format, "coroutine closed"))
          {
            mulib_mune_frame_ *f;
            for (f = s->frame; f && !f->chunk; f = f->previous) {}
            if (f)
              {
                char source[80], message[sizeof (s->message)];
                mulib_mune_rt_short_source_ (f->chunk, source); memcpy (message, s->message, sizeof (message));
                (void)snprintf (s->message, sizeof (s->message), "%s:%zu: %.400s", source, mulib_mune_rt_line_ (f->chunk, f->location), message);
              }
          }
      }
    return 0;
  }
  static void *
  mulib_mune_rt_memory_ (mulib_mune_state_t *s, void *old, size_t before, size_t after)
  {
    void *p;
    if (!after) { free (old); s->bytes -= before; return NULL; }
    if (after > s->limits.memory_bytes || s->bytes - before > s->limits.memory_bytes - after)
      { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "Lua memory limit"); return NULL; }
    p = realloc (old, after);
    if (!p) { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "Lua allocation failed"); return NULL; }
    s->bytes = s->bytes - before + after; return p;
  }
  static mulib_mune_object_t *
  mulib_mune_rt_allocate_ (mulib_mune_state_t *s, unsigned kind, size_t extra)
  {
    mulib_mune_object_t *o;
    if (extra > SIZE_MAX - sizeof (*o))
      { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "Lua object size overflow"); return NULL; }
    o = (mulib_mune_object_t *)mulib_mune_rt_memory_ (s, NULL, 0, sizeof (*o) + extra);
    if (!o) return NULL;
    memset (o, 0, sizeof (*o)); o->owner = s; o->kind = kind; o->bytes = sizeof (*o) + extra;
    o->marked = s->collecting != 0 || s->gc_sweep != NULL;
    o->next = s->objects; s->objects = o; return o;
  }
  static int
  mulib_mune_rt_valid_ (mulib_mune_state_t *s, mulib_mune_value_t v)
  {
    if (v.type < MULIB_MUNE_NIL || v.type > MULIB_MUNE_THREAD) return 0;
    if (v.type == MULIB_MUNE_BOOLEAN) return v.as.boolean == 0 || v.as.boolean == 1;
    return v.type < MULIB_MUNE_STRING || (v.as.object && v.as.object->owner == s && v.as.object->kind == (unsigned)v.type);
  }
  static mulib_mune_result_t *
  mulib_mune_rt_results_ (mulib_mune_state_t *s)
  {
    mulib_mune_result_t *r = (mulib_mune_result_t *)mulib_mune_rt_memory_ (s, NULL, 0, sizeof (*r));
    if (!r) return NULL;
    memset (r, 0, sizeof (*r)); r->owner = s; r->next = s->results;
    r->thread = s->running ? s->current_thread : NULL;
    if (r->next) r->next->previous = r;
    s->results = r; return r;
  }
  size_t
  mulib_mune_result_count (const mulib_mune_result_t *r) { return r ? r->count : 0; }
  mulib_mune_result_t *
  mulib_mune_result_create (mulib_mune_state_t *s)
  {
    mulib_mune_result_t *r;
    if (!s) return NULL;
    if (!s->running) s->status = MULIB_MUNE_OK;
    r = mulib_mune_rt_results_ (s); if (r) r->thread = NULL; return r;
  }
  mulib_mune_value_t
  mulib_mune_result_get (const mulib_mune_result_t *r, size_t index)
  {
    return r && index < r->count ? r->values[index] : mulib_mune_rt_nil_ ();
  }
  int
  mulib_mune_result_push (mulib_mune_result_t *r, mulib_mune_value_t v)
  {
    size_t cap;
    mulib_mune_value_t *p;
    if (!r || !mulib_mune_rt_valid_ (r->owner, v)) return 0;
    if (r->count == r->owner->limits.results)
      return mulib_mune_rt_error_ (r->owner, MULIB_MUNE_LIMIT, "Lua value-list limit");
    if (r->count == r->capacity)
      {
        cap = r->capacity ? r->capacity > SIZE_MAX / 2 ? SIZE_MAX : r->capacity * 2 : 4;
        if (cap > r->owner->limits.results) cap = r->owner->limits.results;
        if (cap > SIZE_MAX / sizeof (*p)) return mulib_mune_rt_error_ (r->owner, MULIB_MUNE_MEMORY, "Lua value-list size overflow");
        p = (mulib_mune_value_t *)mulib_mune_rt_memory_ (r->owner, r->values, r->capacity * sizeof (*p), cap * sizeof (*p));
        if (!p) return 0;
        r->values = p; r->capacity = cap;
      }
    r->values[r->count++] = v; return 1;
  }
  void
  mulib_mune_result_destroy (mulib_mune_result_t *r)
  {
    mulib_mune_state_t *s;
    if (!r) return;
    s = r->owner;
    if (r->previous) r->previous->next = r->next; else s->results = r->next;
    if (r->next) r->next->previous = r->previous;
    (void)mulib_mune_rt_memory_ (s, r->values, r->capacity * sizeof (*r->values), 0);
    (void)mulib_mune_rt_memory_ (s, r, sizeof (*r), 0);
  }
  static mulib_mune_result_t *
  mulib_mune_rt_single_ (mulib_mune_state_t *s, mulib_mune_value_t v)
  {
    mulib_mune_result_t *r = mulib_mune_rt_results_ (s);
    if (r && !mulib_mune_result_push (r, v)) { mulib_mune_result_destroy (r); r = NULL; }
    return r;
  }
  static int
  mulib_mune_rt_pin_ (mulib_mune_state_t *s, mulib_mune_value_t v)
  {
    if (!s->frame) return mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "missing native call frame");
    if (!s->frame->varargs) s->frame->varargs = mulib_mune_rt_results_ (s);
    return s->frame->varargs && mulib_mune_result_push (s->frame->varargs, v);
  }
  static int
  mulib_mune_rt_append_ (mulib_mune_result_t *to, const mulib_mune_result_t *from, size_t start)
  {
    size_t k;
    for (k = start; k < from->count; ++k) if (!mulib_mune_result_push (to, from->values[k])) return 0;
    return 1;
  }
  const char *
  mulib_mune_value_string (mulib_mune_value_t v, size_t *length)
  {
    if (length) *length = 0;
    if (v.type != MULIB_MUNE_STRING || !v.as.object) return NULL;
    if (length) *length = v.as.object->as.length;
    return (const char *)(v.as.object + 1);
  }
  mulib_mune_status_t
  mulib_mune_make_string (mulib_mune_state_t *s, const char *text, size_t length, mulib_mune_value_t *out)
  {
    mulib_mune_object_t *o;
    if (!s || !out || (!text && length) || length == SIZE_MAX) return MULIB_MUNE_INVALID;
    o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_STRING, length + 1);
    if (!o) return s->status;
    o->as.length = length;
    if (length) memcpy (o + 1, text, length);
    ((char *)(o + 1))[length] = 0; *out = mulib_mune_rt_object_ (o); return MULIB_MUNE_OK;
  }
  static mulib_mune_value_t
  mulib_mune_rt_string_ (mulib_mune_state_t *s, const char *text, size_t n)
  {
    mulib_mune_value_t v = mulib_mune_rt_nil_ ();
    (void)mulib_mune_make_string (s, text, n, &v); return v;
  }
  static int64_t
  mulib_mune_rt_signed_ (uint64_t n)
  {
    return n <= INT64_MAX ? (int64_t)n : -1 - (int64_t)(UINT64_MAX - n);
  }
  static int
  mulib_mune_rt_exact_integer_ (double n, int64_t *out)
  {
    if (!(n >= -0x1p63 && n < 0x1p63) || trunc (n) != n) return 0;
    *out = (int64_t)n; return 1;
  }
  static mulib_mune_value_t
  mulib_mune_rt_key_ (mulib_mune_value_t v)
  {
    int64_t i;
    if (v.type == MULIB_MUNE_NUMBER && mulib_mune_rt_exact_integer_ (v.as.number, &i)) return mulib_mune_rt_integer_ (i);
    return v;
  }
  static int
  mulib_mune_rt_equal_ (mulib_mune_value_t a, mulib_mune_value_t b)
  {
    a = mulib_mune_rt_key_ (a); b = mulib_mune_rt_key_ (b);
    if (a.type != b.type) return 0;
    switch (a.type)
      {
      case MULIB_MUNE_NIL: return 1;
      case MULIB_MUNE_BOOLEAN: return a.as.boolean == b.as.boolean;
      case MULIB_MUNE_INTEGER: return a.as.integer == b.as.integer;
      case MULIB_MUNE_NUMBER: return a.as.number == b.as.number;
      case MULIB_MUNE_STRING:
        return a.as.object == b.as.object || (a.as.object->as.length == b.as.object->as.length
           && !memcmp (a.as.object + 1, b.as.object + 1, a.as.object->as.length));
      case MULIB_MUNE_USERDATA:
        if (a.as.object->userdata_tag == 3 && b.as.object->userdata_tag == 3)
          return a.as.object->as.userdata.identity == b.as.object->as.userdata.identity;
        return a.as.object == b.as.object;
      default: return a.as.object == b.as.object;
      }
  }
  static uint64_t
  mulib_mune_rt_hash_ (mulib_mune_value_t v)
  {
    uint64_t h = UINT64_C (1469598103934665603);
    const unsigned char *bytes = NULL;
    size_t n = 0, k;
    v = mulib_mune_rt_key_ (v);
    if (v.type == MULIB_MUNE_USERDATA && v.as.object->userdata_tag == 3)
      { h ^= (uint64_t)(uintptr_t)v.as.object->as.userdata.identity; h ^= h >> 30; return h; }
    if (v.type == MULIB_MUNE_STRING) { bytes = (const unsigned char *)(v.as.object + 1); n = v.as.object->as.length; }
    else if (v.type == MULIB_MUNE_NUMBER) { bytes = (const unsigned char *)&v.as.number; n = sizeof (v.as.number); }
    else
      {
        h ^= v.type == MULIB_MUNE_INTEGER ? (uint64_t)v.as.integer
             : v.type == MULIB_MUNE_BOOLEAN ? (uint64_t)v.as.boolean : (uint64_t)(uintptr_t)v.as.object;
      }
    for (k = 0; k < n; ++k) h = (h ^ bytes[k]) * UINT64_C (1099511628211);
    h ^= (uint64_t)v.type; h ^= h >> 30; h *= UINT64_C (0xbf58476d1ce4e5b9);
    h ^= h >> 27; h *= UINT64_C (0x94d049bb133111eb); return h ^ (h >> 31);
  }
  static size_t
  mulib_mune_rt_slot_ (mulib_mune_object_t *t, mulib_mune_value_t key, int *found)
  {
    size_t at = (size_t)mulib_mune_rt_hash_ (key) & (t->as.table.capacity - 1), tomb = SIZE_MAX;
    for (;;)
      {
        mulib_mune_entry_ *e = &t->as.table.entries[at];
        if (!e->used) { *found = 0; return tomb != SIZE_MAX ? tomb : at; }
        if (e->used == 1 && mulib_mune_rt_equal_ (e->key, key)) { *found = 1; return at; }
        if (e->used == 2 && tomb == SIZE_MAX) tomb = at;
        at = (at + 1) & (t->as.table.capacity - 1);
      }
  }
  static int
  mulib_mune_rt_resize_ (mulib_mune_state_t *s, mulib_mune_object_t *t, size_t cap)
  {
    mulib_mune_entry_ *entries, *old = t->as.table.entries;
    size_t before = t->as.table.capacity, k;
    if (cap > SIZE_MAX / sizeof (*entries)) return mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "table size overflow");
    entries = (mulib_mune_entry_ *)mulib_mune_rt_memory_ (s, NULL, 0, cap * sizeof (*entries));
    if (!entries) return 0;
    memset (entries, 0, cap * sizeof (*entries)); t->as.table.entries = entries; t->as.table.capacity = cap; t->as.table.tombs = 0;
    for (k = 0; k < before; ++k) if (old[k].used == 1)
      { int found; size_t at = mulib_mune_rt_slot_ (t, old[k].key, &found); entries[at] = old[k]; }
    (void)mulib_mune_rt_memory_ (s, old, before * sizeof (*entries), 0); return 1;
  }
  static mulib_mune_value_t
  mulib_mune_rt_rawget_ (mulib_mune_object_t *t, mulib_mune_value_t key)
  {
    int found; size_t at;
    if (!t || !t->as.table.capacity || key.type == MULIB_MUNE_NIL) return mulib_mune_rt_nil_ ();
    at = mulib_mune_rt_slot_ (t, key, &found);
    return found ? t->as.table.entries[at].value : mulib_mune_rt_nil_ ();
  }
  static mulib_mune_value_t
  mulib_mune_rt_field_ (mulib_mune_object_t *t, const char *name, size_t n)
  {
    size_t k;
    if (t) for (k = 0; k < t->as.table.capacity; ++k)
      {
        const mulib_mune_entry_ *e = &t->as.table.entries[k];
        if (e->used == 1 && e->key.type == MULIB_MUNE_STRING && e->key.as.object->as.length == n
            && !memcmp (e->key.as.object + 1, name, n)) return e->value;
      }
    return mulib_mune_rt_nil_ ();
  }
  static int
  mulib_mune_rt_rawset_ (mulib_mune_state_t *s, mulib_mune_object_t *t, mulib_mune_value_t key, mulib_mune_value_t v)
  {
    int found; size_t at;
    mulib_mune_entry_ *e;
    key = mulib_mune_rt_key_ (key);
    if (key.type == MULIB_MUNE_NIL || (key.type == MULIB_MUNE_NUMBER && isnan (key.as.number)))
      return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "table index is nil or NaN");
    if (t->as.table.capacity)
      {
        at = mulib_mune_rt_slot_ (t, key, &found);
        if (found)
          {
            e = &t->as.table.entries[at]; e->value = v;
            if (v.type == MULIB_MUNE_NIL) { e->used = 2; --t->as.table.count; ++t->as.table.tombs; }
            return 1;
          }
      }
    if (v.type == MULIB_MUNE_NIL) return 1;
    if (!t->as.table.capacity || t->as.table.count + t->as.table.tombs >= t->as.table.capacity - t->as.table.capacity / 4)
      {
        if (t->as.table.capacity > SIZE_MAX / 2) return mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "table capacity overflow");
        if (!mulib_mune_rt_resize_ (s, t, t->as.table.capacity ? t->as.table.tombs > t->as.table.count ? t->as.table.capacity : t->as.table.capacity * 2 : 8)) return 0;
      }
    at = mulib_mune_rt_slot_ (t, key, &found); e = &t->as.table.entries[at];
    if (v.type == MULIB_MUNE_NIL)
      {
        if (found) { e->used = 2; e->value = v; --t->as.table.count; ++t->as.table.tombs; }
        return 1;
      }
    if (!found) { if (e->used == 2) --t->as.table.tombs; ++t->as.table.count; }
    e->used = 1; e->key = key; e->value = v; return 1;
  }
  static int
  mulib_mune_rt_setfield_ (mulib_mune_state_t *s, mulib_mune_object_t *t, const char *name, size_t n, mulib_mune_value_t v)
  {
    mulib_mune_value_t key = mulib_mune_rt_string_ (s, name, n);
    return !s->status && mulib_mune_rt_rawset_ (s, t, key, v);
  }
  static mulib_mune_object_t *
  mulib_mune_rt_environment_ (mulib_mune_state_t *s, mulib_mune_object_t *parent,
                             const char *name, size_t n, mulib_mune_value_t v, size_t offset, int close)
  {
    mulib_mune_object_t *o;
    if (n == SIZE_MAX) { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "local name size overflow"); return NULL; }
    o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_ENVIRONMENT_, n + 1); if (!o) return NULL;
    memcpy (o + 1, name, n); ((char *)(o + 1))[n] = 0;
    o->as.environment.parent = parent; o->as.environment.value = v; o->as.environment.length = n;
    o->as.environment.offset = offset; o->as.environment.close = close; return o;
  }
  static mulib_mune_object_t *
  mulib_mune_rt_local_ (mulib_mune_object_t *e, const char *name, size_t n)
  {
    for (; e; e = e->as.environment.parent)
      if (e->as.environment.length == n && !memcmp (e + 1, name, n)) return e->as.environment.cell ? e->as.environment.cell : e;
    return NULL;
  }
  static void
  mulib_mune_rt_mark_object_ (mulib_mune_object_t *o, mulib_mune_object_t **gray)
  {
    if (o && !o->marked) { o->marked = 1; o->gray = *gray; *gray = o; }
  }
  static void
  mulib_mune_rt_mark_value_ (mulib_mune_value_t v, mulib_mune_object_t **gray)
  {
    if (v.type >= MULIB_MUNE_STRING) mulib_mune_rt_mark_object_ (v.as.object, gray);
  }
  static void mulib_mune_rt_thread_free_ (mulib_mune_state_t *, mulib_mune_object_t *);
  static int mulib_mune_rt_file_close_ (mulib_mune_object_t *);
  static void
  mulib_mune_rt_free_object_ (mulib_mune_state_t *s, mulib_mune_object_t *o)
  {
    if (o->kind == MULIB_MUNE_TABLE)
      (void)mulib_mune_rt_memory_ (s, o->as.table.entries, o->as.table.capacity * sizeof (*o->as.table.entries), 0);
    if (o->kind == MULIB_MUNE_FUNCTION) mulib_mune_chunk_destroy (o->as.function.chunk);
    if (o->kind == MULIB_MUNE_USERDATA && o->userdata_tag == 1 && o->as.file.stream && o->as.file.owned) (void)mulib_mune_rt_file_close_ (o);
    if (o->kind == MULIB_MUNE_THREAD) mulib_mune_rt_thread_free_ (s, o);
    (void)mulib_mune_rt_memory_ (s, o, o->bytes, 0);
  }
  static void mulib_mune_rt_finalizers_ (mulib_mune_state_t *, int);
  static void
  mulib_mune_rt_finalize_register_ (mulib_mune_state_t *s, mulib_mune_object_t *o)
  {
    if (!o->finalizable && !o->finalized && mulib_mune_rt_field_ (o->metatable, "__gc", 4).type != MULIB_MUNE_NIL)
      { o->finalizable = 1; o->finalize_next = s->finalizers; s->finalizers = o; }
  }
  static int
  mulib_mune_rt_alive_ (mulib_mune_value_t v)
  {
    return v.type <= MULIB_MUNE_STRING || (v.type == MULIB_MUNE_USERDATA && v.as.object->userdata_tag == 3) || v.as.object->marked;
  }
  static unsigned
  mulib_mune_rt_weak_ (mulib_mune_object_t *o)
  {
    mulib_mune_value_t mode = mulib_mune_rt_field_ (o->metatable, "__mode", 6);
    if (mode.type != MULIB_MUNE_STRING) return 0;
    return (memchr (mode.as.object + 1, 'k', mode.as.object->as.length) ? 1u : 0u)
           | (memchr (mode.as.object + 1, 'v', mode.as.object->as.length) ? 2u : 0u);
  }
  static void
  mulib_mune_rt_trace_ (mulib_mune_state_t *s, mulib_mune_object_t **gray)
  {
    mulib_mune_object_t *o;
    size_t k;
    do
      {
        while (*gray)
          {
            unsigned weak;
            o = *gray; *gray = o->gray; mulib_mune_rt_mark_object_ (o->metatable, gray);
            weak = o->kind == MULIB_MUNE_TABLE ? mulib_mune_rt_weak_ (o) : 0;
            if (o->kind == MULIB_MUNE_TABLE)
              for (k = 0; k < o->as.table.capacity; ++k)
                {
                  mulib_mune_entry_ *e = &o->as.table.entries[k];
                  if (e->used == 1)
                    {
                      if (!(weak & 1) || mulib_mune_rt_alive_ (e->key)) mulib_mune_rt_mark_value_ (e->key, gray);
                      if ((!(weak & 2) && (!(weak & 1) || mulib_mune_rt_alive_ (e->key))) || mulib_mune_rt_alive_ (e->value))
                        mulib_mune_rt_mark_value_ (e->value, gray);
                    }
                  else if (e->used == 2 && (e->key.type == MULIB_MUNE_STRING
                      || (e->key.type == MULIB_MUNE_USERDATA && e->key.as.object->userdata_tag == 3)))
                    mulib_mune_rt_mark_value_ (e->key, gray);
                }
            if (o->kind == MULIB_MUNE_FUNCTION)
              { mulib_mune_rt_mark_object_ (o->as.function.environment, gray); for (k = 0; k < 3; ++k) mulib_mune_rt_mark_value_ (o->as.function.context[k], gray); }
            if (o->kind == MULIB_MUNE_ENVIRONMENT_)
              { mulib_mune_rt_mark_object_ (o->as.environment.parent, gray); mulib_mune_rt_mark_object_ (o->as.environment.cell, gray); mulib_mune_rt_mark_value_ (o->as.environment.value, gray); }
            if (o->kind == MULIB_MUNE_USERDATA && o->userdata_tag == 2)
              mulib_mune_rt_mark_object_ (o->as.userdata.values, gray);
            if (o->kind == MULIB_MUNE_THREAD && o->as.thread)
              {
                mulib_mune_frame_ *f;
                mulib_mune_coroutine_ *co = o->as.thread;
                mulib_mune_result_t *r;
                mulib_mune_protected_ *p;
                for (p = s->current_thread && co == s->current_thread->as.thread ? s->protected_call : co->protected_call; p; p = p->previous)
                  mulib_mune_rt_mark_value_ (p->handler, gray);
                for (r = s->results; r; r = r->next) if (r->thread == o)
                  for (k = 0; k < r->count; ++k) mulib_mune_rt_mark_value_ (r->values[k], gray);
                mulib_mune_rt_mark_object_ (co->parent, gray); mulib_mune_rt_mark_value_ (co->function, gray); mulib_mune_rt_mark_value_ (co->error, gray);
                mulib_mune_rt_mark_value_ (co->hook, gray);
                for (f = co->frame; f; f = f->previous)
                  { mulib_mune_rt_mark_object_ (f->environment, gray); mulib_mune_rt_mark_value_ (f->function, gray); }
              }
          }
        /* Ephemerons: a weak-key table retains its value only when its key is
         * independently reachable. Repeat until cross-table reachability settles. */
        for (o = s->objects; o; o = o->next)
          if (o->marked && o->kind == MULIB_MUNE_TABLE && mulib_mune_rt_weak_ (o) == 1)
            for (k = 0; k < o->as.table.capacity; ++k)
              if (o->as.table.entries[k].used == 1 && mulib_mune_rt_alive_ (o->as.table.entries[k].key))
                mulib_mune_rt_mark_value_ (o->as.table.entries[k].value, gray);
      }
    while (*gray);
  }
  static void
  mulib_mune_rt_clear_weak_ (mulib_mune_state_t *s, unsigned bit)
  {
    mulib_mune_object_t *o; size_t k;
    for (o = s->objects; o; o = o->next)
      if (o->marked && o->kind == MULIB_MUNE_TABLE && (mulib_mune_rt_weak_ (o) & bit))
        for (k = 0; k < o->as.table.capacity; ++k)
          {
            mulib_mune_entry_ *e = &o->as.table.entries[k];
            if (e->used == 1 && !mulib_mune_rt_alive_ (bit == 1 ? e->key : e->value))
              { e->used = 2; e->key = e->value = mulib_mune_rt_nil_ (); --o->as.table.count; ++o->as.table.tombs; }
          }
  }
  static size_t
  mulib_mune_rt_gc_scale_ (size_t bytes, unsigned percent)
  {
    size_t whole = bytes / 100, fraction = bytes % 100 * (uint64_t)percent / 100;
    return percent && whole > (SIZE_MAX - fraction) / percent ? SIZE_MAX : whole * percent + fraction;
  }
  static void
  mulib_mune_rt_gc_start_ (mulib_mune_state_t *s, int minor)
  {
    mulib_mune_object_t *gray = NULL, **link, *o;
    mulib_mune_result_t *r;
    mulib_mune_frame_ *f;
    size_t k;
    s->collecting = 1; s->gc_minor_cycle = minor;
    for (o = s->objects; o; o = o->next) o->marked = 0;
    /* Minor collections conservatively rescan old objects, so old-to-young
     * mutations need no write barrier. Full cycles reclaim old unreachable
     * graphs. Marking, weak references and finalization form one atomic phase. */
    if (minor) for (o = s->objects; o; o = o->next)
      if (o->age >= 2) mulib_mune_rt_mark_object_ (o, &gray);
    mulib_mune_rt_mark_object_ (s->globals, &gray); mulib_mune_rt_mark_object_ (s->environment, &gray);
    mulib_mune_rt_mark_object_ (s->main_thread, &gray); mulib_mune_rt_mark_object_ (s->current_thread, &gray);
    mulib_mune_rt_mark_object_ (s->file_metatable, &gray); mulib_mune_rt_mark_object_ (s->registry, &gray);
    mulib_mune_rt_mark_value_ (s->error_value, &gray);
    mulib_mune_rt_mark_value_ (s->input, &gray); mulib_mune_rt_mark_value_ (s->output, &gray);
    for (k = 0; k < 9; ++k) mulib_mune_rt_mark_object_ (s->type_metatables[k], &gray);
    for (r = s->results; r; r = r->next)
      if (!r->thread) for (k = 0; k < r->count; ++k) mulib_mune_rt_mark_value_ (r->values[k], &gray);
    for (f = s->frame; f; f = f->previous)
      { mulib_mune_rt_mark_object_ (f->environment, &gray); mulib_mune_rt_mark_value_ (f->function, &gray); }
    mulib_mune_rt_trace_ (s, &gray);
    mulib_mune_rt_clear_weak_ (s, 2);
    for (o = s->objects; o; o = o->next)
      if (!o->marked && o->finalizable && !o->finalized)
        { o->finalizable = 2; mulib_mune_rt_mark_object_ (o, &gray); }
    mulib_mune_rt_trace_ (s, &gray);
    mulib_mune_rt_clear_weak_ (s, 1);
    /* Finalizers may resurrect suspended threads. Only abandon stacks after
     * tracing the entire pending-finalizer graph, before reclaiming its cells. */
    for (o = s->objects; o; o = o->next)
      if (!o->marked && o->kind == MULIB_MUNE_THREAD && o->as.thread && o->as.thread->started)
        mulib_mune_rt_thread_free_ (s, o);
    mulib_mune_rt_finalizers_ (s, 0);
    for (o = s->objects; o; o = o->next)
      if (o->marked && o->kind == MULIB_MUNE_TABLE)
        for (k = 0; k < o->as.table.capacity; ++k)
          if (o->as.table.entries[k].used == 2 && !mulib_mune_rt_alive_ (o->as.table.entries[k].key))
            o->as.table.entries[k].key = mulib_mune_rt_nil_ ();
    for (link = &s->finalizers; *link;)
      { if (!(*link)->marked) *link = (*link)->finalize_next; else link = &(*link)->finalize_next; }
    s->gc_sweep = &s->objects; s->collecting = 0;
  }
  static int
  mulib_mune_rt_gc_sweep_ (mulib_mune_state_t *s, size_t budget)
  {
    s->collecting = 1;
    while (s->gc_sweep && *s->gc_sweep && budget)
      {
        mulib_mune_object_t *o = *s->gc_sweep;
        size_t work = o->bytes;
        if (o->kind == MULIB_MUNE_TABLE) work += o->as.table.capacity * sizeof (*o->as.table.entries);
        budget = work >= budget ? 0 : budget - work;
        if (!o->marked) { *s->gc_sweep = o->next; mulib_mune_rt_free_object_ (s, o); }
        else
          {
            if (!s->gc_minor_cycle) o->age = 2;
            else if (o->age < 2) ++o->age;
            s->gc_sweep = &o->next;
          }
      }
    s->collecting = 0;
    if (s->gc_sweep && *s->gc_sweep) return 0;
    s->gc_sweep = NULL;
    if (s->gc_minor_cycle) ++s->gc_minor_cycles;
    else { s->gc_major_bytes = s->bytes; s->gc_minor_cycles = 0; }
    s->threshold = mulib_mune_rt_gc_scale_ (s->bytes, s->gc_generation ? 100u + (unsigned)s->gc_minor : (unsigned)s->gc_pause);
    if (s->threshold < 65536) s->threshold = 65536;
    if (s->threshold > s->limits.memory_bytes / 2) s->threshold = s->limits.memory_bytes / 2;
    if (s->gc_stopped) s->threshold = SIZE_MAX;
    return 1;
  }
  static int
  mulib_mune_rt_gc_step_ (mulib_mune_state_t *s, size_t budget)
  {
    if (s->collecting) return 0;
    if (!s->gc_sweep)
      mulib_mune_rt_gc_start_ (s, s->gc_generation && s->gc_major_bytes && s->gc_minor_cycles < 20
          && s->bytes <= mulib_mune_rt_gc_scale_ (s->gc_major_bytes, 100u + (unsigned)s->gc_major));
    return mulib_mune_rt_gc_sweep_ (s, budget ? budget : 1);
  }
  static void
  mulib_mune_rt_gc_auto_ (mulib_mune_state_t *s)
  {
    size_t budget;
    if (s->collecting || s->gc_stopped || (!s->gc_sweep && s->bytes <= s->threshold)) return;
    budget = s->gc_stepsize >= (int)(sizeof (size_t) * CHAR_BIT) ? SIZE_MAX : (size_t)1 << s->gc_stepsize;
    budget = mulib_mune_rt_gc_scale_ (budget, (unsigned)s->gc_stepmul);
    (void)mulib_mune_rt_gc_step_ (s, budget);
  }
  size_t
  mulib_mune_collect (mulib_mune_state_t *s)
  {
    if (!s || s->collecting) return s ? s->bytes : 0;
    if (s->gc_sweep) (void)mulib_mune_rt_gc_sweep_ (s, SIZE_MAX);
    mulib_mune_rt_gc_start_ (s, 0);
    (void)mulib_mune_rt_gc_sweep_ (s, SIZE_MAX);
    return s->bytes;
  }
  static const char *
  mulib_mune_rt_typename_ (mulib_mune_value_t v)
  {
    static const char *const names[] = { "nil", "boolean", "number", "number", "string", "table", "function", "userdata", "thread" };
    return v.type <= MULIB_MUNE_THREAD ? names[v.type] : "invalid";
  }
  static int
  mulib_mune_rt_parse_number_ (const char *text, size_t n, mulib_mune_value_t *out)
  {
    size_t begin = 0, end = n, at, digits = 0;
    int negative = 0, hex = 0, overflow = 0, integral = 1, saved_errno;
    uint64_t integer = 0;
    double number;
    char *tail;
    if (!text || memchr (text, 0, n)) return 0;
    while (begin < end && mulib_mune_space_ ((unsigned char)text[begin])) ++begin;
    while (end > begin && mulib_mune_space_ ((unsigned char)text[end - 1])) --end;
    at = begin;
    if (at < end && (text[at] == '-' || text[at] == '+')) negative = text[at++] == '-';
    if (at == end || (!mulib_mune_digit_ ((unsigned char)text[at]) && text[at] != '.')) return 0;
    if (end - at >= 2 && text[at] == '0' && (text[at + 1] == 'x' || text[at + 1] == 'X')) { hex = 1; at += 2; }
    for (; at < end; ++at)
      {
        int digit = hex ? mulib_mune_hex_ ((unsigned char)text[at])
                        : mulib_mune_digit_ ((unsigned char)text[at]) ? text[at] - '0' : -1;
        unsigned base = hex ? 16 : 10;
        if (digit < 0) { integral = 0; break; }
        if (integer > (UINT64_MAX - (unsigned)digit) / base) overflow = 1;
        integer = integer * base + (unsigned)digit; ++digits;
      }
    if (integral && digits && (hex || (!overflow && integer <= (negative ? UINT64_C (0x8000000000000000) : (uint64_t)INT64_MAX))))
      {
        *out = mulib_mune_rt_integer_ (mulib_mune_rt_signed_ (negative ? UINT64_C (0) - integer : integer)); return 1;
      }
    saved_errno = errno; errno = 0; number = strtod (text + begin, &tail); errno = saved_errno;
    if (tail != text + end) return 0;
    *out = mulib_mune_rt_number_ (number); return 1;
  }
  static int
  mulib_mune_rt_numeric_ (mulib_mune_value_t v, mulib_mune_value_t *out)
  {
    if (v.type == MULIB_MUNE_INTEGER || v.type == MULIB_MUNE_NUMBER) { *out = v; return 1; }
    return v.type == MULIB_MUNE_STRING && mulib_mune_rt_parse_number_ ((const char *)(v.as.object + 1), v.as.object->as.length, out);
  }
  static double
  mulib_mune_rt_double_ (mulib_mune_value_t v)
  {
    return v.type == MULIB_MUNE_INTEGER ? (double)v.as.integer : v.as.number;
  }
  static int
  mulib_mune_rt_int_ (mulib_mune_value_t v, int64_t *out)
  {
    if (!mulib_mune_rt_numeric_ (v, &v)) return 0;
    if (v.type == MULIB_MUNE_INTEGER) { *out = v.as.integer; return 1; }
    return mulib_mune_rt_exact_integer_ (v.as.number, out);
  }
  static mulib_mune_result_t *mulib_mune_rt_call_ (mulib_mune_state_t *, mulib_mune_value_t, mulib_mune_result_t *);
  static mulib_mune_result_t *mulib_mune_rt_expression_ (mulib_mune_state_t *, const mulib_ast_t *);
  static mulib_mune_value_t
  mulib_mune_rt_meta_ (mulib_mune_state_t *s, mulib_mune_value_t v, const char *name)
  {
    mulib_mune_object_t *t = v.type >= MULIB_MUNE_STRING ? v.as.object->metatable : NULL;
    if (!t && v.type != MULIB_MUNE_TABLE && (v.type != MULIB_MUNE_USERDATA || v.as.object->userdata_tag == 3))
      t = s->type_metatables[v.type == MULIB_MUNE_INTEGER ? MULIB_MUNE_NUMBER : v.type];
    return mulib_mune_rt_field_ (t, name, strlen (name));
  }
  static mulib_mune_result_t *
  mulib_mune_rt_metacall_ (mulib_mune_state_t *s, mulib_mune_value_t fn,
                          mulib_mune_value_t a, mulib_mune_value_t b, mulib_mune_value_t c, size_t n)
  {
    mulib_mune_result_t *args = mulib_mune_rt_results_ (s), *out = NULL;
    if (!args) return NULL;
    if (mulib_mune_result_push (args, a) && (n < 2 || mulib_mune_result_push (args, b))
        && (n < 3 || mulib_mune_result_push (args, c))) out = mulib_mune_rt_call_ (s, fn, args);
    mulib_mune_result_destroy (args); return out;
  }
  static void
  mulib_mune_rt_finalizers_ (mulib_mune_state_t *s, int all)
  {
    mulib_mune_object_t *o;
    mulib_mune_status_t saved = s->status;
    mulib_mune_value_t error = s->error_value;
    char message[sizeof (s->message)];
    size_t location = s->location;
    int running = s->running, has_error = s->has_error;
    mulib_mune_protected_ protect;
    protect.previous = s->protected_call; protect.handler = mulib_mune_rt_nil_ (); protect.handled = 0;
    s->protected_call = &protect;
    memcpy (message, s->message, sizeof (message)); s->running = 1;
    for (;;)
      {
        for (o = s->finalizers; o; o = o->finalize_next)
          if (!o->finalized && (all || o->finalizable == 2)) break;
        if (!o) break;
        {
          mulib_mune_result_t *r;
          mulib_mune_value_t value = mulib_mune_rt_object_ (o), method;
          o->finalized = 1; s->status = MULIB_MUNE_OK;
          method = mulib_mune_rt_meta_ (s, value, "__gc");
          if (method.type != MULIB_MUNE_NIL) { r = mulib_mune_rt_metacall_ (s, method, value, value, value, 1); mulib_mune_result_destroy (r); }
        }
      }
    s->protected_call = protect.previous;
    s->status = saved; s->has_error = has_error; s->error_value = error; s->location = location; s->running = running; memcpy (s->message, message, sizeof (message));
  }
  static int
  mulib_mune_rt_index_ (mulib_mune_state_t *s, mulib_mune_value_t object, mulib_mune_value_t key, mulib_mune_value_t *out)
  {
    unsigned depth;
    for (depth = 0; depth < 200; ++depth)
      {
        mulib_mune_value_t meta;
        if (object.type == MULIB_MUNE_TABLE)
          { *out = mulib_mune_rt_rawget_ (object.as.object, key); if (out->type != MULIB_MUNE_NIL) return 1; }
        meta = mulib_mune_rt_meta_ (s, object, "__index");
        if (meta.type == MULIB_MUNE_NIL)
          {
            if (object.type == MULIB_MUNE_TABLE) { *out = meta; return 1; }
            return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "attempt to index a %s value", mulib_mune_rt_typename_ (object));
          }
        if (meta.type == MULIB_MUNE_FUNCTION)
          {
            mulib_mune_result_t *r = mulib_mune_rt_metacall_ (s, meta, object, key, mulib_mune_rt_nil_ (), 2);
            if (!r) return 0;
            *out = mulib_mune_result_get (r, 0); mulib_mune_result_destroy (r); return 1;
          }
        object = meta;
      }
    return mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "__index chain too long");
  }
  static int
  mulib_mune_rt_newindex_ (mulib_mune_state_t *s, mulib_mune_value_t object, mulib_mune_value_t key, mulib_mune_value_t value)
  {
    unsigned depth;
    for (depth = 0; depth < 200; ++depth)
      {
        mulib_mune_value_t meta;
        if (object.type == MULIB_MUNE_TABLE && mulib_mune_rt_rawget_ (object.as.object, key).type != MULIB_MUNE_NIL)
          return mulib_mune_rt_rawset_ (s, object.as.object, key, value);
        meta = mulib_mune_rt_meta_ (s, object, "__newindex");
        if (meta.type == MULIB_MUNE_NIL)
          {
            if (object.type == MULIB_MUNE_TABLE) return mulib_mune_rt_rawset_ (s, object.as.object, key, value);
            return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "attempt to index a %s value", mulib_mune_rt_typename_ (object));
          }
        if (meta.type == MULIB_MUNE_FUNCTION)
          {
            mulib_mune_result_t *r = mulib_mune_rt_metacall_ (s, meta, object, key, value, 3);
            if (!r) return 0;
            mulib_mune_result_destroy (r); return 1;
          }
        object = meta;
      }
    return mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "__newindex chain too long");
  }
  static int
  mulib_mune_rt_lookup_ (mulib_mune_state_t *s, const char *name, size_t n, mulib_mune_value_t *out)
  {
    mulib_mune_object_t *e = mulib_mune_rt_local_ (s->frame->environment, name, n);
    mulib_mune_value_t key;
    if (e) { *out = e->as.environment.value; return 1; }
    e = mulib_mune_rt_local_ (s->frame->environment, "_ENV", 4);
    key = mulib_mune_rt_string_ (s, name, n);
    return !s->status && mulib_mune_rt_index_ (s, e ? e->as.environment.value : mulib_mune_rt_nil_ (), key, out);
  }
  static int
  mulib_mune_rt_text_ (mulib_mune_state_t *s, mulib_mune_value_t v, int metamethod, mulib_mune_value_t *out)
  {
    char text[128]; int n;
    if (metamethod)
      {
        mulib_mune_value_t method = mulib_mune_rt_meta_ (s, v, "__tostring");
        if (method.type != MULIB_MUNE_NIL)
          {
            mulib_mune_result_t *r = mulib_mune_rt_metacall_ (s, method, v, v, v, 1);
            if (!r) return 0;
            *out = mulib_mune_result_get (r, 0); mulib_mune_result_destroy (r);
            return out->type == MULIB_MUNE_STRING || mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "__tostring must return a string");
          }
      }
    if (v.type == MULIB_MUNE_STRING) { *out = v; return 1; }
    if (v.type == MULIB_MUNE_INTEGER) n = snprintf (text, sizeof (text), "%" PRId64, v.as.integer);
    else if (v.type == MULIB_MUNE_NUMBER)
      {
        n = snprintf (text, sizeof (text), "%.14g", v.as.number);
        if (n > 0 && (size_t)n + 2 < sizeof (text) && isfinite (v.as.number)
            && !strchr (text, '.') && !strchr (text, 'e') && !strchr (text, 'E'))
          { text[n++] = '.'; text[n++] = '0'; text[n] = 0; }
      }
    else if (!metamethod) return 0;
    else if (v.type == MULIB_MUNE_NIL) n = snprintf (text, sizeof (text), "nil");
    else if (v.type == MULIB_MUNE_BOOLEAN) n = snprintf (text, sizeof (text), "%s", v.as.boolean ? "true" : "false");
    else n = snprintf (text, sizeof (text), "%s: %p", mulib_mune_rt_typename_ (v), (void *)v.as.object);
    if (n < 0 || (size_t)n >= sizeof (text)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "number formatting failed");
    *out = mulib_mune_rt_string_ (s, text, (size_t)n); return !s->status;
  }
  static int
  mulib_mune_rt_less_ (mulib_mune_value_t a, mulib_mune_value_t b, int equal)
  {
    if (a.type == MULIB_MUNE_STRING)
      {
        size_t an = a.as.object->as.length, bn = b.as.object->as.length, n = an < bn ? an : bn;
        int cmp = memcmp (a.as.object + 1, b.as.object + 1, n);
        return cmp < 0 || (!cmp && (equal ? an <= bn : an < bn));
      }
    if (a.type == b.type)
      {
        if (a.type == MULIB_MUNE_INTEGER) return equal ? a.as.integer <= b.as.integer : a.as.integer < b.as.integer;
        return equal ? a.as.number <= b.as.number : a.as.number < b.as.number;
      }
    if (a.type == MULIB_MUNE_NUMBER)
      {
        if (isnan (a.as.number)) return 0;
        return !mulib_mune_rt_less_ (b, a, !equal);
      }
    if (isnan (b.as.number)) return 0;
    if (b.as.number >= 0x1p63) return 1;
    if (b.as.number < -0x1p63) return 0;
    {
      int64_t n = (int64_t)b.as.number;
      return a.as.integer < n || (a.as.integer == n
        && (equal ? (double)a.as.integer <= b.as.number : (double)a.as.integer < b.as.number));
    }
  }
  static int
  mulib_mune_rt_operation_ (mulib_mune_state_t *s, const char *op, mulib_mune_value_t left, mulib_mune_value_t right, mulib_mune_value_t *out)
  {
    mulib_mune_value_t a, b, method;
    const char *meta = NULL;
    int unary = !strcmp (op, "unm") || !strcmp (op, "bnot") || !strcmp (op, "not") || !strcmp (op, "#");
    if (!strcmp (op, "not")) { *out = mulib_mune_rt_boolean_ (!mulib_mune_rt_truth_ (left)); return 1; }
    if (!strcmp (op, "==") || !strcmp (op, "~="))
      {
        int eq = mulib_mune_rt_equal_ (left, right);
        if (eq || left.type != right.type || (left.type != MULIB_MUNE_TABLE && left.type != MULIB_MUNE_USERDATA))
          { *out = mulib_mune_rt_boolean_ (eq != !strcmp (op, "~=")); return 1; }
        meta = "__eq";
      }
    else if (!strcmp (op, "<") || !strcmp (op, "<=") || !strcmp (op, ">") || !strcmp (op, ">="))
      {
        int le = op[1] == '=';
        if (op[0] == '>') { a = left; left = right; right = a; }
        if (((left.type == MULIB_MUNE_INTEGER || left.type == MULIB_MUNE_NUMBER)
             && (right.type == MULIB_MUNE_INTEGER || right.type == MULIB_MUNE_NUMBER))
            || (left.type == MULIB_MUNE_STRING && right.type == MULIB_MUNE_STRING))
          { *out = mulib_mune_rt_boolean_ (mulib_mune_rt_less_ (left, right, le)); return 1; }
        meta = le ? "__le" : "__lt";
      }
    else if (!strcmp (op, "#"))
      {
        if (left.type == MULIB_MUNE_STRING) { *out = mulib_mune_rt_integer_ ((int64_t)left.as.object->as.length); return 1; }
        method = mulib_mune_rt_meta_ (s, left, "__len");
        if (left.type == MULIB_MUNE_TABLE && method.type == MULIB_MUNE_NIL)
          {
            int64_t lo = 0, hi = 1;
            while (mulib_mune_rt_rawget_ (left.as.object, mulib_mune_rt_integer_ (hi)).type != MULIB_MUNE_NIL)
              { lo = hi; if (hi > INT64_MAX / 2) { hi = INT64_MAX; break; } hi *= 2; }
            while (hi - lo > 1)
              {
                int64_t mid = lo + (hi - lo) / 2;
                if (mulib_mune_rt_rawget_ (left.as.object, mulib_mune_rt_integer_ (mid)).type == MULIB_MUNE_NIL) hi = mid; else lo = mid;
              }
            *out = mulib_mune_rt_integer_ (lo); return 1;
          }
        meta = "__len";
      }
    else if (!strcmp (op, ".."))
      {
        if (mulib_mune_rt_text_ (s, left, 0, &a) && mulib_mune_rt_text_ (s, right, 0, &b))
          {
            size_t an = a.as.object->as.length, bn = b.as.object->as.length;
            mulib_mune_object_t *o;
            if (an > SIZE_MAX - bn - 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "concatenation size overflow");
            o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_STRING, an + bn + 1); if (!o) return 0;
            memcpy (o + 1, a.as.object + 1, an); memcpy ((char *)(o + 1) + an, b.as.object + 1, bn);
            ((char *)(o + 1))[an + bn] = 0; o->as.length = an + bn; *out = mulib_mune_rt_object_ (o); return 1;
          }
        meta = "__concat";
      }
    else if (!strcmp (op, "&") || !strcmp (op, "|") || !strcmp (op, "~")
             || !strcmp (op, "<<") || !strcmp (op, ">>") || !strcmp (op, "bnot"))
      {
        int64_t x, y = 0;
        if (left.type != MULIB_MUNE_STRING && (unary || right.type != MULIB_MUNE_STRING)
            && mulib_mune_rt_int_ (left, &x) && (unary || mulib_mune_rt_int_ (right, &y)))
          {
            uint64_t z, ux = (uint64_t)x;
            if (!strcmp (op, "&")) z = ux & (uint64_t)y;
            else if (!strcmp (op, "|")) z = ux | (uint64_t)y;
            else if (!strcmp (op, "~")) z = ux ^ (uint64_t)y;
            else if (unary) z = ~ux;
            else
              {
                uint64_t shift = y < 0 ? UINT64_C (0) - (uint64_t)y : (uint64_t)y;
                int to_left = (op[0] == '<') != (y < 0);
                z = shift >= 64 ? 0 : to_left ? ux << shift : ux >> shift;
              }
            *out = mulib_mune_rt_integer_ (mulib_mune_rt_signed_ (z)); return 1;
          }
        meta = !strcmp (op, "&") ? "__band" : !strcmp (op, "|") ? "__bor" : !strcmp (op, "~") ? "__bxor"
               : !strcmp (op, "<<") ? "__shl" : !strcmp (op, ">>") ? "__shr" : "__bnot";
      }
    else
      {
        if ((left.type == MULIB_MUNE_INTEGER || left.type == MULIB_MUNE_NUMBER)
            && (unary || right.type == MULIB_MUNE_INTEGER || right.type == MULIB_MUNE_NUMBER))
          {
            a = left; b = right;
            if (unary) b = mulib_mune_rt_integer_ (0);
            if (a.type == MULIB_MUNE_INTEGER && b.type == MULIB_MUNE_INTEGER && strcmp (op, "/") && strcmp (op, "^"))
              {
                int64_t x = a.as.integer, y = b.as.integer, z;
                if (!strcmp (op, "+")) z = mulib_mune_rt_signed_ ((uint64_t)x + (uint64_t)y);
                else if (!strcmp (op, "-")) z = mulib_mune_rt_signed_ ((uint64_t)x - (uint64_t)y);
                else if (!strcmp (op, "*")) z = mulib_mune_rt_signed_ ((uint64_t)x * (uint64_t)y);
                else if (unary) z = mulib_mune_rt_signed_ (UINT64_C (0) - (uint64_t)x);
                else
                  {
                    if (!y) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "integer division by zero");
                    if (x == INT64_MIN && y == -1) z = !strcmp (op, "//") ? INT64_MIN : 0;
                    else if (!strcmp (op, "//")) { z = x / y; if (x % y && ((x < 0) != (y < 0))) --z; }
                    else { z = x % y; if (z && ((z < 0) != (y < 0))) z += y; }
                  }
                *out = mulib_mune_rt_integer_ (z); return 1;
              }
            else
              {
                double x = mulib_mune_rt_double_ (a), y = mulib_mune_rt_double_ (b), z;
                if (!strcmp (op, "+")) z = x + y;
                else if (!strcmp (op, "-")) z = x - y;
                else if (!strcmp (op, "*")) z = x * y;
                else if (!strcmp (op, "/")) z = x / y;
                else if (!strcmp (op, "//")) z = floor (x / y);
                else if (!strcmp (op, "^")) z = pow (x, y);
                else if (unary) z = -x;
                else { z = fmod (x, y); if (z && ((z < 0) != (y < 0))) z += y; }
                *out = mulib_mune_rt_number_ (z); return 1;
              }
          }
        meta = !strcmp (op, "+") ? "__add" : !strcmp (op, "-") ? "__sub" : !strcmp (op, "*") ? "__mul"
               : !strcmp (op, "/") ? "__div" : !strcmp (op, "//") ? "__idiv" : !strcmp (op, "%") ? "__mod"
               : !strcmp (op, "^") ? "__pow" : "__unm";
      }
    if (s->status) return 0;
    method = mulib_mune_rt_meta_ (s, left, meta);
    if (method.type == MULIB_MUNE_NIL && !unary) method = mulib_mune_rt_meta_ (s, right, meta);
    if (method.type == MULIB_MUNE_NIL && !strcmp (meta, "__eq"))
      { *out = mulib_mune_rt_boolean_ (!strcmp (op, "~=")); return 1; }
    if (method.type == MULIB_MUNE_NIL && !strcmp (meta, "__le"))
      {
        method = mulib_mune_rt_meta_ (s, right, "__lt");
        if (method.type == MULIB_MUNE_NIL) method = mulib_mune_rt_meta_ (s, left, "__lt");
        if (method.type != MULIB_MUNE_NIL)
          {
            mulib_mune_result_t *r = mulib_mune_rt_metacall_ (s, method, right, left, left, 2);
            if (!r) return 0;
            *out = mulib_mune_rt_boolean_ (!mulib_mune_rt_truth_ (mulib_mune_result_get (r, 0))); mulib_mune_result_destroy (r); return 1;
          }
      }
    if (method.type != MULIB_MUNE_NIL)
      {
        mulib_mune_result_t *r = mulib_mune_rt_metacall_ (s, method, left, unary ? left : right, left, 2);
        if (!r) return 0;
        *out = mulib_mune_result_get (r, 0);
        if (!strcmp (meta, "__eq") || !strcmp (meta, "__lt") || !strcmp (meta, "__le"))
          *out = mulib_mune_rt_boolean_ (mulib_mune_rt_truth_ (*out) != !strcmp (op, "~="));
        mulib_mune_result_destroy (r); return 1;
      }
    return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid operands to %s (%s, %s)", op,
                                mulib_mune_rt_typename_ (left), mulib_mune_rt_typename_ (right));
  }
  static int mulib_mune_rt_hook_ (mulib_mune_state_t *, const char *, size_t);
  static void mulib_mune_rt_handle_error_ (mulib_mune_state_t *);
  static void mulib_mune_rt_thread_error_ (mulib_mune_state_t *);
  static size_t
  mulib_mune_rt_line_ (const mulib_mune_chunk_t *chunk, size_t offset)
  {
    size_t at, line = 1;
    if (!chunk) return 0;
    for (at = 0; at < offset && at < chunk->walker->root->length; ++at)
      if (chunk->walker->root->text[at] == '\n' || chunk->walker->root->text[at] == '\r')
        {
          char c = chunk->walker->root->text[at]; ++line;
          if (at + 1 < offset && at + 1 < chunk->walker->root->length
              && (chunk->walker->root->text[at + 1] == '\n' || chunk->walker->root->text[at + 1] == '\r')
              && chunk->walker->root->text[at + 1] != c) ++at;
        }
    return line;
  }
  static int
  mulib_mune_rt_tick_ (mulib_mune_state_t *s, const mulib_ast_t *n)
  {
    if (s->status) return 0;
    if (s->current_thread && (s->current_thread->as.thread->discard
        || (s->current_thread->as.thread->close_requested && !s->current_thread->as.thread->closing_callbacks)))
      return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "coroutine closed");
    s->location = n->offset;
    if (s->frame) s->frame->location = n->offset;
    if (s->steps == s->limits.steps) return mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "Lua execution step limit");
    ++s->steps;
    if (s->current_thread && s->current_thread->as.thread->hook.type == MULIB_MUNE_FUNCTION)
      {
        mulib_mune_coroutine_ *co = s->current_thread->as.thread;
        if (!co->in_hook && co->hook_count && !--co->hook_remaining)
          { co->hook_remaining = co->hook_count; if (!mulib_mune_rt_hook_ (s, "count", 0)) return 0; }
        if (!co->in_hook && (co->hook_mask & 4) && (mulib_mune_is_ (n, "statement") || mulib_mune_is_ (n, "return_statement")))
          {
            size_t line = mulib_mune_rt_line_ (s->frame->chunk, n->offset);
            if (line != co->hook_line) { co->hook_line = line; if (!mulib_mune_rt_hook_ (s, "line", line)) return 0; }
          }
      }
    return 1;
  }
  static mulib_mune_value_t
  mulib_mune_rt_literal_ (mulib_mune_state_t *s, const mulib_ast_t *n)
  {
    const unsigned char *text = (const unsigned char *)n->text;
    size_t at = 1, end = n->length - 1, used = 0;
    mulib_mune_object_t *o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_STRING, n->length + 1);
    unsigned char *out;
    if (!o) return mulib_mune_rt_nil_ ();
    out = (unsigned char *)(o + 1);
    if (text[0] == '[')
      {
        while (text[at] == '=') ++at;
        ++at; end = n->length - at;
        if (at < end && (text[at] == '\r' || text[at] == '\n'))
          { unsigned char c = text[at++]; if (at < end && (text[at] == '\r' || text[at] == '\n') && text[at] != c) ++at; }
      }
    while (at < end)
      {
        unsigned char c = text[at++];
        if (text[0] != '[' && c == '\\')
          {
            c = text[at++];
            if (c == 'z') { while (at < end && mulib_mune_space_ (text[at])) ++at; continue; }
            if (c == 'x') { c = (unsigned char)(16 * mulib_mune_hex_ (text[at]) + mulib_mune_hex_ (text[at + 1])); at += 2; }
            else if (c == 'u')
              {
                uint32_t value = 0; unsigned char bytes[6]; unsigned count = 0, j;
                ++at; while (text[at] != '}') value = value * 16 + (unsigned)mulib_mune_hex_ (text[at++]); ++at;
                if (value < 128) out[used++] = (unsigned char)value;
                else
                  {
                    uint32_t limit = 0x1f;
                    do { bytes[count++] = (unsigned char)(0x80 | (value & 63)); value >>= 6; limit >>= 1; } while (value > limit);
                    out[used++] = (unsigned char)((0xffu << (7 - count)) | value);
                    for (j = count; j; --j) out[used++] = bytes[j - 1];
                  }
                continue;
              }
            else if (mulib_mune_digit_ (c))
              { unsigned v = c - '0', j; for (j = 1; j < 3 && at < end && mulib_mune_digit_ (text[at]); ++j) v = v * 10 + text[at++] - '0'; c = (unsigned char)v; }
            else
              {
                switch (c)
                  {
                  case 'a': c = '\a'; break; case 'b': c = '\b'; break;
                  case 'f': c = '\f'; break; case 'n': c = '\n'; break;
                  case 'r': c = '\r'; break; case 't': c = '\t'; break; case 'v': c = '\v'; break;
                  case '\r': case '\n':
                    if (at < end && (text[at] == '\r' || text[at] == '\n') && text[at] != c) ++at;
                    c = '\n'; break;
                  default: break;
                  }
              }
          }
        else if (text[0] == '[' && (c == '\r' || c == '\n'))
          { if (at < end && (text[at] == '\r' || text[at] == '\n') && text[at] != c) ++at; c = '\n'; }
        out[used++] = c;
      }
    out[used] = 0; o->as.length = used; return mulib_mune_rt_object_ (o);
  }
  static mulib_mune_value_t
  mulib_mune_rt_closure_ (mulib_mune_state_t *s, const mulib_ast_t *body, int method)
  {
    mulib_mune_object_t *o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0);
    mulib_mune_prototype_ *p;
    mulib_mune_capture_name_ *c;
    mulib_mune_object_t **tail;
    if (!o) return mulib_mune_rt_nil_ ();
    o->as.function.body = body; o->as.function.method = method;
    o->as.function.chunk = s->frame->chunk; ++o->as.function.chunk->references;
    for (p = o->as.function.chunk->prototypes; p && p->body != body; p = p->next) {}
    tail = &o->as.function.environment;
    for (c = p ? p->captures : NULL; c; c = c->next)
      {
        mulib_mune_object_t *cell = mulib_mune_rt_local_ (s->frame->environment, c->name->text, c->name->length);
        *tail = mulib_mune_rt_environment_ (s, NULL, c->name->text, c->name->length, mulib_mune_rt_nil_ (), c->name->offset, 0);
        if (!*tail) return mulib_mune_rt_nil_ ();
        (*tail)->as.environment.cell = cell; tail = &(*tail)->as.environment.parent;
      }
    return mulib_mune_rt_object_ (o);
  }
  static void
  mulib_mune_rt_operator_ (const mulib_ast_t *n, const mulib_ast_t *left, char op[4])
  {
    mulib_mune_frontend_ p; size_t end, count = 0;
    memset (&p, 0, sizeof (p)); p.source = n->text; p.length = n->length;
    p.at = left->offset + left->length - n->offset;
    for (;;)
      {
        while (p.at < p.length && mulib_mune_space_ ((unsigned char)p.source[p.at])) ++p.at;
        if (p.length - p.at < 2 || p.source[p.at] != '-' || p.source[p.at + 1] != '-') break;
        p.at += 2;
        if (mulib_mune_long_ (&p, p.at, &end) == 1) p.at = end;
        else while (p.at < p.length && p.source[p.at] != '\n' && p.source[p.at] != '\r') ++p.at;
      }
    if (p.at < p.length)
      {
        char c = p.source[p.at++]; op[count++] = c;
        if (c == 'a') { op[count++] = 'n'; op[count++] = 'd'; }
        else if (c == 'o') op[count++] = 'r';
        else if (p.at < p.length && (p.source[p.at] == '=' || ((c == '<' || c == '>' || c == '/' || c == '.') && p.source[p.at] == c)))
          op[count++] = p.source[p.at];
      }
    op[count] = 0;
  }
  static mulib_mune_result_t *
  mulib_mune_rt_list_ (mulib_mune_state_t *s, const mulib_ast_t *n)
  {
    mulib_mune_result_t *r = mulib_mune_rt_results_ (s), *part;
    const mulib_ast_t *c;
    if (!r) return NULL;
    for (c = n ? n->child : NULL; c; c = c->next)
      {
        part = mulib_mune_rt_expression_ (s, c);
        if (!part) goto fail;
        if (c->next) (void)mulib_mune_result_push (r, mulib_mune_result_get (part, 0));
        else (void)mulib_mune_rt_append_ (r, part, 0);
        mulib_mune_result_destroy (part); if (s->status) goto fail;
      }
    return r;
  fail:
    mulib_mune_result_destroy (r); return NULL;
  }
  static mulib_mune_result_t *
  mulib_mune_rt_prefix_ (mulib_mune_state_t *s, const mulib_ast_t *n, const mulib_ast_t *stop,
                        int tail, mulib_mune_value_t *tail_function)
  {
    const mulib_ast_t *c;
    mulib_mune_result_t *r = mulib_mune_rt_expression_ (s, n->child);
    if (!r) return NULL;
    for (c = n->child->next; c && c != stop; c = c->next)
      {
        const mulib_ast_t *part = c->child;
        mulib_mune_value_t object = mulib_mune_result_get (r, 0), key, value;
        mulib_mune_result_t *args = NULL, *next = NULL;
        if (mulib_mune_is_ (part, "call"))
          {
            const mulib_ast_t *method = mulib_mune_child_ (part, "method_name");
            const mulib_ast_t *arguments = mulib_mune_child_ (part, "arguments")->child;
            mulib_mune_value_t fn = object;
            if (method)
              {
                const mulib_ast_t *name = method->child;
                key = mulib_mune_rt_string_ (s, name->text, name->length);
                if (s->status || !mulib_mune_rt_index_ (s, object, key, &fn) || !mulib_mune_result_push (r, fn)) goto fail;
              }
            args = mulib_mune_is_ (arguments, "expressions") ? mulib_mune_rt_list_ (s, arguments)
                   : arguments ? mulib_mune_rt_expression_ (s, arguments) : mulib_mune_rt_results_ (s);
            if (!args) goto fail;
            if (method)
              {
                if (!mulib_mune_result_push (args, mulib_mune_rt_nil_ ())) { mulib_mune_result_destroy (args); goto fail; }
                memmove (args->values + 1, args->values, (args->count - 1) * sizeof (*args->values)); args->values[0] = object;
              }
            if (tail && !c->next)
              { *tail_function = fn; mulib_mune_result_destroy (r); return args; }
            s->location = s->frame->location = part->offset;
            next = mulib_mune_rt_call_ (s, fn, args); mulib_mune_result_destroy (args);
          }
        else
          {
            if (mulib_mune_is_ (part, "index"))
              { args = mulib_mune_rt_expression_ (s, part->child); if (!args) goto fail; key = mulib_mune_result_get (args, 0); }
            else key = mulib_mune_rt_string_ (s, part->child->text, part->child->length);
            if (!s->status && mulib_mune_rt_index_ (s, object, key, &value)) next = mulib_mune_rt_single_ (s, value);
            mulib_mune_result_destroy (args);
          }
        mulib_mune_result_destroy (r); r = next; if (!r) return NULL;
      }
    return r;
  fail:
    mulib_mune_result_destroy (r); return NULL;
  }
  static mulib_mune_result_t *
  mulib_mune_rt_table_ (mulib_mune_state_t *s, const mulib_ast_t *n)
  {
    mulib_mune_object_t *table = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0);
    mulib_mune_result_t *r, *keys = NULL, *values = NULL;
    const mulib_ast_t *fields = n->child, *field;
    int64_t index = 1;
    if (!table) return NULL;
    r = mulib_mune_rt_single_ (s, mulib_mune_rt_object_ (table)); if (!r) return NULL;
    for (field = fields ? fields->child : NULL; field; field = field->next)
      if (mulib_mune_is_ (field, "field"))
        {
          const mulib_ast_t *f = field->child;
          mulib_mune_value_t key;
          if (mulib_mune_is_ (f, "indexed_field") || mulib_mune_is_ (f, "named_field"))
            {
              if (mulib_mune_is_ (f, "indexed_field")) keys = mulib_mune_rt_expression_ (s, f->child);
              else keys = mulib_mune_rt_single_ (s, mulib_mune_rt_string_ (s, f->child->text, f->child->length));
              if (!keys) goto fail;
              key = mulib_mune_result_get (keys, 0); values = mulib_mune_rt_expression_ (s, f->child->next);
              if (!values || !mulib_mune_rt_rawset_ (s, table, key, mulib_mune_result_get (values, 0))) goto fail;
            }
          else
            {
              size_t j, count;
              values = mulib_mune_rt_expression_ (s, f); if (!values) goto fail;
              count = field->next && field->next->next ? 1 : values->count;
              for (j = 0; j < count; ++j)
                if (!mulib_mune_rt_rawset_ (s, table, mulib_mune_rt_integer_ (index++), mulib_mune_result_get (values, j))) goto fail;
            }
          mulib_mune_result_destroy (keys); keys = NULL; mulib_mune_result_destroy (values); values = NULL;
        }
    return r;
  fail:
    mulib_mune_result_destroy (keys); mulib_mune_result_destroy (values); mulib_mune_result_destroy (r); return NULL;
  }
  static mulib_mune_result_t *mulib_mune_rt_compiled_expression_ (mulib_mune_state_t *, const mulib_ast_t *);
  static mulib_mune_result_t *mulib_mune_rt_tree_expression_ (mulib_mune_state_t *, const mulib_ast_t *);
  static mulib_mune_result_t *
  mulib_mune_rt_expression_ (mulib_mune_state_t *s, const mulib_ast_t *n)
  {
    return s->mode == MULIB_MUNE_TREE ? mulib_mune_rt_tree_expression_ (s, n) : mulib_mune_rt_compiled_expression_ (s, n);
  }
  static mulib_mune_result_t *
  mulib_mune_rt_tree_expression_ (mulib_mune_state_t *s, const mulib_ast_t *n)
  {
    mulib_mune_value_t v = mulib_mune_rt_nil_ ();
    mulib_mune_result_t *r, *right;
    const mulib_ast_t *c, *left;
    if (!mulib_mune_rt_tick_ (s, n)) return NULL;
    if (mulib_mune_is_ (n, "nil")) return mulib_mune_rt_single_ (s, v);
    if (mulib_mune_is_ (n, "true") || mulib_mune_is_ (n, "false")) return mulib_mune_rt_single_ (s, mulib_mune_rt_boolean_ (mulib_mune_is_ (n, "true")));
    if (mulib_mune_is_ (n, "number"))
      { if (!mulib_mune_rt_parse_number_ (n->text, n->length, &v)) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid numeral in current locale"); }
    else if (mulib_mune_is_ (n, "string")) v = mulib_mune_rt_literal_ (s, n);
    else if (mulib_mune_is_ (n, "name")) { if (!mulib_mune_rt_lookup_ (s, n->text, n->length, &v)) return NULL; }
    else if (mulib_mune_is_ (n, "vararg"))
      { r = mulib_mune_rt_results_ (s); if (r && s->frame->varargs) (void)mulib_mune_rt_append_ (r, s->frame->varargs, 0); return r; }
    else if (mulib_mune_is_ (n, "function_literal")) v = mulib_mune_rt_closure_ (s, n->child, 0);
    else if (mulib_mune_is_ (n, "table")) return mulib_mune_rt_table_ (s, n);
    else if (mulib_mune_is_ (n, "prefix")) return mulib_mune_rt_prefix_ (s, n, NULL, 0, NULL);
    else if (mulib_mune_is_ (n, "parenthesized"))
      {
        r = mulib_mune_rt_expression_ (s, n->child); if (!r) return NULL;
        v = mulib_mune_result_get (r, 0); r->count = 0; (void)mulib_mune_result_push (r, v); return r;
      }
    else if (mulib_mune_is_ (n, "unary") && mulib_mune_is_ (n->child, "unary_op"))
      {
        const char *op = n->child->text;
        r = mulib_mune_rt_expression_ (s, n->child->next); if (!r) return NULL;
        if (!mulib_mune_rt_operation_ (s, !strcmp (op, "-") ? "unm" : !strcmp (op, "~") ? "bnot" : op,
                                     mulib_mune_result_get (r, 0), v, &v)) { mulib_mune_result_destroy (r); return NULL; }
        r->count = 0; (void)mulib_mune_result_push (r, v); return r;
      }
    else if (n->child)
      {
        left = n->child; r = mulib_mune_rt_expression_ (s, left); if (!r) return NULL;
        for (c = left->next; c;)
          {
            char op[4];
            mulib_mune_rt_operator_ (n, left, op);
            if (mulib_mune_is_ (c, "compare_op") || mulib_mune_is_ (c, "shift_op")
                || mulib_mune_is_ (c, "add_op") || mulib_mune_is_ (c, "multiply_op")) c = c->next;
            v = mulib_mune_result_get (r, 0);
            if ((!strcmp (op, "and") && !mulib_mune_rt_truth_ (v)) || (!strcmp (op, "or") && mulib_mune_rt_truth_ (v)))
              { r->count = 0; (void)mulib_mune_result_push (r, v); left = c; c = c->next; continue; }
            right = mulib_mune_rt_expression_ (s, c); if (!right) { mulib_mune_result_destroy (r); return NULL; }
            if (!strcmp (op, "and") || !strcmp (op, "or")) v = mulib_mune_result_get (right, 0);
            else (void)mulib_mune_rt_operation_ (s, op, v, mulib_mune_result_get (right, 0), &v);
            r->count = 0; if (!s->status) (void)mulib_mune_result_push (r, v);
            mulib_mune_result_destroy (right);
            if (s->status) { mulib_mune_result_destroy (r); return NULL; }
            left = c; c = c->next;
          }
        return r;
      }
    else { mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "invalid expression node"); return NULL; }
    return s->status ? NULL : mulib_mune_rt_single_ (s, v);
  }
  enum { MULIB_MUNE_NEXT_, MULIB_MUNE_BREAK_, MULIB_MUNE_RETURN_, MULIB_MUNE_GOTO_, MULIB_MUNE_TAIL_ };
  typedef struct mulib_mune_control_
  {
    int kind;
    const mulib_ast_t *label;
    mulib_mune_result_t *values;
    mulib_mune_value_t function;
  } mulib_mune_control_;
  static void mulib_mune_rt_block_ (mulib_mune_state_t *, const mulib_ast_t *, int, mulib_mune_control_ *);
  static int
  mulib_mune_rt_bind_local_ (mulib_mune_state_t *s, const mulib_ast_t *name, mulib_mune_value_t value, int close)
  {
    mulib_mune_object_t *e;
    if (close && mulib_mune_rt_truth_ (value) && mulib_mune_rt_meta_ (s, value, "__close").type == MULIB_MUNE_NIL)
      return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "to-be-closed variable has no __close metamethod");
    e = mulib_mune_rt_environment_ (s, s->frame->environment, name->text, name->length, value, name->offset, close);
    if (!e) return 0;
    s->frame->environment = e; return 1;
  }
  static void
  mulib_mune_rt_close_ (mulib_mune_state_t *s, mulib_mune_object_t *base)
  {
    while (s->frame->environment != base)
      {
        mulib_mune_object_t *e = s->frame->environment;
        if (e->as.environment.close && mulib_mune_rt_truth_ (e->as.environment.value)
            && !(s->current_thread && s->current_thread->as.thread->discard))
          {
            mulib_mune_status_t saved = s->status;
            int had_error = s->has_error;
            char message[sizeof (s->message)];
            size_t location = s->location;
            mulib_mune_value_t error = s->error_value, method;
            mulib_mune_result_t *r;
            memcpy (message, s->message, sizeof (message)); s->status = MULIB_MUNE_OK;
            if (saved && !had_error && strcmp (message, "coroutine closed")) error = mulib_mune_rt_string_ (s, message, strlen (message));
            method = mulib_mune_rt_meta_ (s, e->as.environment.value, "__close"); e->as.environment.close = 0;
            if (s->current_thread) ++s->current_thread->as.thread->closing_callbacks;
            r = s->status ? NULL : mulib_mune_rt_metacall_ (s, method, e->as.environment.value, error, error, 2);
            if (s->current_thread) --s->current_thread->as.thread->closing_callbacks;
            mulib_mune_result_destroy (r);
            if (!s->status) { s->status = saved; s->has_error = had_error; s->location = location; s->error_value = error; memcpy (s->message, message, sizeof (message)); }
          }
        s->frame->environment = e->as.environment.parent; e->as.environment.parent = NULL;
      }
  }
  static const mulib_ast_t *
  mulib_mune_rt_tail_prefix_ (const mulib_ast_t *n)
  {
    while (n && !mulib_mune_is_ (n, "prefix"))
      {
        if (mulib_mune_is_ (n, "parenthesized") || mulib_mune_is_ (n, "table") || mulib_mune_is_ (n, "function_literal")
            || !n->child || n->child->next) return NULL;
        n = n->child;
      }
    return n && mulib_mune_is_ (n->tail, "suffix") && mulib_mune_is_ (n->tail->child, "call") ? n : NULL;
  }
  static mulib_mune_result_t *
  mulib_mune_rt_call_ (mulib_mune_state_t *s, mulib_mune_value_t fn, mulib_mune_result_t *args)
  {
    mulib_mune_frame_ frame;
    mulib_mune_result_t *owned = NULL, *result = NULL;
    unsigned chain = 0;
    if (s->status) return NULL;
    if (s->depth == s->limits.call_depth) { mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "Lua call depth limit"); return NULL; }
    memset (&frame, 0, sizeof (frame)); frame.previous = s->frame; s->frame = &frame; ++s->depth;
    for (;;)
      {
        mulib_mune_object_t *closure;
        mulib_mune_control_ control;
        frame.function = fn;
        if (fn.type != MULIB_MUNE_FUNCTION)
          {
            mulib_mune_value_t meta = mulib_mune_rt_meta_ (s, fn, "__call");
            mulib_mune_result_t *next;
            if (meta.type == MULIB_MUNE_NIL || ++chain == 200)
              { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "attempt to call a %s value", mulib_mune_rt_typename_ (fn)); break; }
            next = mulib_mune_rt_single_ (s, fn);
            if (!next || !mulib_mune_rt_append_ (next, args, 0)) { mulib_mune_result_destroy (next); break; }
            mulib_mune_result_destroy (owned); owned = args = next; fn = meta; continue;
          }
        closure = fn.as.object; frame.environment = closure->as.function.environment; frame.chunk = closure->as.function.chunk;
        if (closure->as.function.native)
          {
            mulib_mune_status_t status;
            if (!mulib_mune_rt_hook_ (s, frame.tail_call ? "tail call" : "call", 0)) break;
            result = mulib_mune_rt_results_ (s); if (!result) break;
            status = closure->as.function.native (s, args->values, args->count, result, closure->as.function.user);
            if (status && !s->status) mulib_mune_rt_error_ (s, status, "native function failed");
            break;
          }
        else
          {
            const mulib_ast_t *body = closure->as.function.body;
            const mulib_ast_t *params = mulib_mune_child_ (body, "parameters"), *names = mulib_mune_child_ (params, "names"), *name;
            size_t at = 0;
            if (closure->as.function.method)
              {
                frame.environment = mulib_mune_rt_environment_ (s, frame.environment, "self", 4, mulib_mune_result_get (args, at++), 0, 0);
                if (!frame.environment) break;
              }
            for (name = names ? names->child : NULL; name; name = name->next)
              if (!mulib_mune_rt_bind_local_ (s, name, mulib_mune_result_get (args, at++), 0)) break;
            if (s->status) break;
            frame.varargs = mulib_mune_rt_results_ (s); if (!frame.varargs) break;
            if (!mulib_mune_rt_append_ (frame.varargs, args, at)) break;
            if (!mulib_mune_rt_hook_ (s, frame.tail_call ? "tail call" : "call", 0)) break;
            memset (&control, 0, sizeof (control));
            mulib_mune_rt_block_ (s, mulib_mune_is_ (body, "chunk") ? body->child : mulib_mune_child_ (body, "block"), 0, &control);
            mulib_mune_rt_close_ (s, closure->as.function.environment);
            mulib_mune_result_destroy (frame.varargs); frame.varargs = NULL;
            if (s->status) { mulib_mune_result_destroy (control.values); break; }
            if (control.kind == MULIB_MUNE_TAIL_)
              {
                fn = control.function; mulib_mune_result_destroy (owned); owned = args = control.values;
                frame.environment = NULL; frame.tail_call = 1; continue;
              }
            result = control.values ? control.values : mulib_mune_rt_results_ (s); break;
          }
      }
    if (!s->status) (void)mulib_mune_rt_hook_ (s, "return", 0);
    else mulib_mune_rt_handle_error_ (s);
    mulib_mune_result_destroy (frame.varargs); mulib_mune_result_destroy (owned);
    s->frame = frame.previous; --s->depth;
    if (s->status) { mulib_mune_result_destroy (result); result = NULL; }
    return result;
  }
  typedef struct mulib_mune_reference_
  {
    mulib_mune_object_t *local;
    mulib_mune_value_t object, key;
  } mulib_mune_reference_;
  static int
  mulib_mune_rt_reference_ (mulib_mune_state_t *s, const mulib_ast_t *n, mulib_mune_reference_ *ref, mulib_mune_result_t *roots)
  {
    const mulib_ast_t *last = n->tail;
    memset (ref, 0, sizeof (*ref));
    if (last == n->child)
      {
        ref->local = mulib_mune_rt_local_ (s->frame->environment, last->text, last->length);
        if (ref->local) return 1;
        ref->local = mulib_mune_rt_local_ (s->frame->environment, "_ENV", 4);
        ref->object = ref->local ? ref->local->as.environment.value : mulib_mune_rt_nil_ (); ref->local = NULL;
        ref->key = mulib_mune_rt_string_ (s, last->text, last->length);
      }
    else
      {
        mulib_mune_result_t *object = mulib_mune_rt_prefix_ (s, n, last, 0, NULL), *key = NULL;
        if (!object) return 0;
        ref->object = mulib_mune_result_get (object, 0);
        if (mulib_mune_is_ (last->child, "index"))
          { key = mulib_mune_rt_expression_ (s, last->child->child); ref->key = mulib_mune_result_get (key, 0); }
        else ref->key = mulib_mune_rt_string_ (s, last->child->child->text, last->child->child->length);
        (void)mulib_mune_result_push (roots, ref->object); (void)mulib_mune_result_push (roots, ref->key);
        mulib_mune_result_destroy (key); mulib_mune_result_destroy (object); return !s->status;
      }
    return !s->status && mulib_mune_result_push (roots, ref->object) && mulib_mune_result_push (roots, ref->key);
  }
  static void
  mulib_mune_rt_assignment_ (mulib_mune_state_t *s, const mulib_ast_t *n)
  {
    const mulib_ast_t *variables = n->child, *v;
    size_t count = 0, at = 0;
    mulib_mune_reference_ *refs;
    mulib_mune_result_t *roots = NULL, *values = NULL;
    for (v = variables->child; v; v = v->next) ++count;
    if (count > SIZE_MAX / sizeof (*refs)) { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "assignment size overflow"); return; }
    refs = (mulib_mune_reference_ *)mulib_mune_rt_memory_ (s, NULL, 0, count * sizeof (*refs)); if (!refs) return;
    roots = mulib_mune_rt_results_ (s); if (!roots) goto done;
    for (v = variables->child; v; v = v->next) if (!mulib_mune_rt_reference_ (s, v, &refs[at++], roots)) goto done;
    values = mulib_mune_rt_list_ (s, variables->next); if (!values) goto done;
    for (at = count; at-- > 0;)
      {
        mulib_mune_value_t value = mulib_mune_result_get (values, at);
        if (refs[at].local) refs[at].local->as.environment.value = value;
        else if (!mulib_mune_rt_newindex_ (s, refs[at].object, refs[at].key, value)) break;
      }
  done:
    mulib_mune_result_destroy (roots); mulib_mune_result_destroy (values);
    (void)mulib_mune_rt_memory_ (s, refs, count * sizeof (*refs), 0);
  }
  static int
  mulib_mune_rt_condition_ (mulib_mune_state_t *s, const mulib_ast_t *expression)
  {
    mulib_mune_result_t *r = mulib_mune_rt_expression_ (s, expression);
    int value = r && mulib_mune_rt_truth_ (mulib_mune_result_get (r, 0)); mulib_mune_result_destroy (r); return value;
  }
  static void
  mulib_mune_rt_for_ (mulib_mune_state_t *s, const mulib_ast_t *n, mulib_mune_control_ *control)
  {
    const mulib_ast_t *names = n->child, *c, *block = mulib_mune_child_ (n, "block");
    mulib_mune_object_t *base = s->frame->environment;
    mulib_mune_result_t *values = NULL;
    if (mulib_mune_is_ (n, "numeric_for"))
      {
        mulib_mune_value_t numbers[3]; size_t k = 0;
        int integer;
        numbers[2] = mulib_mune_rt_integer_ (1);
        for (c = names->next; c && c != block; c = c->next)
          {
            values = mulib_mune_rt_expression_ (s, c); if (!values) goto done;
            if (!mulib_mune_rt_numeric_ (mulib_mune_result_get (values, 0), &numbers[k]))
              { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "numeric for requires numeric bounds"); goto done; }
            if (mulib_mune_result_get (values, 0).type == MULIB_MUNE_STRING) numbers[k] = mulib_mune_rt_number_ (mulib_mune_rt_double_ (numbers[k]));
            ++k;
            mulib_mune_result_destroy (values); values = NULL;
          }
        integer = numbers[0].type == MULIB_MUNE_INTEGER && numbers[2].type == MULIB_MUNE_INTEGER;
        if (!mulib_mune_rt_double_ (numbers[2])) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "for step is zero"); goto done; }
        if (integer)
          {
            int64_t i = numbers[0].as.integer, step = numbers[2].as.integer, limit;
            if (numbers[1].type == MULIB_MUNE_INTEGER) limit = numbers[1].as.integer;
            else
              {
                double bound = numbers[1].as.number;
                if (isnan (bound) || (step > 0 && bound < -0x1p63) || (step < 0 && bound >= 0x1p63)) goto done;
                limit = bound >= 0x1p63 ? INT64_MAX : bound < -0x1p63 ? INT64_MIN : (int64_t)(step > 0 ? floor (bound) : ceil (bound));
              }
            while (step > 0 ? i <= limit : i >= limit)
              {
                if (!mulib_mune_rt_tick_ (s, n) || !mulib_mune_rt_bind_local_ (s, names, mulib_mune_rt_integer_ (i), 0)) break;
                mulib_mune_rt_block_ (s, block, 0, control); mulib_mune_rt_close_ (s, base);
                if (s->status || control->kind) break;
                if ((step > 0 && i > INT64_MAX - step) || (step < 0 && i < INT64_MIN - step)) break;
                i += step;
              }
          }
        else
          {
            double i = mulib_mune_rt_double_ (numbers[0]), limit = mulib_mune_rt_double_ (numbers[1]), step = mulib_mune_rt_double_ (numbers[2]);
            while (step > 0 ? i <= limit : i >= limit)
              {
                if (!mulib_mune_rt_tick_ (s, n) || !mulib_mune_rt_bind_local_ (s, names, mulib_mune_rt_number_ (i), 0)) break;
                mulib_mune_rt_block_ (s, block, 0, control); mulib_mune_rt_close_ (s, base);
                if (s->status || control->kind) break;
                i += step;
              }
          }
      }
    else
      {
        mulib_mune_object_t *loop;
        values = mulib_mune_rt_list_ (s, names->next); if (!values) goto done;
        while (values->count < 4) if (!mulib_mune_result_push (values, mulib_mune_rt_nil_ ())) goto done;
        if (!mulib_mune_rt_bind_local_ (s, names, values->values[3], 1)) goto done;
        loop = s->frame->environment;
        while (mulib_mune_rt_tick_ (s, n))
          {
            mulib_mune_result_t *args = mulib_mune_rt_results_ (s), *iteration;
            size_t at = 0;
            if (!args) break;
            (void)mulib_mune_result_push (args, values->values[1]); (void)mulib_mune_result_push (args, values->values[2]);
            iteration = mulib_mune_rt_call_ (s, values->values[0], args); mulib_mune_result_destroy (args);
            if (!iteration) break;
            values->values[2] = mulib_mune_result_get (iteration, 0);
            if (values->values[2].type == MULIB_MUNE_NIL) { mulib_mune_result_destroy (iteration); break; }
            for (c = names->child; c; c = c->next) if (!mulib_mune_rt_bind_local_ (s, c, mulib_mune_result_get (iteration, at++), 0)) break;
            mulib_mune_result_destroy (iteration);
            if (!s->status) mulib_mune_rt_block_ (s, block, 0, control);
            mulib_mune_rt_close_ (s, loop); if (s->status || control->kind) break;
          }
      }
  done:
    mulib_mune_rt_close_ (s, base); mulib_mune_result_destroy (values);
    if (control->kind == MULIB_MUNE_BREAK_) control->kind = MULIB_MUNE_NEXT_;
  }
  static void
  mulib_mune_rt_statement_ (mulib_mune_state_t *s, const mulib_ast_t *n, mulib_mune_control_ *control)
  {
    const mulib_ast_t *c, *block;
    mulib_mune_result_t *r = NULL;
    if (!mulib_mune_rt_tick_ (s, n)) return;
    if (mulib_mune_is_ (n, "statement")) { mulib_mune_rt_statement_ (s, n->child, control); return; }
    if (mulib_mune_is_ (n, "empty_statement") || mulib_mune_is_ (n, "label")) return;
    if (mulib_mune_is_ (n, "break_statement")) { control->kind = MULIB_MUNE_BREAK_; return; }
    if (mulib_mune_is_ (n, "goto_statement")) { control->kind = MULIB_MUNE_GOTO_; control->label = n->child; return; }
    if (mulib_mune_is_ (n, "return_statement"))
      {
        const mulib_ast_t *list = mulib_mune_child_ (n, "expressions"), *tail = NULL;
        mulib_mune_object_t *e;
        if (list && !list->child->next) tail = mulib_mune_rt_tail_prefix_ (list->child);
        for (e = s->frame->environment; e && e != s->frame->function.as.object->as.function.environment; e = e->as.environment.parent)
          if (e->as.environment.close) { tail = NULL; break; }
        control->kind = tail ? MULIB_MUNE_TAIL_ : MULIB_MUNE_RETURN_;
        control->values = tail ? mulib_mune_rt_prefix_ (s, tail, NULL, 1, &control->function) : mulib_mune_rt_list_ (s, list);
        /* A tail callee must remain rooted while enclosing scopes unwind. */
        if (tail && control->values) s->frame->function = control->function;
        return;
      }
    if (mulib_mune_is_ (n, "assignment")) { mulib_mune_rt_assignment_ (s, n); return; }
    if (mulib_mune_is_ (n, "call_statement")) { r = mulib_mune_rt_expression_ (s, n->child); }
    else if (mulib_mune_is_ (n, "local_statement"))
      {
        size_t at = 0;
        r = mulib_mune_rt_list_ (s, mulib_mune_child_ (n, "expressions")); if (!r) return;
        for (c = n->child->child; c; c = c->next)
          {
            const mulib_ast_t *attr = mulib_mune_child_ (c, "attribute");
            if (!mulib_mune_rt_bind_local_ (s, c->child, mulib_mune_result_get (r, at++), attr && !strcmp (attr->child->text, "close"))) break;
          }
      }
    else if (mulib_mune_is_ (n, "local_function"))
      {
        if (mulib_mune_rt_bind_local_ (s, n->child, mulib_mune_rt_nil_ (), 0))
          s->frame->environment->as.environment.value = mulib_mune_rt_closure_ (s, n->child->next, 0);
      }
    else if (mulib_mune_is_ (n, "function_statement"))
      {
        mulib_mune_value_t fn = mulib_mune_rt_closure_ (s, n->child->next, mulib_mune_child_ (n->child, "method_name") != NULL), object, key;
        mulib_mune_object_t *local;
        r = mulib_mune_rt_single_ (s, fn); if (!r) return;
        c = n->child->child;
        if (!c->next)
          {
            local = mulib_mune_rt_local_ (s->frame->environment, c->text, c->length);
            if (local) local->as.environment.value = fn;
            else
              {
                local = mulib_mune_rt_local_ (s->frame->environment, "_ENV", 4);
                key = mulib_mune_rt_string_ (s, c->text, c->length);
                if (!s->status) (void)mulib_mune_rt_newindex_ (s, local->as.environment.value, key, fn);
              }
          }
        else if (mulib_mune_rt_lookup_ (s, c->text, c->length, &object))
          {
            for (c = c->next; c; c = c->next)
              {
                const mulib_ast_t *name = mulib_mune_is_ (c, "method_name") ? c->child : c;
                (void)mulib_mune_result_push (r, object);
                key = mulib_mune_rt_string_ (s, name->text, name->length);
                if (s->status) break;
                if (!c->next) (void)mulib_mune_rt_newindex_ (s, object, key, fn);
                else if (!mulib_mune_rt_index_ (s, object, key, &object)) break;
              }
          }
      }
    else if (mulib_mune_is_ (n, "do_statement")) mulib_mune_rt_block_ (s, n->child, 0, control);
    else if (mulib_mune_is_ (n, "if_statement"))
      {
        c = n->child;
        if (mulib_mune_rt_condition_ (s, c)) mulib_mune_rt_block_ (s, c->next, 0, control);
        else for (c = c->next->next; c && !s->status; c = c->next)
          {
            if (mulib_mune_is_ (c, "else_clause")) { mulib_mune_rt_block_ (s, c->child, 0, control); break; }
            if (mulib_mune_rt_condition_ (s, c->child)) { mulib_mune_rt_block_ (s, c->child->next, 0, control); break; }
          }
      }
    else if (mulib_mune_is_ (n, "while_statement") || mulib_mune_is_ (n, "repeat_statement"))
      {
        int repeat = mulib_mune_is_ (n, "repeat_statement");
        mulib_mune_object_t *base = s->frame->environment;
        block = mulib_mune_child_ (n, "block"); c = mulib_mune_child_ (n, "expression");
        while (mulib_mune_rt_tick_ (s, n) && (repeat || mulib_mune_rt_condition_ (s, c)))
          {
            int stop;
            mulib_mune_rt_block_ (s, block, repeat, control);
            stop = !s->status && !control->kind && repeat && mulib_mune_rt_condition_ (s, c);
            mulib_mune_rt_close_ (s, base);
            if (s->status || control->kind || stop) break;
          }
        if (control->kind == MULIB_MUNE_BREAK_) control->kind = MULIB_MUNE_NEXT_;
      }
    else if (mulib_mune_is_ (n, "numeric_for") || mulib_mune_is_ (n, "generic_for")) mulib_mune_rt_for_ (s, n, control);
    else mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "invalid statement node");
    mulib_mune_result_destroy (r);
  }
  static void mulib_mune_rt_compiled_block_ (mulib_mune_state_t *, const mulib_ast_t *, int, mulib_mune_control_ *);
  static void
  mulib_mune_rt_block_ (mulib_mune_state_t *s, const mulib_ast_t *n, int retain, mulib_mune_control_ *control)
  {
    const mulib_ast_t *c = n->child;
    mulib_mune_object_t *base = s->frame->environment;
    if (s->mode != MULIB_MUNE_TREE) { mulib_mune_rt_compiled_block_ (s, n, retain, control); return; }
    while (c && !s->status)
      {
        mulib_mune_rt_gc_auto_ (s);
        mulib_mune_rt_statement_ (s, c, control);
        if (s->status) { mulib_mune_rt_handle_error_ (s); break; }
        if (control->kind == MULIB_MUNE_GOTO_)
          {
            const mulib_ast_t *label;
            for (label = n->child; label; label = label->next)
              if (mulib_mune_is_ (label, "statement") && mulib_mune_is_ (label->child, "label")
                  && mulib_mune_same_name_ (label->child->child, control->label)) break;
            if (label)
              {
                const mulib_ast_t *after;
                mulib_mune_object_t *target = s->frame->environment;
                for (after = label->next; after; after = after->next)
                  if (!mulib_mune_is_ (after, "statement") || (!mulib_mune_is_ (after->child, "label") && !mulib_mune_is_ (after->child, "empty_statement"))) break;
                if (!after && !mulib_mune_is_ (n->parent, "repeat_statement")) target = base;
                else while (target != base && target->as.environment.offset > label->offset) target = target->as.environment.parent;
                mulib_mune_rt_close_ (s, target); control->kind = MULIB_MUNE_NEXT_; c = label->next; continue;
              }
          }
        if (control->kind) break;
        c = c->next;
      }
    if (!retain || s->status || control->kind) mulib_mune_rt_close_ (s, base);
  }
  static int mulib_mune_rt_library_ (mulib_mune_state_t *);
  static mulib_mune_object_t *mulib_mune_rt_thread_create_ (mulib_mune_state_t *, mulib_mune_value_t, int);
  mulib_mune_state_t *
  mulib_mune_state_create (const mulib_mune_runtime_options_t *options)
  {
    mulib_mune_state_t *s = (mulib_mune_state_t *)calloc (1, sizeof (*s));
    if (!s) return NULL;
    if (options) s->limits = *options;
    if (!s->limits.memory_bytes) s->limits.memory_bytes = 64 * 1024 * 1024;
    if (!s->limits.steps) s->limits.steps = 10000000;
    if (!s->limits.call_depth) s->limits.call_depth = 200;
    if (!s->limits.results) s->limits.results = 100000;
    s->threshold = 65536;
    s->gc_pause = 200; s->gc_stepmul = 100; s->gc_stepsize = 13; s->gc_minor = 20; s->gc_major = 100;
    s->globals = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0);
    s->environment = mulib_mune_rt_environment_ (s, NULL, "_ENV", 4, mulib_mune_rt_object_ (s->globals), 0, 0);
    s->main_thread = s->current_thread = mulib_mune_rt_thread_create_ (s, mulib_mune_rt_nil_ (), 1);
    if (!s->globals || !s->environment || !s->main_thread || !mulib_mune_rt_library_ (s)) { mulib_mune_state_destroy (s); return NULL; }
    return s;
  }
  void
  mulib_mune_state_destroy (mulib_mune_state_t *s)
  {
    mulib_mune_object_t *o;
    if (!s || s->running) return;
    s->collecting = 1; mulib_mune_rt_finalizers_ (s, 1);
    for (o = s->objects; o; o = o->next) if (o->kind == MULIB_MUNE_THREAD && o != s->main_thread) mulib_mune_rt_thread_free_ (s, o);
    while (s->results) mulib_mune_result_destroy (s->results);
    while ((o = s->objects) != NULL) { s->objects = o->next; mulib_mune_rt_free_object_ (s, o); }
    while (s->libraries)
      {
        mulib_mune_library_ *library = s->libraries; s->libraries = library->next;
        (void)mulib_dynload_close (&library->handle, NULL, 0); (void)mulib_mune_rt_memory_ (s, library, sizeof (*library), 0);
      }
    free (s);
  }
  mulib_mune_status_t
  mulib_mune_get_global (mulib_mune_state_t *s, const char *name, mulib_mune_value_t *out)
  {
    if (!s || !name || !out) return MULIB_MUNE_INVALID;
    *out = mulib_mune_rt_field_ (s->globals, name, strlen (name)); return MULIB_MUNE_OK;
  }
  mulib_mune_status_t
  mulib_mune_set_global (mulib_mune_state_t *s, const char *name, mulib_mune_value_t value)
  {
    if (!s || !name || !mulib_mune_rt_valid_ (s, value)) return MULIB_MUNE_INVALID;
    if (!s->running) s->status = MULIB_MUNE_OK;
    return mulib_mune_rt_setfield_ (s, s->globals, name, strlen (name), value) ? MULIB_MUNE_OK : s->status;
  }
  mulib_mune_status_t
  mulib_mune_bind (mulib_mune_state_t *s, const char *name, mulib_mune_native_fn function, void *user)
  {
    mulib_mune_object_t *o;
    if (!s || !name || !function) return MULIB_MUNE_INVALID;
    if (!s->running) s->status = MULIB_MUNE_OK;
    o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0); if (!o) return s->status;
    o->as.function.native = function; o->as.function.user = user;
    return mulib_mune_set_global (s, name, mulib_mune_rt_object_ (o));
  }
  mulib_mune_status_t
  mulib_mune_run (mulib_mune_state_t *s, mulib_mune_chunk_t *chunk, mulib_mune_result_t **out, mulib_mune_error_t *error)
  {
    mulib_mune_result_t *args = NULL, *result = NULL;
    mulib_mune_object_t *function;
    if (!s || !chunk || !out || *out || s->running)
      { if (error) { error->code = MULIB_MUNE_INVALID; error->offset = 0; error->message = "invalid execution arguments"; } return MULIB_MUNE_INVALID; }
    s->status = MULIB_MUNE_OK; s->has_error = 0; s->steps = s->location = 0; s->message[0] = 0; s->error_value = mulib_mune_rt_nil_ (); s->running = 1;
    function = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0);
    if (function)
      {
        function->as.function.body = chunk->walker->root; function->as.function.chunk = chunk; ++chunk->references;
        function->as.function.environment = s->environment;
        args = mulib_mune_rt_results_ (s);
        if (args) result = mulib_mune_rt_call_ (s, mulib_mune_rt_object_ (function), args);
      }
    mulib_mune_result_destroy (args); s->running = 0;
    if (error) { error->code = s->status; error->offset = s->location; error->message = s->status ? s->message : "ok"; }
    if (!s->status) { if (result) result->thread = NULL; *out = result; } else mulib_mune_result_destroy (result);
    return s->status;
  }
  mulib_mune_status_t
  mulib_mune_run_source (mulib_mune_state_t *s, const char *source, size_t n, mulib_mune_result_t **out, mulib_mune_error_t *error)
  {
    mulib_mune_error_t e; mulib_mune_status_t status;
    mulib_mune_chunk_t *chunk = mulib_mune_parse (source, n, NULL, &e);
    if (!chunk) { if (error) *error = e; return e.code; }
    status = mulib_mune_run (s, chunk, out, error); mulib_mune_chunk_destroy (chunk); return status;
  }
  mulib_mune_status_t
  mulib_mune_run_file (mulib_mune_state_t *s, const char *path, mulib_mune_result_t **out, mulib_mune_error_t *error)
  {
    mulib_mune_error_t e; mulib_mune_status_t status;
    mulib_mune_chunk_t *chunk = mulib_mune_parse_file (path, NULL, &e);
    if (!chunk) { if (error) *error = e; return e.code; }
    status = mulib_mune_run (s, chunk, out, error); mulib_mune_chunk_destroy (chunk); return status;
  }
  static mulib_mune_status_t
  mulib_mune_rt_builtin_ (mulib_mune_state_t *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *, void *);
  static int
  mulib_mune_rt_install_ (mulib_mune_state_t *s, mulib_mune_object_t *table, const char *field, const char *operation)
  {
    mulib_mune_object_t *o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0); if (!o) return 0;
    o->as.function.native = mulib_mune_rt_builtin_; o->as.function.user = (void *)operation;
    return mulib_mune_rt_setfield_ (s, table, field, strlen (field), mulib_mune_rt_object_ (o));
  }
  static mulib_mune_value_t
  mulib_mune_rt_arg_ (const mulib_mune_value_t *args, size_t n, size_t i)
  {
    return i < n ? args[i] : mulib_mune_rt_nil_ ();
  }
  static int
  mulib_mune_rt_need_ (mulib_mune_state_t *s, mulib_mune_value_t v, mulib_mune_type_t type, size_t i)
  {
    return v.type == type || mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "bad argument #%zu (%s expected)", i, mulib_mune_rt_typename_ ((mulib_mune_value_t){type, {0}}));
  }
  static int
  mulib_mune_rt_need_int_ (mulib_mune_state_t *s, mulib_mune_value_t v, int64_t *out, size_t i)
  {
    return mulib_mune_rt_int_ (v, out) || mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "bad argument #%zu (integer expected)", i);
  }
  static int
  mulib_mune_rt_next_ (mulib_mune_state_t *s, mulib_mune_value_t table, mulib_mune_value_t key, mulib_mune_result_t *out)
  {
    size_t k = 0;
    mulib_mune_object_t *t = table.as.object;
    if (!mulib_mune_rt_need_ (s, table, MULIB_MUNE_TABLE, 1)) return 0;
    if (key.type != MULIB_MUNE_NIL)
      {
        for (; k < t->as.table.capacity; ++k)
          if (t->as.table.entries[k].used && mulib_mune_rt_equal_ (key, t->as.table.entries[k].key)) break;
        if (k == t->as.table.capacity) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid key to next");
        ++k;
      }
    for (; k < t->as.table.capacity; ++k) if (t->as.table.entries[k].used == 1)
      return mulib_mune_result_push (out, t->as.table.entries[k].key) && mulib_mune_result_push (out, t->as.table.entries[k].value);
    return mulib_mune_result_push (out, mulib_mune_rt_nil_ ());
  }
  static int
  mulib_mune_rt_raise_ (mulib_mune_state_t *s, mulib_mune_value_t error)
  {
    if (s->status) return 0;
    s->status = MULIB_MUNE_RUNTIME;
    if (error.type == MULIB_MUNE_STRING) (void)snprintf (s->message, sizeof (s->message), "%s", (const char *)(error.as.object + 1));
    else (void)snprintf (s->message, sizeof (s->message), "Lua error (%s value)", mulib_mune_rt_typename_ (error));
    s->error_value = error; s->has_error = 1; return 0;
  }
  static int
  mulib_mune_rt_raise_level_ (mulib_mune_state_t *s, mulib_mune_value_t error, int64_t level)
  {
    mulib_mune_frame_ *f = s->frame;
    if (error.type == MULIB_MUNE_STRING && level > 0)
      {
        while (f && level--) f = f->previous;
        if (f && f->chunk)
          {
            char source[80], prefix[112]; int size;
            mulib_mune_object_t *o;
            mulib_mune_rt_short_source_ (f->chunk, source);
            size = snprintf (prefix, sizeof (prefix), "%s:%zu: ", source, mulib_mune_rt_line_ (f->chunk, f->location));
            if (size < 0 || (size_t)size >= sizeof (prefix)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "error location formatting failed");
            if (error.as.object->as.length > SIZE_MAX - (size_t)size - 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "error message size overflow");
            o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_STRING, error.as.object->as.length + (size_t)size + 1); if (!o) return 0;
            o->as.length = error.as.object->as.length + (size_t)size;
            memcpy (o + 1, prefix, (size_t)size);
            memcpy ((char *)(o + 1) + size, error.as.object + 1, error.as.object->as.length + 1);
            error = mulib_mune_rt_object_ (o);
          }
      }
    return mulib_mune_rt_raise_ (s, error);
  }
  static mulib_mune_value_t
  mulib_mune_rt_loaded_ (mulib_mune_state_t *s, mulib_mune_chunk_t *chunk, mulib_mune_value_t environment)
  {
    mulib_mune_object_t *f = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0), *env;
    if (!f) { mulib_mune_chunk_destroy (chunk); return mulib_mune_rt_nil_ (); }
    f->as.function.chunk = chunk; f->as.function.body = chunk->walker->root;
    env = mulib_mune_rt_environment_ (s, NULL, "_ENV", 4, environment, 0, 0);
    f->as.function.environment = env; return mulib_mune_rt_object_ (f);
  }
  static uint64_t
  mulib_mune_rt_random_ (mulib_mune_state_t *s)
  {
    uint64_t z = (s->random_state += UINT64_C (0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C (0xbf58476d1ce4e5b9); z = (z ^ (z >> 27)) * UINT64_C (0x94d049bb133111eb); return z ^ (z >> 31);
  }
  static int
  mulib_mune_rt_math_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0), b = mulib_mune_rt_arg_ (args, n, 1), v;
    double x, y = 0;
    if (!n && (!strcmp (op, "type") || !strcmp (op, "tointeger"))) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "value expected");
    if (!strcmp (op, "type"))
      return mulib_mune_result_push (out, a.type == MULIB_MUNE_INTEGER ? mulib_mune_rt_string_ (s, "integer", 7)
          : a.type == MULIB_MUNE_NUMBER ? mulib_mune_rt_string_ (s, "float", 5) : mulib_mune_rt_nil_ ());
    if (!strcmp (op, "tointeger"))
      { int64_t i; return mulib_mune_result_push (out, mulib_mune_rt_int_ (a, &i) ? mulib_mune_rt_integer_ (i) : mulib_mune_rt_nil_ ()); }
    if (!strcmp (op, "ult"))
      { int64_t i, j; return mulib_mune_rt_need_int_ (s, a, &i, 1) && mulib_mune_rt_need_int_ (s, b, &j, 2) && mulib_mune_result_push (out, mulib_mune_rt_boolean_ ((uint64_t)i < (uint64_t)j)); }
    if (!strcmp (op, "randomseed"))
      {
        int64_t i = (int64_t)time (NULL), j = 0;
        if ((a.type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, a, &i, 1)) || (b.type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, b, &j, 2))) return 0;
        s->random_state = (uint64_t)i ^ ((uint64_t)j * UINT64_C (0x9e3779b97f4a7c15));
        return mulib_mune_result_push (out, mulib_mune_rt_integer_ (i)) && mulib_mune_result_push (out, mulib_mune_rt_integer_ (j));
      }
    if (!strcmp (op, "random"))
      {
        int64_t lo = 1, hi;
        uint64_t range, z, threshold;
        if (!n) return mulib_mune_result_push (out, mulib_mune_rt_number_ ((double)(mulib_mune_rt_random_ (s) >> 11) * 0x1p-53));
        if (!mulib_mune_rt_need_int_ (s, a, &hi, 1) || (n > 1 && (!mulib_mune_rt_need_int_ (s, a, &lo, 1) || !mulib_mune_rt_need_int_ (s, b, &hi, 2)))) return 0;
        if (n == 1 && hi == 0) return mulib_mune_result_push (out, mulib_mune_rt_integer_ (mulib_mune_rt_signed_ (mulib_mune_rt_random_ (s))));
        if (lo > hi) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "empty random interval");
        range = (uint64_t)hi - (uint64_t)lo + 1; threshold = range ? (UINT64_C (0) - range) % range : 0;
        do { z = mulib_mune_rt_random_ (s); } while (z < threshold);
        return mulib_mune_result_push (out, mulib_mune_rt_integer_ (mulib_mune_rt_signed_ ((uint64_t)lo + (range ? z % range : z))));
      }
    if (!strcmp (op, "min") || !strcmp (op, "max"))
      {
        size_t i;
        if (!n) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "value expected");
        for (i = 1; i < n; ++i)
          {
            if (!mulib_mune_rt_operation_ (s, !strcmp (op, "min") ? "<" : ">", args[i], a, &v)) return 0;
            if (mulib_mune_rt_truth_ (v)) a = args[i];
          }
        return mulib_mune_result_push (out, a);
      }
    if (!mulib_mune_rt_numeric_ (a, &a)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "number expected");
    x = mulib_mune_rt_double_ (a);
    if (n > 1 && b.type != MULIB_MUNE_NIL && (!strcmp (op, "atan") || !strcmp (op, "log") || !strcmp (op, "fmod")))
      { if (!mulib_mune_rt_numeric_ (b, &b)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "number expected"); y = mulib_mune_rt_double_ (b); }
    if (!strcmp (op, "abs")) v = a.type == MULIB_MUNE_INTEGER ? mulib_mune_rt_integer_ (a.as.integer < 0 ? mulib_mune_rt_signed_ (UINT64_C (0) - (uint64_t)a.as.integer) : a.as.integer) : mulib_mune_rt_number_ (fabs (x));
    else if (!strcmp (op, "floor") || !strcmp (op, "ceil"))
      { int64_t i; x = !strcmp (op, "floor") ? floor (x) : ceil (x); v = a.type == MULIB_MUNE_INTEGER ? a : mulib_mune_rt_exact_integer_ (x, &i) ? mulib_mune_rt_integer_ (i) : mulib_mune_rt_number_ (x); }
    else if (!strcmp (op, "modf"))
      {
        int64_t i; double fraction = isinf (x) ? copysign (0.0, x) : modf (x, &y);
        if (isinf (x)) y = x;
        v = a.type == MULIB_MUNE_INTEGER ? a : mulib_mune_rt_exact_integer_ (y, &i) ? mulib_mune_rt_integer_ (i) : mulib_mune_rt_number_ (y);
        return mulib_mune_result_push (out, v) && mulib_mune_result_push (out, mulib_mune_rt_number_ (a.type == MULIB_MUNE_INTEGER ? 0.0 : fraction));
      }
    else
      {
        if (!strcmp (op, "sin")) x = sin (x); else if (!strcmp (op, "cos")) x = cos (x); else if (!strcmp (op, "tan")) x = tan (x);
        else if (!strcmp (op, "asin")) x = asin (x); else if (!strcmp (op, "acos")) x = acos (x);
        else if (!strcmp (op, "atan")) x = atan2 (x, b.type != MULIB_MUNE_NIL ? y : 1.0);
        else if (!strcmp (op, "sqrt")) x = sqrt (x); else if (!strcmp (op, "exp")) x = exp (x);
        else if (!strcmp (op, "log")) x = b.type != MULIB_MUNE_NIL ? log (x) / log (y) : log (x);
        else if (!strcmp (op, "deg")) x *= 180.0 / acos (-1.0); else if (!strcmp (op, "rad")) x *= acos (-1.0) / 180.0;
        else if (!strcmp (op, "fmod"))
          {
            if (n < 2 || b.type == MULIB_MUNE_NIL) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "second number expected");
            if (a.type == MULIB_MUNE_INTEGER && b.type == MULIB_MUNE_INTEGER)
              { if (!b.as.integer) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "zero divisor"); return mulib_mune_result_push (out, mulib_mune_rt_integer_ (b.as.integer == -1 ? 0 : a.as.integer % b.as.integer)); }
            x = fmod (x, y);
          }
        v = mulib_mune_rt_number_ (x);
      }
    return mulib_mune_result_push (out, v);
  }
  static int
  mulib_mune_rt_tablelike_ (mulib_mune_state_t *s, mulib_mune_value_t value, unsigned access)
  {
    if (value.type == MULIB_MUNE_TABLE) return 1;
    if (((access & 1) && mulib_mune_rt_meta_ (s, value, "__index").type == MULIB_MUNE_NIL)
        || ((access & 2) && mulib_mune_rt_meta_ (s, value, "__newindex").type == MULIB_MUNE_NIL)
        || ((access & 4) && mulib_mune_rt_meta_ (s, value, "__len").type == MULIB_MUNE_NIL))
      return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "table or table-like value expected");
    return 1;
  }
  static int
  mulib_mune_rt_tablelib_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_value_t t = mulib_mune_rt_arg_ (args, n, 0), v, length;
    int64_t len = 0, at, end;
    int need_length = strcmp (op, "move") && (strcmp (op, "unpack") || n < 3 || args[2].type == MULIB_MUNE_NIL);
    if (!strcmp (op, "pack"))
      {
        size_t i; mulib_mune_object_t *table = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0); if (!table) return 0;
        for (i = 0; i < n; ++i) if (!mulib_mune_rt_rawset_ (s, table, mulib_mune_rt_integer_ ((int64_t)i + 1), args[i])) return 0;
        return mulib_mune_rt_setfield_ (s, table, "n", 1, mulib_mune_rt_integer_ ((int64_t)n)) && mulib_mune_result_push (out, mulib_mune_rt_object_ (table));
      }
    if (!mulib_mune_rt_tablelike_ (s, t, 1u | (need_length ? 4u : 0u)
        | (!strcmp (op, "move") || !strcmp (op, "unpack") ? 0u : 2u))) return 0;
    if (need_length && (!mulib_mune_rt_operation_ (s, "#", t, t, &length) || !mulib_mune_rt_need_int_ (s, length, &len, 1))) return 0;
    if (!strcmp (op, "unpack"))
      {
        at = 1; end = len;
        if ((n > 1 && args[1].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[1], &at, 2))
            || (n > 2 && args[2].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[2], &end, 3))) return 0;
        for (; at <= end; ++at)
          { if (!mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (at), &v) || !mulib_mune_result_push (out, v)) return 0; if (at == INT64_MAX) break; }
        return 1;
      }
    if (!strcmp (op, "insert"))
      {
        at = len == INT64_MAX ? len : len + 1;
        if (n < 2 || n > 3 || (n == 3 && !mulib_mune_rt_need_int_ (s, args[1], &at, 2))) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid table.insert arguments");
        if (len == INT64_MAX || at < 1 || at > len + 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "insert position out of bounds");
        for (end = len; end >= at; --end)
          if (!mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (end), &v) || !mulib_mune_rt_newindex_ (s, t, mulib_mune_rt_integer_ (end + 1), v)) return 0;
        return mulib_mune_rt_newindex_ (s, t, mulib_mune_rt_integer_ (at), args[n - 1]);
      }
    if (!strcmp (op, "remove"))
      {
        at = len; if (n > 1 && args[1].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[1], &at, 2)) return 0;
        if (at != len && (at < 1 || (at > len && (len == INT64_MAX || at != len + 1)))) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "remove position out of bounds");
        if (!mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (at), &v) || !mulib_mune_result_push (out, v)) return 0;
        for (end = at; end < len; ++end)
          if (!mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (end + 1), &v) || !mulib_mune_rt_newindex_ (s, t, mulib_mune_rt_integer_ (end), v)) return 0;
        return mulib_mune_rt_newindex_ (s, t, mulib_mune_rt_integer_ (end), mulib_mune_rt_nil_ ());
      }
    if (!strcmp (op, "move"))
      {
        int64_t target; uint64_t count, k;
        mulib_mune_value_t dst = n > 4 && args[4].type != MULIB_MUNE_NIL ? args[4] : t;
        if (n < 4 || !mulib_mune_rt_need_int_ (s, args[1], &at, 2) || !mulib_mune_rt_need_int_ (s, args[2], &end, 3)
            || !mulib_mune_rt_need_int_ (s, args[3], &target, 4) || !mulib_mune_rt_tablelike_ (s, dst, 2)) return 0;
        count = end < at ? 0 : (uint64_t)end - (uint64_t)at + 1;
        if (end >= at && (!count || count > s->limits.results || count - 1 > (uint64_t)INT64_MAX - (uint64_t)target))
          return mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "table.move range too large");
        for (k = 0; k < count; ++k)
          {
            uint64_t index = mulib_mune_rt_equal_ (dst, t) && target > at ? count - k - 1 : k;
            if (!mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (mulib_mune_rt_signed_ ((uint64_t)at + index)), &v)
                || !mulib_mune_rt_newindex_ (s, dst, mulib_mune_rt_integer_ (mulib_mune_rt_signed_ ((uint64_t)target + index)), v)) return 0;
          }
        return mulib_mune_result_push (out, dst);
      }
    if (!strcmp (op, "sort"))
      {
        /* In-place heapsort bounds work even with an inconsistent comparator. */
        int64_t start, size;
        mulib_mune_value_t compare = mulib_mune_rt_arg_ (args, n, 1);
        if (compare.type != MULIB_MUNE_NIL && !mulib_mune_rt_need_ (s, compare, MULIB_MUNE_FUNCTION, 2)) return 0;
        if (len < 2) return 1;
        if ((uint64_t)len > s->limits.results) return mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "sort length limit");
        for (start = len / 2, size = len; size > 1;)
          {
            int64_t root;
            mulib_mune_value_t item;
            if (start) { root = --start; if (!mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (root + 1), &item)) return 0; }
            else
              {
                --size; root = 0;
                if (!mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (size + 1), &item) || !mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (1), &v)
                    || !mulib_mune_rt_newindex_ (s, t, mulib_mune_rt_integer_ (size + 1), v)) return 0;
              }
            /* Root removed from its slot is rooted across comparator calls. */
            out->count = 0; if (!mulib_mune_result_push (out, item)) return 0;
            while (root < size / 2)
              {
                int64_t child = root * 2 + 1; mulib_mune_value_t left, right, less;
                if (!mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (child + 1), &left)) return 0;
                out->count = 1; if (!mulib_mune_result_push (out, left)) return 0;
                if (child + 1 < size)
                  {
                    if (!mulib_mune_rt_index_ (s, t, mulib_mune_rt_integer_ (child + 2), &right)) return 0;
                    if (compare.type == MULIB_MUNE_NIL) { if (!mulib_mune_rt_operation_ (s, "<", left, right, &less)) return 0; }
                    else { mulib_mune_result_t *r = mulib_mune_rt_metacall_ (s, compare, left, right, right, 2); if (!r) return 0; less = mulib_mune_result_get (r, 0); mulib_mune_result_destroy (r); }
                    if (mulib_mune_rt_truth_ (less)) { ++child; left = right; out->values[1] = left; }
                  }
                if (compare.type == MULIB_MUNE_NIL) { if (!mulib_mune_rt_operation_ (s, "<", item, left, &less)) return 0; }
                else { mulib_mune_result_t *r = mulib_mune_rt_metacall_ (s, compare, item, left, left, 2); if (!r) return 0; less = mulib_mune_result_get (r, 0); mulib_mune_result_destroy (r); }
                if (!mulib_mune_rt_truth_ (less)) break;
                if (!mulib_mune_rt_newindex_ (s, t, mulib_mune_rt_integer_ (root + 1), left)) return 0;
                root = child;
              }
            if (!mulib_mune_rt_newindex_ (s, t, mulib_mune_rt_integer_ (root + 1), item)) return 0;
          }
        out->count = 0; return 1;
      }
    return mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "unknown table function");
  }
  static int mulib_mune_rt_stringlib_ (mulib_mune_state_t *, const char *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *);
  static int mulib_mune_rt_pack_ (mulib_mune_state_t *, const char *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *);
  static int mulib_mune_rt_loadlib_ (mulib_mune_state_t *, const char *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *);
  static int mulib_mune_rt_package_ (mulib_mune_state_t *, const char *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *);
  static int mulib_mune_rt_utf8_ (mulib_mune_state_t *, const char *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *);
  static int mulib_mune_rt_os_ (mulib_mune_state_t *, const char *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *);
  static int mulib_mune_rt_io_ (mulib_mune_state_t *, const char *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *);
  static int mulib_mune_rt_io_init_ (mulib_mune_state_t *);
  static int mulib_mune_rt_coroutine_ (mulib_mune_state_t *, const char *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *);
  static int mulib_mune_rt_debug_ (mulib_mune_state_t *, const char *, const mulib_mune_value_t *, size_t, mulib_mune_result_t *);
  static mulib_mune_status_t
  mulib_mune_rt_builtin_ (mulib_mune_state_t *s, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out, void *user)
  {
    const char *op = (const char *)user;
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0), b = mulib_mune_rt_arg_ (args, n, 1), v;
    size_t k;
    if (!strncmp (op, "string.arith.", 13))
      {
        mulib_mune_value_t x, y, method;
        const char *name = op + 13;
        const char *operator = !strcmp (name, "__add") ? "+" : !strcmp (name, "__sub") ? "-" : !strcmp (name, "__mul") ? "*"
            : !strcmp (name, "__div") ? "/" : !strcmp (name, "__idiv") ? "//" : !strcmp (name, "__mod") ? "%" : !strcmp (name, "__pow") ? "^" : "unm";
        if (mulib_mune_rt_numeric_ (a, &x) && mulib_mune_rt_numeric_ (b, &y))
          { if (mulib_mune_rt_operation_ (s, operator, x, y, &v)) (void)mulib_mune_result_push (out, v); }
        else if (b.type != MULIB_MUNE_STRING && (method = mulib_mune_rt_meta_ (s, b, name)).type != MULIB_MUNE_NIL
                 && !mulib_mune_rt_equal_ (method, s->frame->function))
          {
            mulib_mune_result_t *r = mulib_mune_rt_metacall_ (s, method, a, b, b, 2);
            if (r) { (void)mulib_mune_result_push (out, mulib_mune_result_get (r, 0)); mulib_mune_result_destroy (r); }
          }
        else mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid arithmetic operands");
        return s->status;
      }
    if (!strncmp (op, "math.", 5)) { (void)mulib_mune_rt_math_ (s, op + 5, args, n, out); return s->status; }
    if (!strncmp (op, "table.", 6) && strcmp (op, "table.concat")) { (void)mulib_mune_rt_tablelib_ (s, op + 6, args, n, out); return s->status; }
    if (!strncmp (op, "string.", 7) || !strcmp (op, "table.concat")) { (void)mulib_mune_rt_stringlib_ (s, op, args, n, out); return s->status; }
    if (!strncmp (op, "package.", 8) || !strcmp (op, "require")) { (void)mulib_mune_rt_package_ (s, op, args, n, out); return s->status; }
    if (!strncmp (op, "utf8.", 5)) { (void)mulib_mune_rt_utf8_ (s, op + 5, args, n, out); return s->status; }
    if (!strncmp (op, "os.", 3)) { (void)mulib_mune_rt_os_ (s, op + 3, args, n, out); return s->status; }
    if (!strncmp (op, "io.", 3) || !strncmp (op, "file.", 5)) { (void)mulib_mune_rt_io_ (s, op, args, n, out); return s->status; }
    if (!strncmp (op, "coroutine.", 10)) { (void)mulib_mune_rt_coroutine_ (s, op + 10, args, n, out); return s->status; }
    if (!strncmp (op, "debug.", 6)) { (void)mulib_mune_rt_debug_ (s, op + 6, args, n, out); return s->status; }
    if (!strcmp (op, "assert"))
      {
        if (!mulib_mune_rt_truth_ (a)) (void)mulib_mune_rt_raise_level_ (s, n > 1 ? b : mulib_mune_rt_string_ (s, "assertion failed!", 17), 1);
        else for (k = 0; k < n; ++k) if (!mulib_mune_result_push (out, args[k])) break;
      }
    else if (!strcmp (op, "error"))
      { int64_t level = 1; if (b.type == MULIB_MUNE_NIL || mulib_mune_rt_need_int_ (s, b, &level, 2)) (void)mulib_mune_rt_raise_level_ (s, a, level); }
    else if (!strcmp (op, "type")) { if (!n) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "value expected"); else (void)mulib_mune_result_push (out, mulib_mune_rt_string_ (s, mulib_mune_rt_typename_ (a), strlen (mulib_mune_rt_typename_ (a)))); }
    else if (!strcmp (op, "tostring")) { if (!n) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "value expected"); else if (mulib_mune_rt_text_ (s, a, 1, &v)) (void)mulib_mune_result_push (out, v); }
    else if (!strcmp (op, "tonumber"))
      {
        if (!n) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "value expected"), s->status);
        if (b.type == MULIB_MUNE_NIL) { if (!mulib_mune_rt_numeric_ (a, &v)) v = mulib_mune_rt_nil_ (); (void)mulib_mune_result_push (out, v); }
        else
          {
            int64_t base; size_t at = 0, digits = 0; uint64_t value = 0; int negative = 0;
            const char *text; size_t length;
            if (!mulib_mune_rt_need_int_ (s, b, &base, 2) || !mulib_mune_rt_need_ (s, a, MULIB_MUNE_STRING, 1)) return s->status;
            if (base < 2 || base > 36) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "base out of range"), s->status);
            text = mulib_mune_value_string (a, &length);
            while (at < length && mulib_mune_space_ ((unsigned char)text[at])) ++at;
            if (at < length && (text[at] == '+' || text[at] == '-')) negative = text[at++] == '-';
            for (; at < length; ++at)
              {
                unsigned char c = (unsigned char)text[at]; int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'z' ? c - 'a' + 10 : c >= 'A' && c <= 'Z' ? c - 'A' + 10 : -1;
                if (d < 0 || d >= base) break;
                value = value * (uint64_t)base + (unsigned)d; ++digits;
              }
            while (at < length && mulib_mune_space_ ((unsigned char)text[at])) ++at;
            v = digits && at == length ? mulib_mune_rt_integer_ (mulib_mune_rt_signed_ (negative ? UINT64_C (0) - value : value)) : mulib_mune_rt_nil_ ();
            (void)mulib_mune_result_push (out, v);
          }
      }
    else if (!strcmp (op, "select"))
      {
        int64_t at;
        if (a.type == MULIB_MUNE_STRING && a.as.object->as.length == 1 && *(const char *)(a.as.object + 1) == '#')
          (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ (n ? (int64_t)n - 1 : 0));
        else if (mulib_mune_rt_need_int_ (s, a, &at, 1))
          {
            if (at < 0) at += (int64_t)n;
            if (at < 1) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "select index out of range");
            else for (k = (size_t)at; k < n; ++k) if (!mulib_mune_result_push (out, args[k])) break;
          }
      }
    else if (!strcmp (op, "rawequal")) { if (n < 2) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "two values expected"); else (void)mulib_mune_result_push (out, mulib_mune_rt_boolean_ (mulib_mune_rt_equal_ (a, b))); }
    else if (!strcmp (op, "rawget") || !strcmp (op, "rawset"))
      {
        if (n < 2) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "key expected"), s->status);
        if (mulib_mune_rt_need_ (s, a, MULIB_MUNE_TABLE, 1))
          {
            if (!strcmp (op, "rawget")) (void)mulib_mune_result_push (out, mulib_mune_rt_rawget_ (a.as.object, b));
            else if (n < 3) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "third argument expected");
            else if (mulib_mune_rt_rawset_ (s, a.as.object, b, args[2])) (void)mulib_mune_result_push (out, a);
          }
      }
    else if (!strcmp (op, "rawlen"))
      {
        if (a.type == MULIB_MUNE_STRING) v = mulib_mune_rt_integer_ ((int64_t)a.as.object->as.length);
        else if (mulib_mune_rt_need_ (s, a, MULIB_MUNE_TABLE, 1))
          { mulib_mune_object_t *meta = a.as.object->metatable; a.as.object->metatable = NULL; (void)mulib_mune_rt_operation_ (s, "#", a, a, &v); a.as.object->metatable = meta; }
        else return s->status;
        (void)mulib_mune_result_push (out, v);
      }
    else if (!strcmp (op, "getmetatable"))
      {
        mulib_mune_object_t *meta = a.type >= MULIB_MUNE_STRING ? a.as.object->metatable : NULL;
        if (!n) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "value expected"), s->status);
        if (!meta && a.type != MULIB_MUNE_TABLE && (a.type != MULIB_MUNE_USERDATA || a.as.object->userdata_tag == 3))
          meta = s->type_metatables[a.type == MULIB_MUNE_INTEGER ? MULIB_MUNE_NUMBER : a.type];
        v = mulib_mune_rt_field_ (meta, "__metatable", 11);
        (void)mulib_mune_result_push (out, v.type == MULIB_MUNE_NIL ? mulib_mune_rt_object_ (meta) : v);
      }
    else if (!strcmp (op, "setmetatable"))
      {
        if (n < 2) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "metatable expected"), s->status);
        if (!mulib_mune_rt_need_ (s, a, MULIB_MUNE_TABLE, 1) || (b.type != MULIB_MUNE_NIL && !mulib_mune_rt_need_ (s, b, MULIB_MUNE_TABLE, 2))) return s->status;
        if (mulib_mune_rt_field_ (a.as.object->metatable, "__metatable", 11).type != MULIB_MUNE_NIL) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "cannot change a protected metatable");
        else
          {
            a.as.object->metatable = b.type == MULIB_MUNE_NIL ? NULL : b.as.object;
            mulib_mune_rt_finalize_register_ (s, a.as.object);
            (void)mulib_mune_result_push (out, a);
          }
      }
    else if (!strcmp (op, "next")) (void)mulib_mune_rt_next_ (s, a, b, out);
    else if (!strcmp (op, "pairs"))
      {
        v = mulib_mune_rt_meta_ (s, a, "__pairs");
        if (v.type != MULIB_MUNE_NIL)
          { mulib_mune_result_t *r = mulib_mune_rt_metacall_ (s, v, a, a, a, 1); if (r) { for (k = 0; k < 3; ++k) (void)mulib_mune_result_push (out, mulib_mune_result_get (r, k)); mulib_mune_result_destroy (r); } }
        else if (mulib_mune_rt_need_ (s, a, MULIB_MUNE_TABLE, 1))
          { (void)mulib_mune_result_push (out, mulib_mune_rt_field_ (s->globals, "next", 4)); (void)mulib_mune_result_push (out, a); (void)mulib_mune_result_push (out, mulib_mune_rt_nil_ ()); }
      }
    else if (!strcmp (op, "ipairs"))
      {
        mulib_mune_object_t *f;
        if (!n) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "value expected"), s->status);
        f = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0);
        if (f) { f->as.function.native = mulib_mune_rt_builtin_; f->as.function.user = (void *)"ipairs.iterator";
          (void)mulib_mune_result_push (out, mulib_mune_rt_object_ (f)); (void)mulib_mune_result_push (out, a); (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ (0)); }
      }
    else if (!strcmp (op, "ipairs.iterator"))
      {
        int64_t i;
        if (mulib_mune_rt_need_int_ (s, b, &i, 2))
          {
            v = mulib_mune_rt_integer_ (mulib_mune_rt_signed_ ((uint64_t)i + 1));
            if (mulib_mune_rt_index_ (s, a, v, &b) && b.type != MULIB_MUNE_NIL) { (void)mulib_mune_result_push (out, v); (void)mulib_mune_result_push (out, b); }
          }
      }
    else if (!strcmp (op, "pcall") || !strcmp (op, "xpcall"))
      {
        size_t start = !strcmp (op, "xpcall") ? 2 : 1;
        mulib_mune_protected_ protect;
        mulib_mune_result_t *callargs, *r;
        if (!n) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "function expected"), s->status);
        callargs = mulib_mune_rt_results_ (s);
        if (!callargs) return s->status;
        if (start == 2 && !mulib_mune_rt_need_ (s, b, MULIB_MUNE_FUNCTION, 2)) { mulib_mune_result_destroy (callargs); return s->status; }
        protect.previous = s->protected_call; protect.handler = start == 2 ? b : mulib_mune_rt_nil_ (); protect.handled = 0; s->protected_call = &protect;
        for (k = start; k < n; ++k) (void)mulib_mune_result_push (callargs, args[k]);
        r = mulib_mune_rt_call_ (s, a, callargs); mulib_mune_result_destroy (callargs);
        if (s->status) mulib_mune_rt_handle_error_ (s);
        s->protected_call = protect.previous;
        if (s->current_thread && s->current_thread->as.thread->close_requested && !s->current_thread->as.thread->closing_callbacks)
          { mulib_mune_result_destroy (r); return s->status; }
        if (r) { (void)mulib_mune_result_push (out, mulib_mune_rt_boolean_ (1)); (void)mulib_mune_rt_append_ (out, r, 0); mulib_mune_result_destroy (r); }
        else
          {
            s->status = MULIB_MUNE_OK; v = s->error_value;
            if (!s->has_error) v = mulib_mune_rt_string_ (s, s->message, strlen (s->message));
            (void)mulib_mune_result_push (out, mulib_mune_rt_boolean_ (0)); (void)mulib_mune_result_push (out, v);
            s->error_value = mulib_mune_rt_nil_ (); s->has_error = 0;
          }
      }
    else if (!strcmp (op, "print") || !strcmp (op, "warn"))
      {
        FILE *file = !strcmp (op, "warn") ? stderr : stdout;
        if (file == stderr)
          {
            if (!n) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "warning string expected"), s->status);
            for (k = 0; k < n; ++k) if (!mulib_mune_rt_need_ (s, args[k], MULIB_MUNE_STRING, k + 1)) return s->status;
            if (n == 1 && a.as.object->as.length && *(const char *)(a.as.object + 1) == '@')
              {
                if (a.as.object->as.length == 3 && !memcmp (a.as.object + 1, "@on", 3)) s->warnings = 1;
                if (a.as.object->as.length == 4 && !memcmp (a.as.object + 1, "@off", 4)) s->warnings = 0;
                return s->status;
              }
            if (!s->warnings) return s->status;
          }
        for (k = 0; k < n; ++k)
          {
            if (!mulib_mune_rt_text_ (s, args[k], 1, &v)) break;
            if (k && file == stdout) fputc ('\t', file);
            if (fwrite (v.as.object + 1, 1, v.as.object->as.length, file) != v.as.object->as.length) { mulib_mune_rt_error_ (s, MULIB_MUNE_IO, "output failed"); break; }
          }
        if (!s->status && fputc ('\n', file) == EOF) mulib_mune_rt_error_ (s, MULIB_MUNE_IO, "output failed");
      }
    else if (!strcmp (op, "collectgarbage"))
      {
        const char *what = a.type == MULIB_MUNE_NIL ? "collect" : mulib_mune_value_string (a, NULL);
        if (!what) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "collection option expected");
        else if (!strcmp (what, "count")) (void)mulib_mune_result_push (out, mulib_mune_rt_number_ ((double)s->bytes / 1024));
        else if (!strcmp (what, "isrunning")) (void)mulib_mune_result_push (out, mulib_mune_rt_boolean_ (!s->gc_stopped));
        else if (!strcmp (what, "stop")) { s->gc_stopped = 1; s->threshold = SIZE_MAX; (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ (0)); }
        else if (!strcmp (what, "restart")) { s->gc_stopped = 0; s->threshold = s->bytes; (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ (0)); }
        else if (!strcmp (what, "collect"))
          { (void)mulib_mune_collect (s); (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ (0)); }
        else if (!strcmp (what, "step"))
          {
            int64_t amount = 0; size_t budget;
            if (b.type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, b, &amount, 2)) return s->status;
            if (amount < 0) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "negative collection step"), s->status);
            budget = (uint64_t)amount > SIZE_MAX / 1024 ? SIZE_MAX : amount ? (size_t)amount * 1024 : 1024;
            budget = mulib_mune_rt_gc_scale_ (budget, (unsigned)s->gc_stepmul);
            (void)mulib_mune_result_push (out, mulib_mune_rt_boolean_ (mulib_mune_rt_gc_step_ (s, budget)));
          }
        else if (!strcmp (what, "setpause") || !strcmp (what, "setstepmul"))
          {
            int64_t value; int *field = !strcmp (what, "setpause") ? &s->gc_pause : &s->gc_stepmul;
            if (!mulib_mune_rt_need_int_ (s, b, &value, 2) || value < 0 || value > INT_MAX) return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid collector parameter"), s->status);
            (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ (*field)); *field = (int)value;
          }
        else if (!strcmp (what, "incremental") || !strcmp (what, "generational"))
          {
            int parameters[3] = { 0, 0, 0 }, generational = !strcmp (what, "generational");
            for (k = 1; k < n && k <= (generational ? 2u : 3u); ++k)
              {
                int64_t value = 0;
                if (args[k].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[k], &value, k + 1)) return s->status;
                if (value < 0 || value > INT_MAX || (!generational && k == 3 && value > 63))
                  return (mulib_mune_status_t)(mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid collector parameter"), s->status);
                parameters[k - 1] = (int)value;
              }
            (void)mulib_mune_result_push (out, mulib_mune_rt_string_ (s, s->gc_generation ? "generational" : "incremental", s->gc_generation ? 12 : 11));
            if (s->collecting) return s->status;
            if (s->gc_sweep) (void)mulib_mune_rt_gc_sweep_ (s, SIZE_MAX);
            s->gc_generation = generational;
            if (generational)
              { if (parameters[0]) s->gc_minor = parameters[0]; if (parameters[1]) s->gc_major = parameters[1]; }
            else
              { if (parameters[0]) s->gc_pause = parameters[0]; if (parameters[1]) s->gc_stepmul = parameters[1]; if (parameters[2]) s->gc_stepsize = parameters[2]; }
            if (!s->gc_stopped) s->threshold = s->bytes;
          }
        else mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "unknown collection option");
      }
    else if (!strcmp (op, "load") || !strcmp (op, "loadfile") || !strcmp (op, "dofile"))
      (void)mulib_mune_rt_loadlib_ (s, op, args, n, out);
    else mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "unknown library function %s", op);
    return s->status;
  }
  static int
  mulib_mune_rt_library_ (mulib_mune_state_t *s)
  {
    static const char *const base[] = { "assert", "error", "type", "tostring", "tonumber", "select", "rawequal", "rawget", "rawset", "rawlen", "getmetatable", "setmetatable", "next", "pairs", "ipairs", "pcall", "xpcall", "print", "warn", "collectgarbage", "load", "loadfile", "dofile" };
    static const char *const methods[] = {
      "math.abs", "math.acos", "math.asin", "math.atan", "math.ceil", "math.cos", "math.deg", "math.exp", "math.floor", "math.fmod", "math.log", "math.max", "math.min", "math.modf", "math.rad", "math.random", "math.randomseed", "math.sin", "math.sqrt", "math.tan", "math.tointeger", "math.type", "math.ult",
      "table.concat", "table.insert", "table.move", "table.pack", "table.remove", "table.sort", "table.unpack",
      "string.byte", "string.char", "string.len", "string.lower", "string.upper", "string.rep", "string.reverse", "string.sub", "string.format", "string.find", "string.match", "string.gmatch", "string.gsub", "string.pack", "string.packsize", "string.unpack", "string.dump",
      "package.searchpath", "package.loadlib", "utf8.char", "utf8.codes", "utf8.codepoint", "utf8.len", "utf8.offset",
      "os.clock", "os.date", "os.difftime", "os.execute", "os.exit", "os.getenv", "os.remove", "os.rename", "os.setlocale", "os.time", "os.tmpname",
      "io.open", "io.close", "io.flush", "io.input", "io.output", "io.lines", "io.read", "io.write", "io.tmpfile", "io.type", "io.popen",
      "coroutine.create", "coroutine.resume", "coroutine.yield", "coroutine.status", "coroutine.running", "coroutine.isyieldable", "coroutine.close", "coroutine.wrap",
      "debug.debug", "debug.gethook", "debug.sethook", "debug.getinfo", "debug.getlocal", "debug.setlocal", "debug.getregistry", "debug.getmetatable", "debug.setmetatable", "debug.getupvalue", "debug.setupvalue", "debug.upvalueid", "debug.upvaluejoin", "debug.getuservalue", "debug.setuservalue", "debug.traceback"
    };
    size_t k;
    for (k = 0; k < sizeof (base) / sizeof (*base); ++k) if (!mulib_mune_rt_install_ (s, s->globals, base[k], base[k])) return 0;
    for (k = 0; k < sizeof (methods) / sizeof (*methods); ++k)
      {
        const char *dot = strchr (methods[k], '.'); size_t length = (size_t)(dot - methods[k]);
        mulib_mune_value_t table = mulib_mune_rt_field_ (s->globals, methods[k], length);
        if (table.type == MULIB_MUNE_NIL)
          { table = mulib_mune_rt_object_ (mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0)); if (s->status || !mulib_mune_rt_setfield_ (s, s->globals, methods[k], length, table)) return 0; }
        if (!mulib_mune_rt_install_ (s, table.as.object, dot + 1, methods[k])) return 0;
      }
    {
      mulib_mune_value_t math = mulib_mune_rt_field_ (s->globals, "math", 4), string = mulib_mune_rt_field_ (s->globals, "string", 6);
      mulib_mune_object_t *meta = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0); if (!meta) return 0;
      s->type_metatables[MULIB_MUNE_STRING] = meta;
      {
        static const char *const names[] = { "__add", "__sub", "__mul", "__div", "__idiv", "__mod", "__pow", "__unm" };
        static const char *const ops[] = { "string.arith.__add", "string.arith.__sub", "string.arith.__mul", "string.arith.__div",
          "string.arith.__idiv", "string.arith.__mod", "string.arith.__pow", "string.arith.__unm" };
        for (k = 0; k < sizeof (names) / sizeof (*names); ++k) if (!mulib_mune_rt_install_ (s, meta, names[k], ops[k])) return 0;
      }
      if (!mulib_mune_rt_setfield_ (s, meta, "__index", 7, string)
          || !mulib_mune_rt_setfield_ (s, math.as.object, "pi", 2, mulib_mune_rt_number_ (acos (-1)))
          || !mulib_mune_rt_setfield_ (s, math.as.object, "huge", 4, mulib_mune_rt_number_ (HUGE_VAL))
          || !mulib_mune_rt_setfield_ (s, math.as.object, "mininteger", 10, mulib_mune_rt_integer_ (INT64_MIN))
          || !mulib_mune_rt_setfield_ (s, math.as.object, "maxinteger", 10, mulib_mune_rt_integer_ (INT64_MAX))) return 0;
    }
    {
      mulib_mune_value_t package = mulib_mune_rt_field_ (s->globals, "package", 7), utf8 = mulib_mune_rt_field_ (s->globals, "utf8", 4);
      mulib_mune_object_t *preload = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0), *loaded = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0), *searchers = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0);
      static const char charpattern[] = "[\0-\x7f\xc2-\xfd][\x80-\xbf]*";
      mulib_mune_value_t loader;
      if (!preload || !loaded || !searchers
          || !mulib_mune_rt_setfield_ (s, loaded, "_G", 2, mulib_mune_rt_object_ (s->globals))
          || !mulib_mune_rt_setfield_ (s, package.as.object, "preload", 7, mulib_mune_rt_object_ (preload))
          || !mulib_mune_rt_setfield_ (s, package.as.object, "loaded", 6, mulib_mune_rt_object_ (loaded))
          || !mulib_mune_rt_setfield_ (s, package.as.object, "searchers", 9, mulib_mune_rt_object_ (searchers))
          || !mulib_mune_rt_setfield_ (s, package.as.object, "path", 4, mulib_mune_rt_string_ (s, "./?.lua;./?/init.lua;./?.luac", sizeof ("./?.lua;./?/init.lua;./?.luac") - 1))
          || !mulib_mune_rt_setfield_ (s, package.as.object, "cpath", 5, mulib_mune_rt_string_ (s, "./?.so", 6))
          || !mulib_mune_rt_setfield_ (s, package.as.object, "config", 6, mulib_mune_rt_string_ (s, "/\n;\n?\n!\n-\n", 10))
          || !mulib_mune_rt_install_ (s, package.as.object, "_preload_searcher", "package.preload_searcher")
          || !mulib_mune_rt_install_ (s, package.as.object, "_file_searcher", "package.file_searcher")
          || !mulib_mune_rt_install_ (s, package.as.object, "_native_searcher", "package.native_searcher")
          || !mulib_mune_rt_install_ (s, s->globals, "require", "require")
          || !mulib_mune_rt_setfield_ (s, utf8.as.object, "charpattern", 11, mulib_mune_rt_string_ (s, charpattern, sizeof (charpattern) - 1))) return 0;
      loader = mulib_mune_rt_field_ (package.as.object, "_preload_searcher", 17);
      if (!mulib_mune_rt_rawset_ (s, searchers, mulib_mune_rt_integer_ (1), loader)) return 0;
      loader = mulib_mune_rt_field_ (package.as.object, "_file_searcher", 14);
      if (!mulib_mune_rt_rawset_ (s, searchers, mulib_mune_rt_integer_ (2), loader)) return 0;
      loader = mulib_mune_rt_field_ (package.as.object, "_native_searcher", 16);
      if (!mulib_mune_rt_rawset_ (s, searchers, mulib_mune_rt_integer_ (3), loader)) return 0;
      for (k = 0; k < s->globals->as.table.capacity; ++k)
        {
          const mulib_mune_entry_ *e = &s->globals->as.table.entries[k];
          if (e->used == 1 && e->value.type == MULIB_MUNE_TABLE && !mulib_mune_rt_rawset_ (s, loaded, e->key, e->value)) return 0;
        }
    }
    return mulib_mune_rt_io_init_ (s) && mulib_mune_rt_setfield_ (s, s->globals, "_G", 2, mulib_mune_rt_object_ (s->globals))
           && mulib_mune_rt_setfield_ (s, s->globals, "_VERSION", 8, mulib_mune_rt_string_ (s, "Lua 5.4", 7));
  }
  typedef struct mulib_mune_bytes_
  {
    char *data; size_t length, capacity;
  } mulib_mune_bytes_;
  static int
  mulib_mune_rt_bytes_ (mulib_mune_state_t *s, mulib_mune_bytes_ *b, const char *text, size_t n)
  {
    if (n > SIZE_MAX - b->length - 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "string size overflow");
    if (b->length + n + 1 > b->capacity)
      {
        size_t cap = b->capacity > SIZE_MAX / 2 ? SIZE_MAX : b->capacity * 2;
        char *p;
        if (cap < 64) cap = 64;
        if (cap < b->length + n + 1) cap = b->length + n + 1;
        p = (char *)mulib_mune_rt_memory_ (s, b->data, b->capacity, cap); if (!p) return 0;
        b->data = p; b->capacity = cap;
      }
    if (n) memcpy (b->data + b->length, text, n);
    b->length += n; b->data[b->length] = 0; return 1;
  }
  static int64_t
  mulib_mune_rt_relative_ (int64_t i, size_t length)
  {
    if (i >= 0) return i;
    if (i < -(int64_t)length) return 0;
    return (int64_t)length + i + 1;
  }
  static int
  mulib_mune_rt_quote_ (mulib_mune_state_t *s, mulib_mune_bytes_ *out, mulib_mune_value_t value)
  {
    size_t k;
    char text[128]; int n;
    if (value.type != MULIB_MUNE_STRING)
      {
        if (value.type == MULIB_MUNE_NUMBER)
          n = isnan (value.as.number) ? snprintf (text, sizeof (text), "(0/0)")
              : isinf (value.as.number) ? snprintf (text, sizeof (text), "(%s1/0)", signbit (value.as.number) ? "-" : "")
              : snprintf (text, sizeof (text), "%a", value.as.number);
        else if (value.type == MULIB_MUNE_INTEGER)
          n = value.as.integer == INT64_MIN ? snprintf (text, sizeof (text), "0x8000000000000000") : snprintf (text, sizeof (text), "%" PRId64, value.as.integer);
        else if (value.type == MULIB_MUNE_BOOLEAN) n = snprintf (text, sizeof (text), "%s", value.as.boolean ? "true" : "false");
        else if (value.type == MULIB_MUNE_NIL) n = snprintf (text, sizeof (text), "nil");
        else return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "value has no literal form");
        return n >= 0 && (size_t)n < sizeof (text) && mulib_mune_rt_bytes_ (s, out, text, (size_t)n);
      }
    if (!mulib_mune_rt_bytes_ (s, out, "\"", 1)) return 0;
    for (k = 0; k < value.as.object->as.length; ++k)
      {
        unsigned char c = ((const unsigned char *)(value.as.object + 1))[k];
        if (c == '\\' || c == '"' || c == '\n')
          { text[0] = '\\'; text[1] = (char)c; n = 2; }
        else if (c < 32 || c == 127) n = snprintf (text, sizeof (text), "\\%03u", c);
        else { text[0] = (char)c; n = 1; }
        if (!mulib_mune_rt_bytes_ (s, out, text, (size_t)n)) return 0;
      }
    return mulib_mune_rt_bytes_ (s, out, "\"", 1);
  }
  static int
  mulib_mune_rt_format_ (mulib_mune_state_t *s, mulib_mune_value_t format, const mulib_mune_value_t *args, size_t n, mulib_mune_bytes_ *out)
  {
    const char *fmt = (const char *)(format.as.object + 1);
    size_t length = format.as.object->as.length, at = 0, argument = 1;
    while (at < length)
      {
        char spec[32], buffer[1024], code;
        size_t begin, used, flags_end, width = 0, precision = 0, precision_start;
        int written;
        int64_t integer;
        mulib_mune_value_t v;
        if (fmt[at] != '%') { if (!mulib_mune_rt_bytes_ (s, out, fmt + at++, 1)) return 0; continue; }
        begin = at++;
        if (at < length && fmt[at] == '%') { ++at; if (!mulib_mune_rt_bytes_ (s, out, "%", 1)) return 0; continue; }
        while (at < length && fmt[at] && strchr ("-+ #0", fmt[at])) ++at;
        flags_end = at;
        while (at < length && mulib_mune_digit_ ((unsigned char)fmt[at]))
          { width = width * 10 + (unsigned)(fmt[at++] - '0'); if (width > 99) break; }
        precision_start = at;
        if (at < length && fmt[at] == '.')
          { ++at; while (at < length && mulib_mune_digit_ ((unsigned char)fmt[at])) { precision = precision * 10 + (unsigned)(fmt[at++] - '0'); if (precision > 99) break; } }
        if (at == length || width > 99 || precision > 99 || at - begin > 20 || argument == n)
          return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid format or missing argument");
        code = fmt[at++]; used = at - begin - 1; memcpy (spec, fmt + begin, used); spec[used] = 0; v = args[argument++];
        {
          size_t k;
          const char *allowed = code == 's' || code == 'c' ? "-" : code == 'd' || code == 'i' ? "-+ 0" : code == 'u' ? "-0" : code == 'o' || code == 'x' || code == 'X' ? "-#0" : "-+ #0";
          for (k = begin + 1; k < flags_end; ++k) if (!strchr (allowed, fmt[k])) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid conversion flags");
          if (code == 'c' && precision_start < at - 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "precision is invalid for c");
        }
        if (code == 'q')
          { if (used != 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "q does not accept modifiers"); if (!mulib_mune_rt_quote_ (s, out, v)) return 0; continue; }
        if (code == 's')
          {
            size_t len; const char *text;
            if (!mulib_mune_rt_text_ (s, v, 1, &v)) return 0;
            text = mulib_mune_value_string (v, &len);
            if (used == 1) { if (!mulib_mune_rt_bytes_ (s, out, text, len)) return 0; continue; }
            if (memchr (text, 0, len)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "modified s format cannot contain zeros");
            spec[used++] = code; spec[used] = 0; written = snprintf (buffer, sizeof (buffer), spec, text);
          }
        else if (code == 'c')
          { if (!mulib_mune_rt_need_int_ (s, v, &integer, argument)) return 0; spec[used++] = code; spec[used] = 0; written = snprintf (buffer, sizeof (buffer), spec, (int)(unsigned char)integer); }
        else if (code && strchr ("diouxX", code))
          {
            if (!mulib_mune_rt_need_int_ (s, v, &integer, argument)) return 0;
            spec[used++] = 'j'; spec[used++] = code; spec[used] = 0;
            written = code == 'd' || code == 'i' ? snprintf (buffer, sizeof (buffer), spec, (intmax_t)integer)
                        : snprintf (buffer, sizeof (buffer), spec, (uintmax_t)(uint64_t)integer);
          }
        else if (code && strchr ("aAeEfgG", code))
          {
            if (!mulib_mune_rt_numeric_ (v, &v)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "number expected in format");
            spec[used++] = code; spec[used] = 0; written = snprintf (buffer, sizeof (buffer), spec, mulib_mune_rt_double_ (v));
          }
        else return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid format specifier");
        if (written < 0 || (size_t)written >= sizeof (buffer)) return mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "formatted value too long");
        if (!mulib_mune_rt_bytes_ (s, out, buffer, (size_t)written)) return 0;
      }
    return 1;
  }
  typedef struct mulib_mune_capture_
  {
    size_t start, length;
  } mulib_mune_capture_;
  typedef struct mulib_mune_pattern_
  {
    mulib_mune_state_t *state;
    const unsigned char *text, *pattern;
    size_t length, pattern_length, captures;
    unsigned depth;
    mulib_mune_capture_ capture[32];
  } mulib_mune_pattern_;
  static int
  mulib_mune_rt_class_ (unsigned char c, unsigned char pattern)
  {
    int value;
    switch (tolower (pattern))
      {
      case 'a': value = isalpha (c); break; case 'c': value = iscntrl (c); break;
      case 'd': value = isdigit (c); break; case 'g': value = isgraph (c); break;
      case 'l': value = islower (c); break; case 'p': value = ispunct (c); break;
      case 's': value = isspace (c); break; case 'u': value = isupper (c); break;
      case 'w': value = isalnum (c); break; case 'x': value = isxdigit (c); break;
      case 'z': value = c == 0; break; default: return c == pattern;
      }
    return isupper (pattern) ? !value : !!value;
  }
  static size_t
  mulib_mune_rt_class_end_ (mulib_mune_pattern_ *m, size_t p)
  {
    const unsigned char *pattern = m->pattern;
    if (pattern[p] == '%')
      {
        if (p + 1 < m->pattern_length) return p + 2;
        mulib_mune_rt_error_ (m->state, MULIB_MUNE_RUNTIME, "pattern ends with percent"); return SIZE_MAX;
      }
    if (pattern[p] == '[')
      {
        ++p; if (p < m->pattern_length && pattern[p] == '^') ++p;
        if (p < m->pattern_length && pattern[p] == ']') ++p;
        while (p < m->pattern_length)
          {
            if (pattern[p] == ']') return p + 1;
            if (pattern[p++] == '%' && p < m->pattern_length) ++p;
          }
        mulib_mune_rt_error_ (m->state, MULIB_MUNE_RUNTIME, "unfinished pattern class"); return SIZE_MAX;
      }
    return p + 1;
  }
  static int
  mulib_mune_rt_class_match_ (mulib_mune_pattern_ *m, unsigned char c, size_t p, size_t end)
  {
    const unsigned char *pattern = m->pattern;
    int negate = 0, matched = 0;
    if (pattern[p] == '.') return 1;
    if (pattern[p] == '%') return mulib_mune_rt_class_ (c, pattern[p + 1]);
    if (pattern[p] != '[') return c == pattern[p];
    ++p; --end;
    if (p < end && pattern[p] == '^') { negate = 1; ++p; }
    while (p < end)
      {
        if (pattern[p] == '%' && p + 1 < end) { matched |= mulib_mune_rt_class_ (c, pattern[p + 1]); p += 2; }
        else if (p + 2 < end && pattern[p + 1] == '-') { matched |= c >= pattern[p] && c <= pattern[p + 2]; p += 3; }
        else matched |= c == pattern[p++];
      }
    return negate ? !matched : matched;
  }
  static size_t mulib_mune_rt_match_ (mulib_mune_pattern_ *, size_t, size_t);
  static size_t
  mulib_mune_rt_match_body_ (mulib_mune_pattern_ *m, size_t at, size_t p)
  {
    while (p < m->pattern_length)
      {
        unsigned char c = m->pattern[p]; size_t end, next;
        int matches;
        if (m->state->steps == m->state->limits.steps)
          { mulib_mune_rt_error_ (m->state, MULIB_MUNE_LIMIT, "pattern work limit"); return SIZE_MAX; }
        ++m->state->steps;
        if (c == '(')
          {
            size_t index = m->captures;
            if (index == 32) { mulib_mune_rt_error_ (m->state, MULIB_MUNE_LIMIT, "too many captures"); return SIZE_MAX; }
            ++m->captures; m->capture[index].start = at;
            m->capture[index].length = p + 1 < m->pattern_length && m->pattern[p + 1] == ')' ? SIZE_MAX - 1 : SIZE_MAX;
            next = mulib_mune_rt_match_ (m, at, p + (m->capture[index].length == SIZE_MAX - 1 ? 2 : 1));
            if (next == SIZE_MAX) m->captures = index;
            return next;
          }
        if (c == ')')
          {
            size_t index = m->captures;
            while (index && m->capture[index - 1].length != SIZE_MAX) --index;
            if (!index) { mulib_mune_rt_error_ (m->state, MULIB_MUNE_RUNTIME, "invalid capture close"); return SIZE_MAX; }
            --index; m->capture[index].length = at - m->capture[index].start;
            next = mulib_mune_rt_match_ (m, at, p + 1);
            if (next == SIZE_MAX) m->capture[index].length = SIZE_MAX;
            return next;
          }
        if (c == '$' && p + 1 == m->pattern_length) return at == m->length ? at : SIZE_MAX;
        if (c == '%' && p + 1 < m->pattern_length)
          {
            unsigned char special = m->pattern[p + 1];
            if (special == 'b')
              {
                size_t depth = 1;
                if (p + 3 >= m->pattern_length) { mulib_mune_rt_error_ (m->state, MULIB_MUNE_RUNTIME, "balanced pattern needs two bytes"); return SIZE_MAX; }
                if (at == m->length || m->text[at] != m->pattern[p + 2]) return SIZE_MAX;
                for (++at; at < m->length; ++at)
                  {
                    if (m->text[at] == m->pattern[p + 3] && !--depth) break;
                    else if (m->text[at] == m->pattern[p + 2]) ++depth;
                  }
                if (at == m->length) return SIZE_MAX;
                ++at; p += 4; continue;
              }
            if (special == 'f')
              {
                p += 2;
                if (p == m->pattern_length || m->pattern[p] != '[') { mulib_mune_rt_error_ (m->state, MULIB_MUNE_RUNTIME, "frontier requires a class"); return SIZE_MAX; }
                end = mulib_mune_rt_class_end_ (m, p); if (end == SIZE_MAX) return end;
                if (mulib_mune_rt_class_match_ (m, at ? m->text[at - 1] : 0, p, end)
                    || !mulib_mune_rt_class_match_ (m, at < m->length ? m->text[at] : 0, p, end)) return SIZE_MAX;
                p = end; continue;
              }
            if (special >= '1' && special <= '9')
              {
                size_t index = special - '1', size;
                if (index >= m->captures || m->capture[index].length >= SIZE_MAX - 1)
                  { mulib_mune_rt_error_ (m->state, MULIB_MUNE_RUNTIME, "invalid capture reference"); return SIZE_MAX; }
                size = m->capture[index].length;
                if (size > m->length - at || memcmp (m->text + at, m->text + m->capture[index].start, size)) return SIZE_MAX;
                at += size; p += 2; continue;
              }
          }
        end = mulib_mune_rt_class_end_ (m, p); if (end == SIZE_MAX) return end;
        matches = at < m->length && mulib_mune_rt_class_match_ (m, m->text[at], p, end);
        if (end < m->pattern_length && m->pattern[end] == '?')
          {
            if (matches && (next = mulib_mune_rt_match_ (m, at + 1, end + 1)) != SIZE_MAX) return next;
            p = end + 1; continue;
          }
        if (end < m->pattern_length && (m->pattern[end] == '*' || m->pattern[end] == '+' || m->pattern[end] == '-'))
          {
            size_t minimum = at, maximum;
            if (m->pattern[end] == '+') { if (!matches) return SIZE_MAX; ++minimum; }
            if (m->pattern[end] == '-')
              {
                for (;;)
                  {
                    next = mulib_mune_rt_match_ (m, at, end + 1); if (next != SIZE_MAX || m->state->status) return next;
                    if (at == m->length || !mulib_mune_rt_class_match_ (m, m->text[at], p, end)) return SIZE_MAX;
                    ++at;
                  }
              }
            maximum = minimum;
            while (maximum < m->length && mulib_mune_rt_class_match_ (m, m->text[maximum], p, end)) ++maximum;
            do
              {
                next = mulib_mune_rt_match_ (m, maximum, end + 1); if (next != SIZE_MAX || m->state->status) return next;
              }
            while (maximum-- > minimum);
            return SIZE_MAX;
          }
        if (!matches) return SIZE_MAX;
        ++at; p = end;
      }
    return at;
  }
  static size_t
  mulib_mune_rt_match_ (mulib_mune_pattern_ *m, size_t at, size_t p)
  {
    size_t result, count = m->captures;
    mulib_mune_capture_ saved[32];
    if (m->state->status) return SIZE_MAX;
    if (m->depth == 200) { mulib_mune_rt_error_ (m->state, MULIB_MUNE_LIMIT, "pattern nesting limit"); return SIZE_MAX; }
    memcpy (saved, m->capture, count * sizeof (*saved)); ++m->depth;
    result = mulib_mune_rt_match_body_ (m, at, p); --m->depth;
    if (result == SIZE_MAX) { m->captures = count; memcpy (m->capture, saved, count * sizeof (*saved)); }
    return result;
  }
  static int
  mulib_mune_rt_captures_ (mulib_mune_pattern_ *m, size_t begin, size_t end, int whole, mulib_mune_result_t *out)
  {
    size_t k;
    if (!m->captures && whole) return mulib_mune_result_push (out, mulib_mune_rt_string_ (m->state, (const char *)m->text + begin, end - begin));
    for (k = 0; k < m->captures; ++k)
      {
        mulib_mune_capture_ *c = &m->capture[k]; mulib_mune_value_t v;
        if (c->length == SIZE_MAX) return mulib_mune_rt_error_ (m->state, MULIB_MUNE_RUNTIME, "unfinished capture");
        v = c->length == SIZE_MAX - 1 ? mulib_mune_rt_integer_ ((int64_t)c->start + 1)
            : mulib_mune_rt_string_ (m->state, (const char *)m->text + c->start, c->length);
        if (m->state->status || !mulib_mune_result_push (out, v)) return 0;
      }
    return 1;
  }
  static int
  mulib_mune_rt_patternlib_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0), b = mulib_mune_rt_arg_ (args, n, 1), replacement = mulib_mune_rt_arg_ (args, n, 2);
    mulib_mune_pattern_ m;
    mulib_mune_bytes_ bytes = { NULL, 0, 0 };
    size_t at, end, last = SIZE_MAX, total = 0;
    int64_t initial = 1, limit = INT64_MAX;
    int iterator = !strcmp (op, "iterator"), substitute = !strcmp (op, "gsub"), find = !strcmp (op, "find"), anchored;
    mulib_mune_object_t *closure = s->frame->function.as.object;
    if (iterator) { a = closure->as.function.context[0]; b = closure->as.function.context[1]; initial = (int64_t)closure->as.function.cursor + 1; last = closure->as.function.last_match; }
    if (!mulib_mune_rt_text_ (s, a, 0, &a) || !mulib_mune_rt_text_ (s, b, 0, &b)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "string and pattern expected");
    memset (&m, 0, sizeof (m)); m.state = s; m.text = (const unsigned char *)(a.as.object + 1); m.length = a.as.object->as.length;
    m.pattern = (const unsigned char *)(b.as.object + 1); m.pattern_length = b.as.object->as.length;
    /* Input coercions and captures remain roots during replacement callbacks. */
    if (!mulib_mune_result_push (out, a) || !mulib_mune_result_push (out, b)) return 0;
    if (!iterator && !substitute && n > 2 && args[2].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[2], &initial, 3)) return 0;
    if (substitute && n > 3 && args[3].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[3], &limit, 4)) return 0;
    initial = mulib_mune_rt_relative_ (initial, m.length); if (initial < 1) initial = 1; at = (size_t)initial - 1;
    if (!strcmp (op, "gmatch"))
      {
        mulib_mune_object_t *f = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0); if (!f) return 0;
        f->as.function.native = mulib_mune_rt_builtin_; f->as.function.user = (void *)"string.iterator";
        f->as.function.context[0] = a; f->as.function.context[1] = b; f->as.function.cursor = at; f->as.function.last_match = SIZE_MAX;
        out->count = 0; return mulib_mune_result_push (out, mulib_mune_rt_object_ (f));
      }
    anchored = !iterator && m.pattern_length && m.pattern[0] == '^';
    while (at <= m.length && !s->status && (!substitute || (limit > 0 && total < (uint64_t)limit)))
      {
        m.captures = 0;
        if (find && n > 3 && mulib_mune_rt_truth_ (args[3]))
          end = m.pattern_length <= m.length - at && !memcmp (m.text + at, m.pattern, m.pattern_length) ? at + m.pattern_length : SIZE_MAX;
        else end = mulib_mune_rt_match_ (&m, at, anchored ? 1 : 0);
        if (s->status) break;
        if (end != SIZE_MAX && end != last)
          {
            if (!substitute)
              {
                out->count = 0;
                if (find) { (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)at + 1)); (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)end)); }
                if (iterator) { closure->as.function.cursor = end; closure->as.function.last_match = end; }
                return !s->status && mulib_mune_rt_captures_ (&m, at, end, !find, out);
              }
            else
              {
                mulib_mune_result_t *captures = mulib_mune_rt_results_ (s), *r = NULL;
                mulib_mune_value_t v = replacement;
                if (!captures) break;
                if (!mulib_mune_rt_captures_ (&m, at, end, 1, captures)) { mulib_mune_result_destroy (captures); break; }
                if (v.type == MULIB_MUNE_FUNCTION)
                  { r = mulib_mune_rt_call_ (s, v, captures); if (r) v = mulib_mune_result_get (r, 0); }
                else if (v.type == MULIB_MUNE_TABLE) (void)mulib_mune_rt_index_ (s, v, mulib_mune_result_get (captures, 0), &v);
                if (!s->status)
                  {
                    if (!mulib_mune_rt_truth_ (v)) (void)mulib_mune_rt_bytes_ (s, &bytes, (const char *)m.text + at, end - at);
                    else if (!mulib_mune_rt_text_ (s, v, 0, &v)) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid replacement value");
                    else
                      {
                        size_t k, length = v.as.object->as.length; const char *text = (const char *)(v.as.object + 1);
                        for (k = 0; k < length && !s->status; ++k)
                          {
                            if (replacement.type != MULIB_MUNE_FUNCTION && replacement.type != MULIB_MUNE_TABLE && text[k] == '%')
                              {
                                unsigned char code = k + 1 < length ? (unsigned char)text[++k] : 0;
                                mulib_mune_value_t capture;
                                if (code == '%') (void)mulib_mune_rt_bytes_ (s, &bytes, "%", 1);
                                else if (code == '0') (void)mulib_mune_rt_bytes_ (s, &bytes, (const char *)m.text + at, end - at);
                                else if (code >= '1' && code <= '9' && (size_t)(code - '1') < captures->count)
                                  {
                                    capture = captures->values[code - '1'];
                                    if (mulib_mune_rt_text_ (s, capture, 0, &capture)) (void)mulib_mune_rt_bytes_ (s, &bytes, (const char *)(capture.as.object + 1), capture.as.object->as.length);
                                  }
                                else mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid replacement escape");
                              }
                            else (void)mulib_mune_rt_bytes_ (s, &bytes, text + k, 1);
                          }
                      }
                  }
                mulib_mune_result_destroy (r); mulib_mune_result_destroy (captures);
                ++total; last = end; at = end;
                if (anchored) break;
                continue;
              }
          }
        if (anchored && !(find && n > 3 && mulib_mune_rt_truth_ (args[3]))) break;
        if (at == m.length) { ++at; break; }
        if (substitute && !mulib_mune_rt_bytes_ (s, &bytes, (const char *)m.text + at, 1)) break;
        ++at;
      }
    out->count = 0;
    if (substitute)
      {
        if (!s->status && at < m.length) (void)mulib_mune_rt_bytes_ (s, &bytes, (const char *)m.text + at, m.length - at);
        if (!s->status) { (void)mulib_mune_result_push (out, mulib_mune_rt_string_ (s, bytes.data, bytes.length)); (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)total)); }
        (void)mulib_mune_rt_memory_ (s, bytes.data, bytes.capacity, 0);
      }
    else if (iterator) closure->as.function.cursor = m.length + 1;
    else if (!s->status) (void)mulib_mune_result_push (out, mulib_mune_rt_nil_ ());
    return !s->status;
  }
  static int
  mulib_mune_rt_stringlib_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0), v;
    mulib_mune_bytes_ bytes = { NULL, 0, 0 };
    const char *text;
    size_t length, k;
    int64_t begin, end;
    if (!strcmp (op, "table.concat"))
      {
        mulib_mune_value_t separator = mulib_mune_rt_arg_ (args, n, 1), len;
        const char *sep = ""; size_t seplen = 0;
        if (!mulib_mune_rt_tablelike_ (s, a, 5) || !mulib_mune_rt_operation_ (s, "#", a, a, &len)
            || !mulib_mune_rt_need_int_ (s, len, &end, 1)) return 0;
        if (separator.type != MULIB_MUNE_NIL)
          { if (!mulib_mune_rt_text_ (s, separator, 0, &separator)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "separator string expected"); sep = mulib_mune_value_string (separator, &seplen); }
        /* Converted separator can survive metamethod calls while indexing. */
        if (!mulib_mune_result_push (out, separator)) return 0;
        begin = 1;
        if ((n > 2 && args[2].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[2], &begin, 3))
            || (n > 3 && args[3].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[3], &end, 4))) return 0;
        for (; begin <= end; ++begin)
          {
            if (!mulib_mune_rt_index_ (s, a, mulib_mune_rt_integer_ (begin), &v) || !mulib_mune_rt_text_ (s, v, 0, &v))
              { if (!s->status) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid value in table.concat"); break; }
            text = mulib_mune_value_string (v, &length);
            if (!mulib_mune_rt_bytes_ (s, &bytes, text, length)) break;
            if (begin != end && !mulib_mune_rt_bytes_ (s, &bytes, sep, seplen)) break;
            if (begin == INT64_MAX) break;
          }
        out->count = 0; goto done;
      }
    op += 7;
    if (!strcmp (op, "dump")) return mulib_mune_rt_loadlib_ (s, op, args, n, out);
    if (!strcmp (op, "find") || !strcmp (op, "match") || !strcmp (op, "gmatch") || !strcmp (op, "gsub") || !strcmp (op, "iterator"))
      return mulib_mune_rt_patternlib_ (s, op, args, n, out);
    if (!strcmp (op, "pack") || !strcmp (op, "packsize") || !strcmp (op, "unpack"))
      return mulib_mune_rt_pack_ (s, op, args, n, out);
    if (!strcmp (op, "char"))
      {
        for (k = 0; k < n; ++k)
          {
            char c;
            if (!mulib_mune_rt_need_int_ (s, args[k], &begin, k + 1)) break;
            if (begin < 0 || begin > 255) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "byte out of range"); break; }
            c = (char)begin; if (!mulib_mune_rt_bytes_ (s, &bytes, &c, 1)) break;
          }
        goto done;
      }
    if (!mulib_mune_rt_text_ (s, a, 0, &a)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "string expected");
    text = mulib_mune_value_string (a, &length);
    if (!strcmp (op, "len")) return mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)length));
    if (!strcmp (op, "byte") || !strcmp (op, "sub"))
      {
        begin = 1;
        if (!strcmp (op, "sub") && (n < 2 || args[1].type == MULIB_MUNE_NIL)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "substring position expected");
        if (n > 1 && args[1].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[1], &begin, 2)) return 0;
        end = !strcmp (op, "byte") ? begin : -1;
        if (n > 2 && args[2].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[2], &end, 3)) return 0;
        begin = mulib_mune_rt_relative_ (begin, length); end = mulib_mune_rt_relative_ (end, length);
        if (begin < 1) begin = 1;
        if (end > (int64_t)length) end = (int64_t)length;
        if (!strcmp (op, "byte"))
          { for (; begin <= end; ++begin) if (!mulib_mune_result_push (out, mulib_mune_rt_integer_ ((unsigned char)text[begin - 1]))) return 0; return 1; }
        return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, begin <= end ? text + begin - 1 : "", begin <= end ? (size_t)(end - begin + 1) : 0));
      }
    if (!strcmp (op, "rep"))
      {
        const char *sep = ""; size_t seplen = 0;
        if (!mulib_mune_rt_need_int_ (s, mulib_mune_rt_arg_ (args, n, 1), &end, 2)) return 0;
        if (n > 2 && args[2].type != MULIB_MUNE_NIL)
          { if (!mulib_mune_rt_text_ (s, args[2], 0, &v)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "separator string expected"); sep = mulib_mune_value_string (v, &seplen); }
        if (end > 0 && ((uint64_t)end > s->limits.steps || length > s->limits.memory_bytes / (uint64_t)end
            || (end > 1 && seplen > s->limits.memory_bytes / ((uint64_t)end - 1)))) return mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "repeated string too large");
        for (begin = 0; begin < end; ++begin)
          if (!mulib_mune_rt_bytes_ (s, &bytes, text, length) || (begin + 1 < end && !mulib_mune_rt_bytes_ (s, &bytes, sep, seplen))) break;
      }
    else if (!strcmp (op, "format"))
      {
        if (!mulib_mune_result_push (out, a)) return 0;
        (void)mulib_mune_rt_format_ (s, a, args, n, &bytes); out->count = 0;
      }
    else
      {
        for (k = 0; k < length; ++k)
          {
            unsigned char c = (unsigned char)text[!strcmp (op, "reverse") ? length - k - 1 : k];
            if (!strcmp (op, "lower")) c = (unsigned char)tolower (c);
            else if (!strcmp (op, "upper")) c = (unsigned char)toupper (c);
            if (!mulib_mune_rt_bytes_ (s, &bytes, (const char *)&c, 1)) break;
          }
      }
  done:
    if (!s->status) (void)mulib_mune_result_push (out, mulib_mune_rt_string_ (s, bytes.data, bytes.length));
    (void)mulib_mune_rt_memory_ (s, bytes.data, bytes.capacity, 0); return !s->status;
  }
  enum { MULIB_MUNE_E_LEAF_, MULIB_MUNE_E_COPY_, MULIB_MUNE_E_OP_, MULIB_MUNE_E_INDEX_, MULIB_MUNE_E_STRING_,
         MULIB_MUNE_E_EMPTY_, MULIB_MUNE_E_APPEND_, MULIB_MUNE_E_CALL_, MULIB_MUNE_E_TABLE_, MULIB_MUNE_E_SET_, MULIB_MUNE_E_LIST_, MULIB_MUNE_E_INTEGER_ };
  enum { MULIB_MUNE_B_STATEMENT_ = 100, MULIB_MUNE_B_CONDITION_, MULIB_MUNE_B_BLOCK_, MULIB_MUNE_B_REPEAT_BODY_,
         MULIB_MUNE_B_REPEAT_TEST_, MULIB_MUNE_B_BREAK_, MULIB_MUNE_B_LABEL_ };
  typedef struct mulib_mune_expression_step_
  {
    const mulib_ast_t *node;
    size_t destination, a, b;
    int operation;
    int64_t immediate;
    char op[8];
  } mulib_mune_expression_step_;
  struct mulib_mune_plan_
  {
    mulib_mune_plan_ *next;
    const mulib_ast_t *block;
    const mulib_ast_t **nodes;
    size_t statements, hosts;
    mulib_mir_flow_instruction_t *code;
    mulib_mir_signature_t *signatures;
    mulib_mir_flow_program_t source;
    mulib_vmweave_program_t *vm;
    mulib_jit_flow_t *jit;
    mulib_mune_expression_step_ *expression;
    size_t code_capacity, host_capacity, slots, result_slot;
    mulib_mir_type_t *types;
    size_t integer_registers;
  };
  static void
  mulib_mune_plans_free_ (mulib_mune_plan_ *p)
  {
    while (p)
      {
        mulib_mune_plan_ *next = p->next;
        (void)mulib_jit_flow_destroy (p->jit); mulib_vmweave_program_destroy (p->vm);
        free (p->nodes); free (p->code); free (p->signatures); free (p->expression); free (p->types); free (p); p = next;
      }
  }
  static mulib_mir_flow_instruction_t
  mulib_mune_plan_instruction_ (mulib_mir_flow_opcode_t op, size_t d, size_t a, size_t b, size_t target)
  {
    mulib_mir_flow_instruction_t i;
    memset (&i, 0, sizeof (i)); i.opcode = op; i.destination = d; i.a = a; i.b = b; i.target = target; return i;
  }
  static mulib_mune_plan_ *mulib_mune_plan_get_ (mulib_mune_chunk_t *, const mulib_ast_t *);
  typedef struct mulib_mune_dispatch_
  {
    mulib_mune_state_t *state;
    mulib_mune_plan_ *plan;
    mulib_mune_control_ *control;
    mulib_mune_object_t *base, *loop_base;
    mulib_mune_result_t **values;
  } mulib_mune_dispatch_;
  static int
  mulib_mune_expr_instruction_ (mulib_mune_plan_ *p, mulib_mir_flow_instruction_t i)
  {
    if (p->source.count == p->code_capacity)
      {
        size_t cap = p->code_capacity ? p->code_capacity * 2 : 32;
        mulib_mir_flow_instruction_t *code;
        if (cap > 1000000 || cap > SIZE_MAX / sizeof (*code)) return 0;
        code = (mulib_mir_flow_instruction_t *)realloc (p->code, cap * sizeof (*code)); if (!code) return 0;
        p->code = code; p->code_capacity = cap;
      }
    p->code[p->source.count++] = i; return 1;
  }
  static int
  mulib_mune_expr_step_ (mulib_mune_plan_ *p, int op, size_t d, size_t a, size_t b, int64_t immediate, const mulib_ast_t *n, const char *operator)
  {
    mulib_mune_expression_step_ *e;
    if (p->hosts == p->host_capacity)
      {
        size_t cap = p->host_capacity ? p->host_capacity * 2 : 16;
        mulib_mune_expression_step_ *steps;
        if (cap > 1000000 || cap > SIZE_MAX / sizeof (*steps)) return 0;
        steps = (mulib_mune_expression_step_ *)realloc (p->expression, cap * sizeof (*steps)); if (!steps) return 0;
        p->expression = steps; p->host_capacity = cap;
      }
    e = &p->expression[p->hosts]; memset (e, 0, sizeof (*e));
    e->operation = op; e->destination = d; e->a = a; e->b = b; e->immediate = immediate; e->node = n;
    if (operator) memcpy (e->op, operator, strlen (operator) + 1);
    return mulib_mune_expr_instruction_ (p, mulib_mune_plan_instruction_ (MULIB_MIR_FLOW_CALL, 0, 0, 0, p->hosts++));
  }
  static size_t mulib_mune_expr_build_ (mulib_mune_plan_ *, const mulib_ast_t *);
  static mulib_mune_plan_ *
  mulib_mune_plan_get_ (mulib_mune_chunk_t *chunk, const mulib_ast_t *block)
  {
    static const mulib_mir_type_t type = MULIB_MIR_BOOL;
    mulib_mune_plan_ *p;
    const mulib_ast_t *n;
    size_t count = 0, *starts = NULL, k, dispatch, end;
    for (p = chunk->plans; p; p = p->next) if (p->block == block) return p;
    for (n = block->child; n; n = n->next) ++count;
    if (count > SIZE_MAX / sizeof (*starts) - 1) return NULL;
    p = (mulib_mune_plan_ *)calloc (1, sizeof (*p)); if (!p) return NULL;
    starts = (size_t *)calloc (count + 1, sizeof (*starts)); if (!starts) goto failed;
    p->block = block; p->statements = count;
#define MULIB_MUNE_BHOST_(op,node) do { if (!mulib_mune_expr_step_ (p, op, 0, 0, 0, 0, node, NULL)) goto failed; } while (0)
#define MULIB_MUNE_BJUMP_(target) do { if (!mulib_mune_expr_instruction_ (p, mulib_mune_plan_instruction_ (MULIB_MIR_FLOW_JUMP, MULIB_MIR_NONE, MULIB_MIR_NONE, MULIB_MIR_NONE, target))) goto failed; } while (0)
#define MULIB_MUNE_BBRANCH_(target) do { if (!mulib_mune_expr_instruction_ (p, mulib_mune_plan_instruction_ (MULIB_MIR_FLOW_BRANCH, MULIB_MIR_NONE, 0, MULIB_MIR_NONE, target))) goto failed; } while (0)
    for (n = block->child, k = 0; n; n = n->next, ++k)
      {
        const mulib_ast_t *statement = mulib_mune_is_ (n, "statement") ? n->child : n;
        size_t first = p->source.count, j;
        starts[k] = first;
        if (mulib_mune_is_ (statement, "if_statement"))
          {
            const mulib_ast_t *condition = statement->child, *body = condition->next, *clause = body->next;
            for (;;)
              {
                size_t skip = SIZE_MAX;
                if (condition)
                  {
                    MULIB_MUNE_BHOST_ (MULIB_MUNE_B_CONDITION_, condition);
                    MULIB_MUNE_BBRANCH_ (p->source.count + 2); skip = p->source.count; MULIB_MUNE_BJUMP_ (0);
                  }
                MULIB_MUNE_BHOST_ (MULIB_MUNE_B_BLOCK_, body);
                MULIB_MUNE_BBRANCH_ (SIZE_MAX - 1); MULIB_MUNE_BJUMP_ (SIZE_MAX);
                if (skip != SIZE_MAX) p->code[skip].target = p->source.count;
                if (!clause) break;
                if (mulib_mune_is_ (clause, "else_clause")) { condition = NULL; body = clause->child; }
                else { condition = clause->child; body = condition->next; }
                clause = clause->next;
              }
          }
        else if (mulib_mune_is_ (statement, "while_statement") || mulib_mune_is_ (statement, "repeat_statement"))
          {
            const mulib_ast_t *condition = mulib_mune_child_ (statement, "expression"), *body = mulib_mune_child_ (statement, "block");
            if (mulib_mune_is_ (statement, "while_statement"))
              {
                MULIB_MUNE_BHOST_ (MULIB_MUNE_B_CONDITION_, condition);
                MULIB_MUNE_BBRANCH_ (p->source.count + 2); MULIB_MUNE_BJUMP_ (SIZE_MAX - 1);
                MULIB_MUNE_BHOST_ (MULIB_MUNE_B_BLOCK_, body);
                MULIB_MUNE_BBRANCH_ (first);
              }
            else
              {
                size_t escaped;
                MULIB_MUNE_BHOST_ (MULIB_MUNE_B_REPEAT_BODY_, body);
                MULIB_MUNE_BBRANCH_ (p->source.count + 2); escaped = p->source.count; MULIB_MUNE_BJUMP_ (0);
                MULIB_MUNE_BHOST_ (MULIB_MUNE_B_REPEAT_TEST_, condition);
                MULIB_MUNE_BBRANCH_ (SIZE_MAX - 1); MULIB_MUNE_BJUMP_ (first);
                p->code[escaped].target = p->source.count;
              }
            MULIB_MUNE_BHOST_ (MULIB_MUNE_B_BREAK_, statement);
            MULIB_MUNE_BBRANCH_ (SIZE_MAX - 1); MULIB_MUNE_BJUMP_ (SIZE_MAX);
          }
        else
          {
            MULIB_MUNE_BHOST_ (MULIB_MUNE_B_STATEMENT_, n);
            MULIB_MUNE_BBRANCH_ (SIZE_MAX - 1); MULIB_MUNE_BJUMP_ (SIZE_MAX);
          }
        for (j = first; j < p->source.count; ++j)
          if ((p->code[j].opcode == MULIB_MIR_FLOW_JUMP || p->code[j].opcode == MULIB_MIR_FLOW_BRANCH) && p->code[j].target == SIZE_MAX - 1)
            p->code[j].target = p->source.count;
      }
    starts[count] = p->source.count;
    end = p->source.count; MULIB_MUNE_BJUMP_ (0);
    dispatch = p->source.count;
    for (n = block->child, k = 0; n; n = n->next, ++k)
      if (mulib_mune_is_ (n->child, "label"))
        { MULIB_MUNE_BHOST_ (MULIB_MUNE_B_LABEL_, n); MULIB_MUNE_BBRANCH_ (starts[k + 1]); }
    p->code[end].target = p->source.count;
    if (!mulib_mune_expr_instruction_ (p, mulib_mune_plan_instruction_ (MULIB_MIR_FLOW_RETURN, MULIB_MIR_NONE, 0, MULIB_MIR_NONE, MULIB_MIR_NONE))) goto failed;
    for (k = 0; k < p->source.count; ++k)
      if (p->code[k].opcode == MULIB_MIR_FLOW_JUMP && p->code[k].target == SIZE_MAX) p->code[k].target = dispatch;
    p->signatures = (mulib_mir_signature_t *)calloc (p->hosts ? p->hosts : 1, sizeof (*p->signatures)); if (!p->signatures) goto failed;
    for (k = 0; k < p->hosts; ++k) p->signatures[k].result = MULIB_MIR_BOOL;
    p->source.code = p->code; p->source.register_types = &type; p->source.register_count = 1;
    p->source.signature.result = MULIB_MIR_BOOL; p->source.hosts = p->signatures; p->source.host_count = p->hosts;
    p->next = chunk->plans; chunk->plans = p; free (starts); return p;
  failed:
    free (starts); mulib_mune_plans_free_ (p); return NULL;
#undef MULIB_MUNE_BHOST_
#undef MULIB_MUNE_BJUMP_
#undef MULIB_MUNE_BBRANCH_
  }
  static int
  mulib_mune_expr_integer_ (const mulib_ast_t *n)
  {
    const mulib_ast_t *left, *c;
    if (mulib_mune_is_ (n, "number"))
      { mulib_mune_value_t value; return mulib_mune_rt_parse_number_ (n->text, n->length, &value) && value.type == MULIB_MUNE_INTEGER; }
    if (mulib_mune_is_ (n, "parenthesized")) return mulib_mune_expr_integer_ (n->child);
    if (!n->child || mulib_mune_is_ (n, "prefix") || mulib_mune_is_ (n, "table") || mulib_mune_is_ (n, "function_literal")) return 0;
    if (mulib_mune_is_ (n, "unary") && mulib_mune_is_ (n->child, "unary_op"))
      return (!strcmp (n->child->text, "-") || !strcmp (n->child->text, "~")) && mulib_mune_expr_integer_ (n->child->next);
    left = n->child;
    if (!mulib_mune_expr_integer_ (left)) return 0;
    for (c = left->next; c; c = c->next)
      {
        char op[4]; mulib_mune_rt_operator_ (n, left, op);
        if (!op[0] || op[1] || !strchr ("+-*&|~", op[0])) return 0;
        if (mulib_mune_is_ (c, "add_op") || mulib_mune_is_ (c, "multiply_op")) c = c->next;
        if (!mulib_mune_expr_integer_ (c)) return 0;
        left = c;
      }
    return 1;
  }
  static size_t
  mulib_mune_expr_integer_build_ (mulib_mune_plan_ *p, const mulib_ast_t *n)
  {
    size_t a, b, result;
    const mulib_ast_t *left, *c;
    mulib_mir_flow_instruction_t i;
    if (mulib_mune_is_ (n, "number"))
      {
        mulib_mune_value_t v = mulib_mune_rt_nil_ ();
        (void)mulib_mune_rt_parse_number_ (n->text, n->length, &v);
        result = ++p->integer_registers;
        i = mulib_mune_plan_instruction_ (MULIB_MIR_FLOW_CONST, result, MULIB_MIR_NONE, MULIB_MIR_NONE, MULIB_MIR_NONE);
        i.immediate.integer = v.as.integer;
        return mulib_mune_expr_instruction_ (p, i) ? result : SIZE_MAX;
      }
    if (mulib_mune_is_ (n, "parenthesized")) return mulib_mune_expr_integer_build_ (p, n->child);
    if (mulib_mune_is_ (n, "unary") && mulib_mune_is_ (n->child, "unary_op"))
      {
        a = mulib_mune_expr_integer_build_ (p, n->child->next); b = ++p->integer_registers; result = ++p->integer_registers;
        i = mulib_mune_plan_instruction_ (MULIB_MIR_FLOW_CONST, b, MULIB_MIR_NONE, MULIB_MIR_NONE, MULIB_MIR_NONE);
        i.immediate.integer = n->child->text[0] == '-' ? 0 : -1;
        if (a == SIZE_MAX || !mulib_mune_expr_instruction_ (p, i)) return SIZE_MAX;
        i = mulib_mune_plan_instruction_ (n->child->text[0] == '-' ? MULIB_MIR_FLOW_WRAP_SUB : MULIB_MIR_FLOW_XOR, result, b, a, MULIB_MIR_NONE);
        return mulib_mune_expr_instruction_ (p, i) ? result : SIZE_MAX;
      }
    left = n->child; result = mulib_mune_expr_integer_build_ (p, left); if (result == SIZE_MAX) return SIZE_MAX;
    for (c = left->next; c; c = c->next)
      {
        char op[4]; mulib_mir_flow_opcode_t code;
        mulib_mune_rt_operator_ (n, left, op);
        if (mulib_mune_is_ (c, "add_op") || mulib_mune_is_ (c, "multiply_op")) c = c->next;
        a = result; b = mulib_mune_expr_integer_build_ (p, c); result = ++p->integer_registers;
        code = op[0] == '+' ? MULIB_MIR_FLOW_WRAP_ADD : op[0] == '-' ? MULIB_MIR_FLOW_WRAP_SUB
               : op[0] == '*' ? MULIB_MIR_FLOW_WRAP_MUL : op[0] == '&' ? MULIB_MIR_FLOW_AND
               : op[0] == '|' ? MULIB_MIR_FLOW_OR : MULIB_MIR_FLOW_XOR;
        if (b == SIZE_MAX || !mulib_mune_expr_instruction_ (p, mulib_mune_plan_instruction_ (code, result, a, b, MULIB_MIR_NONE))) return SIZE_MAX;
        left = c;
      }
    return result;
  }
  static size_t
  mulib_mune_expr_list_ (mulib_mune_plan_ *p, const mulib_ast_t *n, size_t self)
  {
    size_t result = p->slots++, part;
    const mulib_ast_t *c;
    if (!mulib_mune_expr_step_ (p, MULIB_MUNE_E_EMPTY_, result, 0, 0, 0, n, NULL)) return SIZE_MAX;
    if (self != SIZE_MAX && !mulib_mune_expr_step_ (p, MULIB_MUNE_E_APPEND_, result, self, 0, 1, n, NULL)) return SIZE_MAX;
    for (c = n ? n->child : NULL; c; c = c->next)
      {
        part = mulib_mune_expr_build_ (p, c); if (part == SIZE_MAX) return SIZE_MAX;
        if (!mulib_mune_expr_step_ (p, MULIB_MUNE_E_APPEND_, result, part, 0, c->next != NULL, c, NULL)) return SIZE_MAX;
      }
    return result;
  }
  static size_t
  mulib_mune_expr_build_ (mulib_mune_plan_ *p, const mulib_ast_t *n)
  {
    size_t result, a, b;
    const mulib_ast_t *c, *left;
    if (mulib_mune_expr_integer_ (n))
      {
        size_t reg = mulib_mune_expr_integer_build_ (p, n);
        result = p->slots++;
        if (reg == SIZE_MAX || !mulib_mune_expr_step_ (p, MULIB_MUNE_E_INTEGER_, result, 0, 0, 0, n, NULL)) return SIZE_MAX;
        p->code[p->source.count - 1].a = reg; p->code[p->source.count - 1].b = 1; return result;
      }
    if (mulib_mune_is_ (n, "parenthesized"))
      {
        a = mulib_mune_expr_build_ (p, n->child); result = p->slots++;
        return a != SIZE_MAX && mulib_mune_expr_step_ (p, MULIB_MUNE_E_COPY_, result, a, 0, 0, n, NULL) ? result : SIZE_MAX;
      }
    if (mulib_mune_is_ (n, "prefix"))
      {
        result = mulib_mune_expr_build_ (p, n->child); if (result == SIZE_MAX) return SIZE_MAX;
        for (c = n->child->next; c; c = c->next)
          {
            const mulib_ast_t *part = c->child;
            a = result; result = p->slots++;
            if (mulib_mune_is_ (part, "call"))
              {
                const mulib_ast_t *method = mulib_mune_child_ (part, "method_name"), *args = mulib_mune_child_ (part, "arguments")->child;
                size_t fn = a;
                if (method)
                  {
                    size_t key = p->slots++; fn = p->slots++;
                    if (!mulib_mune_expr_step_ (p, MULIB_MUNE_E_STRING_, key, 0, 0, 0, method->child, NULL)
                        || !mulib_mune_expr_step_ (p, MULIB_MUNE_E_INDEX_, fn, a, key, 0, part, NULL)) return SIZE_MAX;
                  }
                if (!args || mulib_mune_is_ (args, "expressions")) b = mulib_mune_expr_list_ (p, args, method ? a : SIZE_MAX);
                else
                  {
                    size_t argument = mulib_mune_expr_build_ (p, args);
                    b = p->slots++;
                    if (argument == SIZE_MAX || !mulib_mune_expr_step_ (p, MULIB_MUNE_E_EMPTY_, b, 0, 0, 0, part, NULL)
                        || (method && !mulib_mune_expr_step_ (p, MULIB_MUNE_E_APPEND_, b, a, 0, 1, part, NULL))
                        || !mulib_mune_expr_step_ (p, MULIB_MUNE_E_APPEND_, b, argument, 0, 0, part, NULL)) return SIZE_MAX;
                  }
                if (b == SIZE_MAX || !mulib_mune_expr_step_ (p, MULIB_MUNE_E_CALL_, result, fn, b, 0, part, NULL)) return SIZE_MAX;
              }
            else
              {
                if (mulib_mune_is_ (part, "index")) b = mulib_mune_expr_build_ (p, part->child);
                else { b = p->slots++; if (!mulib_mune_expr_step_ (p, MULIB_MUNE_E_STRING_, b, 0, 0, 0, part->child, NULL)) return SIZE_MAX; }
                if (b == SIZE_MAX || !mulib_mune_expr_step_ (p, MULIB_MUNE_E_INDEX_, result, a, b, 0, part, NULL)) return SIZE_MAX;
              }
          }
        return result;
      }
    if (mulib_mune_is_ (n, "table"))
      {
        int64_t index = 1;
        result = p->slots++;
        if (!mulib_mune_expr_step_ (p, MULIB_MUNE_E_TABLE_, result, 0, 0, 0, n, NULL)) return SIZE_MAX;
        for (c = n->child ? n->child->child : NULL; c; c = c->next)
          if (mulib_mune_is_ (c, "field"))
            {
              const mulib_ast_t *field = c->child;
              if (mulib_mune_is_ (field, "indexed_field") || mulib_mune_is_ (field, "named_field"))
                {
                  if (mulib_mune_is_ (field, "indexed_field")) a = mulib_mune_expr_build_ (p, field->child);
                  else { a = p->slots++; if (!mulib_mune_expr_step_ (p, MULIB_MUNE_E_STRING_, a, 0, 0, 0, field->child, NULL)) return SIZE_MAX; }
                  b = mulib_mune_expr_build_ (p, field->child->next);
                  if (a == SIZE_MAX || b == SIZE_MAX || !mulib_mune_expr_step_ (p, MULIB_MUNE_E_SET_, result, a, b, 0, field, NULL)) return SIZE_MAX;
                }
              else
                {
                  a = mulib_mune_expr_build_ (p, field);
                  if (a == SIZE_MAX || !mulib_mune_expr_step_ (p, MULIB_MUNE_E_LIST_, result, a, c->next && c->next->next ? 1 : 0, index++, field, NULL)) return SIZE_MAX;
                }
            }
        return result;
      }
    if (mulib_mune_is_ (n, "function_literal") || !n->child)
      {
        result = p->slots++;
        return mulib_mune_expr_step_ (p, MULIB_MUNE_E_LEAF_, result, 0, 0, 0, n, NULL) ? result : SIZE_MAX;
      }
    if (mulib_mune_is_ (n, "unary") && mulib_mune_is_ (n->child, "unary_op"))
      {
        const char *op = n->child->text;
        a = mulib_mune_expr_build_ (p, n->child->next); result = p->slots++;
        return a != SIZE_MAX && mulib_mune_expr_step_ (p, MULIB_MUNE_E_OP_, result, a, a, 0, n,
            !strcmp (op, "-") ? "unm" : !strcmp (op, "~") ? "bnot" : op) ? result : SIZE_MAX;
      }
    left = n->child; result = mulib_mune_expr_build_ (p, left); if (result == SIZE_MAX) return SIZE_MAX;
    for (c = left->next; c; c = c->next)
      {
        char op[4]; size_t branch = SIZE_MAX;
        mulib_mune_rt_operator_ (n, left, op);
        if (mulib_mune_is_ (c, "compare_op") || mulib_mune_is_ (c, "shift_op") || mulib_mune_is_ (c, "add_op") || mulib_mune_is_ (c, "multiply_op")) c = c->next;
        a = result; result = p->slots++;
        if (!strcmp (op, "and") || !strcmp (op, "or"))
          {
            if (!mulib_mune_expr_step_ (p, MULIB_MUNE_E_COPY_, result, a, 0, 0, n, NULL)) return SIZE_MAX;
            if (!strcmp (op, "and") && !mulib_mune_expr_instruction_ (p, mulib_mune_plan_instruction_ (MULIB_MIR_FLOW_NOT, 0, 0, MULIB_MIR_NONE, MULIB_MIR_NONE))) return SIZE_MAX;
            branch = p->source.count;
            if (!mulib_mune_expr_instruction_ (p, mulib_mune_plan_instruction_ (MULIB_MIR_FLOW_BRANCH, MULIB_MIR_NONE, 0, MULIB_MIR_NONE, 0))) return SIZE_MAX;
          }
        b = mulib_mune_expr_build_ (p, c); if (b == SIZE_MAX) return SIZE_MAX;
        if (!mulib_mune_expr_step_ (p, branch == SIZE_MAX ? MULIB_MUNE_E_OP_ : MULIB_MUNE_E_COPY_, result,
                                   branch == SIZE_MAX ? a : b, b, 0, n, branch == SIZE_MAX ? op : NULL)) return SIZE_MAX;
        if (branch != SIZE_MAX) p->code[branch].target = p->source.count;
        left = c;
      }
    return result;
  }
  static mulib_mune_plan_ *
  mulib_mune_expr_plan_ (mulib_mune_chunk_t *chunk, const mulib_ast_t *n)
  {
    mulib_mune_plan_ *p; size_t k;
    for (p = chunk->plans; p; p = p->next) if (p->block == n) return p;
    p = (mulib_mune_plan_ *)calloc (1, sizeof (*p)); if (!p) return NULL;
    p->block = n; p->result_slot = mulib_mune_expr_build_ (p, n);
    if (p->result_slot == SIZE_MAX || !mulib_mune_expr_instruction_ (p, mulib_mune_plan_instruction_ (MULIB_MIR_FLOW_RETURN, MULIB_MIR_NONE, 0, MULIB_MIR_NONE, MULIB_MIR_NONE))) goto failed;
    p->signatures = (mulib_mir_signature_t *)calloc (p->hosts, sizeof (*p->signatures)); if (!p->signatures) goto failed;
    p->types = (mulib_mir_type_t *)malloc ((p->integer_registers + 1) * sizeof (*p->types)); if (!p->types) goto failed;
    p->types[0] = MULIB_MIR_BOOL;
    for (k = 1; k <= p->integer_registers; ++k) p->types[k] = MULIB_MIR_I64;
    for (k = 0; k < p->hosts; ++k)
      {
        p->signatures[k].result = MULIB_MIR_BOOL;
        if (p->expression[k].operation == MULIB_MUNE_E_INTEGER_)
          { p->signatures[k].parameters = p->types + 1; p->signatures[k].parameter_count = 1; }
      }
    p->source.code = p->code; p->source.register_types = p->types; p->source.register_count = p->integer_registers + 1;
    p->source.signature.result = MULIB_MIR_BOOL; p->source.hosts = p->signatures; p->source.host_count = p->hosts;
    p->next = chunk->plans; chunk->plans = p; return p;
  failed:
    mulib_mune_plans_free_ (p); return NULL;
  }
  static mulib_mir_status_t
  mulib_mune_expr_host_ (mulib_mune_dispatch_ *d, size_t host, const mulib_mir_value_t *arguments, mulib_mir_value_t *out)
  {
    const mulib_mune_expression_step_ *e = &d->plan->expression[host];
    mulib_mune_state_t *s = d->state;
    mulib_mune_result_t **values = d->values, *r = NULL;
    mulib_mune_value_t a = mulib_mune_result_get (values[e->a], 0), b = mulib_mune_result_get (values[e->b], 0), v;
    if (e->node && !mulib_mune_rt_tick_ (s, e->node)) return MULIB_MIR_HOST_ERROR;
    switch (e->operation)
      {
      case MULIB_MUNE_E_LEAF_: r = mulib_mune_rt_tree_expression_ (s, e->node); break;
      case MULIB_MUNE_E_INTEGER_: r = mulib_mune_rt_single_ (s, mulib_mune_rt_integer_ (arguments[0].integer)); break;
      case MULIB_MUNE_E_COPY_: r = mulib_mune_rt_single_ (s, a); break;
      case MULIB_MUNE_E_OP_: if (mulib_mune_rt_operation_ (s, e->op, a, b, &v)) r = mulib_mune_rt_single_ (s, v); break;
      case MULIB_MUNE_E_INDEX_: if (mulib_mune_rt_index_ (s, a, b, &v)) r = mulib_mune_rt_single_ (s, v); break;
      case MULIB_MUNE_E_STRING_: r = mulib_mune_rt_single_ (s, mulib_mune_rt_string_ (s, e->node->text, e->node->length)); break;
      case MULIB_MUNE_E_EMPTY_: r = mulib_mune_rt_results_ (s); break;
      case MULIB_MUNE_E_APPEND_:
        if (e->immediate) (void)mulib_mune_result_push (values[e->destination], a);
        else (void)mulib_mune_rt_append_ (values[e->destination], values[e->a], 0);
        break;
      case MULIB_MUNE_E_CALL_: r = mulib_mune_rt_call_ (s, a, values[e->b]); break;
      case MULIB_MUNE_E_TABLE_: r = mulib_mune_rt_single_ (s, mulib_mune_rt_object_ (mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0))); break;
      case MULIB_MUNE_E_SET_:
        v = mulib_mune_result_get (values[e->destination], 0);
        (void)mulib_mune_rt_rawset_ (s, v.as.object, a, b); break;
      case MULIB_MUNE_E_LIST_:
        {
          size_t k, count = e->b ? 1 : values[e->a]->count;
          v = mulib_mune_result_get (values[e->destination], 0);
          for (k = 0; k < count; ++k)
            if (!mulib_mune_rt_rawset_ (s, v.as.object, mulib_mune_rt_integer_ (e->immediate + (int64_t)k), mulib_mune_result_get (values[e->a], k))) break;
        }
        break;
      default: return MULIB_MIR_HOST_ERROR;
      }
    if (r) { mulib_mune_result_destroy (values[e->destination]); values[e->destination] = r; }
    out->integer = mulib_mune_rt_truth_ (mulib_mune_result_get (values[e->destination], 0));
    if (s->status) mulib_mune_rt_handle_error_ (s);
    return s->status ? MULIB_MIR_HOST_ERROR : MULIB_MIR_OK;
  }
  static mulib_mir_status_t
  mulib_mune_dispatch_host_ (void *user, size_t host, const mulib_mir_value_t *args, size_t count, mulib_mir_value_t *out)
  {
    mulib_mune_dispatch_ *d = (mulib_mune_dispatch_ *)user;
    mulib_mune_state_t *s = d->state;
    const mulib_ast_t *n;
    (void)args; (void)count;
    if (host >= d->plan->hosts) return MULIB_MIR_HOST_ERROR;
    if (d->plan->slots) return mulib_mune_expr_host_ (d, host, args, out);
    n = d->plan->expression[host].node;
    if (d->plan->expression[host].operation != MULIB_MUNE_B_LABEL_)
      {
        mulib_mune_rt_gc_auto_ (s);
        if (!mulib_mune_rt_tick_ (s, n)) return MULIB_MIR_HOST_ERROR;
        switch (d->plan->expression[host].operation)
          {
          case MULIB_MUNE_B_CONDITION_: out->integer = mulib_mune_rt_condition_ (s, n); break;
          case MULIB_MUNE_B_REPEAT_BODY_:
            d->loop_base = s->frame->environment; mulib_mune_rt_block_ (s, n, 1, d->control);
            out->integer = d->control->kind == MULIB_MUNE_NEXT_; break;
          case MULIB_MUNE_B_REPEAT_TEST_:
            out->integer = mulib_mune_rt_condition_ (s, n); mulib_mune_rt_close_ (s, d->loop_base); break;
          case MULIB_MUNE_B_BLOCK_:
            mulib_mune_rt_block_ (s, n, 0, d->control); out->integer = d->control->kind == MULIB_MUNE_NEXT_; break;
          case MULIB_MUNE_B_BREAK_:
            out->integer = d->control->kind == MULIB_MUNE_BREAK_;
            if (out->integer) d->control->kind = MULIB_MUNE_NEXT_;
            break;
          default:
            mulib_mune_rt_statement_ (s, n, d->control); out->integer = d->control->kind == MULIB_MUNE_NEXT_; break;
          }
      }
    else
      {
        out->integer = d->control->kind == MULIB_MUNE_GOTO_ && mulib_mune_same_name_ (n->child->child, d->control->label);
        if (out->integer)
          {
            const mulib_ast_t *after;
            mulib_mune_object_t *target = s->frame->environment;
            for (after = n->next; after; after = after->next)
              if (!mulib_mune_is_ (after, "statement") || (!mulib_mune_is_ (after->child, "label") && !mulib_mune_is_ (after->child, "empty_statement"))) break;
            if (!after && !mulib_mune_is_ (d->plan->block->parent, "repeat_statement")) target = d->base;
            else while (target != d->base && target->as.environment.offset > n->offset) target = target->as.environment.parent;
            mulib_mune_rt_close_ (s, target); d->control->kind = MULIB_MUNE_NEXT_;
          }
      }
    return s->status ? MULIB_MIR_HOST_ERROR : MULIB_MIR_OK;
  }
  static void
  mulib_mune_rt_plan_run_ (mulib_mune_state_t *s, mulib_mune_plan_ *plan, mulib_mune_dispatch_ *dispatch)
  {
    mulib_mune_chunk_t *chunk = s->frame->chunk;
    mulib_mir_frame_t frame;
    mulib_mir_status_t status = MULIB_MIR_OK;
    const mulib_mir_flow_program_t *program = NULL;
    const mulib_mune_aot_entry_t *aot = NULL;
    size_t k;
    memset (&frame, 0, sizeof (frame));
    if (!plan) { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "block compilation allocation failed"); return; }
    if (s->mode == MULIB_MUNE_JIT)
      {
        if (!plan->jit)
          {
            mulib_mir_diagnostic_t diagnostic = { MULIB_MIR_OK, 0 };
            int compiled = mulib_jit_flow_compile (&plan->source, 1000000, 1, &plan->jit, &diagnostic);
            if (compiled != MULIB_JIT_OK)
              { mulib_mune_rt_error_ (s, compiled == MULIB_JIT_ENOMEM || diagnostic.status == MULIB_MIR_NO_MEMORY ? MULIB_MUNE_MEMORY : MULIB_MUNE_RUNTIME, "native compilation failed"); goto done; }
          }
        program = mulib_jit_flow_program (plan->jit);
      }
    else if (s->mode == MULIB_MUNE_AOT && chunk->aot_count)
      {
        for (k = 0; k < chunk->aot_count; ++k)
          if (chunk->aot[k].block_offset == plan->block->offset && chunk->aot[k].production && !strcmp (chunk->aot[k].production, plan->block->production)) { aot = &chunk->aot[k]; break; }
        if (!aot || !aot->program || !aot->execute) { mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "no AOT entry for block"); goto done; }
        program = aot->program ();
      }
    else
      {
        if (!plan->vm) status = mulib_vmweave_compile (&plan->source, 1000000, 1, &plan->vm, NULL);
        if (status != MULIB_MIR_OK) { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "VM compilation failed"); goto done; }
        program = mulib_vmweave_program_view (plan->vm);
      }
    status = mulib_mir_frame_init (&frame, program, NULL, 0, mulib_mune_dispatch_host_, dispatch, 1000000, NULL);
    if (status != MULIB_MIR_OK) { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "execution frame allocation failed"); goto done; }
    frame.remaining = SIZE_MAX;
    status = aot ? aot->execute (&frame) : s->mode == MULIB_MUNE_JIT ? mulib_jit_flow_run (plan->jit, &frame, SIZE_MAX) : mulib_vmweave_run (&frame, SIZE_MAX);
    if (!s->status && status != MULIB_MIR_FINISHED) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "%s", mulib_mir_status_string (status));
  done:
    mulib_mir_frame_free (&frame);
  }
  static void
  mulib_mune_rt_compiled_block_ (mulib_mune_state_t *s, const mulib_ast_t *block, int retain, mulib_mune_control_ *control)
  {
    mulib_mune_plan_ *plan = mulib_mune_plan_get_ (s->frame->chunk, block);
    mulib_mune_dispatch_ dispatch;
    memset (&dispatch, 0, sizeof (dispatch)); dispatch.state = s; dispatch.plan = plan; dispatch.control = control; dispatch.base = s->frame->environment;
    mulib_mune_rt_plan_run_ (s, plan, &dispatch);
    if (s->status) mulib_mune_rt_handle_error_ (s);
    if (!retain || s->status || control->kind) mulib_mune_rt_close_ (s, dispatch.base);
  }
  static mulib_mune_result_t *
  mulib_mune_rt_compiled_expression_ (mulib_mune_state_t *s, const mulib_ast_t *n)
  {
    mulib_mune_plan_ *plan;
    mulib_mune_dispatch_ dispatch;
    mulib_mune_result_t *result = NULL;
    size_t k;
    if (s->status) return NULL;
    plan = mulib_mune_expr_plan_ (s->frame->chunk, n);
    if (!plan) { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "expression compilation failed"); return NULL; }
    memset (&dispatch, 0, sizeof (dispatch)); dispatch.state = s; dispatch.plan = plan;
    dispatch.values = (mulib_mune_result_t **)mulib_mune_rt_memory_ (s, NULL, 0, plan->slots * sizeof (*dispatch.values));
    if (!dispatch.values) return NULL;
    memset (dispatch.values, 0, plan->slots * sizeof (*dispatch.values));
    mulib_mune_rt_plan_run_ (s, plan, &dispatch);
    if (!s->status) { result = dispatch.values[plan->result_slot]; dispatch.values[plan->result_slot] = NULL; }
    for (k = 0; k < plan->slots; ++k) mulib_mune_result_destroy (dispatch.values[k]);
    (void)mulib_mune_rt_memory_ (s, dispatch.values, plan->slots * sizeof (*dispatch.values), 0);
    return result;
  }
  mulib_mune_status_t
  mulib_mune_set_mode (mulib_mune_state_t *s, mulib_mune_mode_t mode)
  {
    if (!s || s->running || mode < MULIB_MUNE_TREE || mode > MULIB_MUNE_AOT) return MULIB_MUNE_INVALID;
    if (mode == MULIB_MUNE_JIT && !mulib_jit_supported ()) return MULIB_MUNE_RUNTIME;
    s->mode = mode; return MULIB_MUNE_OK;
  }
  static int
  mulib_mune_emit_text_ (mulib_mir_write_fn writer, void *user, const char *format, ...)
  {
    char text[1024]; int n; va_list ap;
    va_start (ap, format); n = vsnprintf (text, sizeof (text), format, ap); va_end (ap);
    return n >= 0 && (size_t)n < sizeof (text) && writer (user, text, (size_t)n);
  }
  static int
  mulib_mune_prepare_blocks_ (mulib_mune_chunk_t *chunk, const mulib_ast_t *n)
  {
    const mulib_ast_t *c;
    if (mulib_mune_is_ (n, "block") && !mulib_mune_plan_get_ (chunk, n)) return 0;
    if ((mulib_mune_is_ (n, "expression") || mulib_mune_is_ (n, "prefix") || mulib_mune_is_ (n, "name")
         || mulib_mune_is_ (n, "parenthesized") || mulib_mune_is_ (n, "table") || mulib_mune_is_ (n, "string"))
        && !mulib_mune_expr_plan_ (chunk, n)) return 0;
    for (c = n->child; c; c = c->next) if (!mulib_mune_prepare_blocks_ (chunk, c)) return 0;
    return 1;
  }
  mulib_mune_status_t
  mulib_mune_emit_c (mulib_mune_chunk_t *chunk, const char *name, mulib_mir_write_fn writer, void *user, mulib_mune_error_t *error)
  {
    mulib_mune_plan_ *p; size_t k = 0, blocks = 0, length;
    const mulib_ast_t *root;
    mulib_mune_status_t status = MULIB_MUNE_OK;
    if (!chunk || !name || !writer || strncmp (name, "mulib_generated_", 16) || !name[16] || strlen (name) > 90)
      { status = MULIB_MUNE_INVALID; goto done; }
    for (k = 0; name[k]; ++k) if (!mulib_mune_alpha_ ((unsigned char)name[k]) && !mulib_mune_digit_ ((unsigned char)name[k])) { status = MULIB_MUNE_INVALID; goto done; }
    root = chunk->walker->root;
    if (!mulib_mune_prepare_blocks_ (chunk, root)) { status = MULIB_MUNE_MEMORY; goto done; }
#define MULIB_MUNE_EMIT_(...) do { if (!mulib_mune_emit_text_ (writer, user, __VA_ARGS__)) { status = MULIB_MUNE_IO; goto done; } } while (0)
    MULIB_MUNE_EMIT_ ("#include \"mulib-mune.h\"\n");
    for (p = chunk->plans; p; p = p->next)
      {
        char symbol[128]; mulib_mir_status_t mir;
        (void)snprintf (symbol, sizeof (symbol), "%s_block%zu", name, blocks++);
        mir = mulib_mir_flow_emit_c (&p->source, symbol, 1000000, writer, user, NULL);
        if (mir != MULIB_MIR_OK) { status = mir == MULIB_MIR_IO_ERROR ? MULIB_MUNE_IO : MULIB_MUNE_INVALID; goto done; }
      }
    MULIB_MUNE_EMIT_ ("static const mulib_mune_aot_entry_t %s_entries[]={\n", name);
    k = 0;
    for (p = chunk->plans; p; p = p->next, ++k) MULIB_MUNE_EMIT_ ("{%zu,%s_block%zu_program,%s_block%zu,\"%s\"},\n", p->block->offset, name, k, name, k, p->block->production);
    MULIB_MUNE_EMIT_ ("};\nstatic const unsigned char %s_source[]={\n", name);
    length = root->length;
    for (k = 0; k < length; ++k) MULIB_MUNE_EMIT_ ("%u,%s", (unsigned)(unsigned char)root->text[k], k % 24 == 23 ? "\n" : "");
    MULIB_MUNE_EMIT_ ("0};\nmulib_mune_status_t %s(mulib_mune_state_t *s,mulib_mune_result_t **r,mulib_mune_error_t *e){\n"
                      "return mulib_mune_run_aot(s,(const char *)%s_source,%zu,%s_entries,%zu,r,e);}\n", name, name, length, name, blocks);
#undef MULIB_MUNE_EMIT_
  done:
    if (error) { error->code = status; error->offset = 0; error->message = status == MULIB_MUNE_OK ? "ok" : "AOT generation failed"; }
    return status;
  }
  mulib_mune_status_t
  mulib_mune_run_aot (mulib_mune_state_t *s, const char *source, size_t length,
                     const mulib_mune_aot_entry_t *entries, size_t count, mulib_mune_result_t **out, mulib_mune_error_t *error)
  {
    mulib_mune_chunk_t *chunk;
    mulib_mune_error_t e;
    mulib_mune_status_t status;
    mulib_mune_mode_t saved;
    if (!s || !entries || !count || !out || *out || s->running) return MULIB_MUNE_INVALID;
    chunk = mulib_mune_parse (source, length, NULL, &e);
    if (!chunk) { if (error) *error = e; return e.code; }
    chunk->aot = entries; chunk->aot_count = count; saved = s->mode; s->mode = MULIB_MUNE_AOT;
    status = mulib_mune_run (s, chunk, out, error); s->mode = saved; mulib_mune_chunk_destroy (chunk); return status;
  }
  static int
  mulib_mune_rt_readfile_ (mulib_mune_state_t *s, const char *path, mulib_mune_bytes_ *bytes)
  {
    FILE *file = fopen (path, "rb"); char buffer[4096]; size_t n; int bad;
    if (!file) return mulib_mune_rt_error_ (s, MULIB_MUNE_IO, "cannot open %s", path);
    while ((n = fread (buffer, 1, sizeof (buffer), file)) != 0)
      if (!mulib_mune_rt_bytes_ (s, bytes, buffer, n)) break;
    bad = ferror (file); if (fclose (file)) bad = 1;
    if (bad && !s->status) mulib_mune_rt_error_ (s, MULIB_MUNE_IO, "cannot read %s", path);
    return !s->status;
  }
  static int
  mulib_mune_rt_searchpath_ (mulib_mune_state_t *s, mulib_mune_value_t name, mulib_mune_value_t path,
                           const char *separator, const char *replacement, mulib_mune_value_t *out)
  {
    mulib_mune_bytes_ module = { NULL, 0, 0 }, candidate = { NULL, 0, 0 };
    const char *text, *paths; size_t n, p, at, i, seplen = strlen (separator), replen = strlen (replacement), length;
    int found = 0;
    if (!mulib_mune_rt_text_ (s, name, 0, &name) || !mulib_mune_rt_need_ (s, path, MULIB_MUNE_STRING, 2)) return 0;
    text = mulib_mune_value_string (name, &n); paths = mulib_mune_value_string (path, &length);
    if (memchr (text, 0, n) || memchr (paths, 0, length)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "zero byte in module path");
    for (at = 0; at < n && !s->status;)
      {
        if (seplen && seplen <= n - at && !memcmp (text + at, separator, seplen))
          { (void)mulib_mune_rt_bytes_ (s, &module, replacement, replen); at += seplen; }
        else (void)mulib_mune_rt_bytes_ (s, &module, text + at++, 1);
      }
    for (p = 0; p <= length && !s->status;)
      {
        FILE *file;
        for (at = p; at < length && paths[at] != ';'; ++at) {}
        candidate.length = 0;
        for (i = p; i < at && !s->status; ++i)
          if (paths[i] == '?') (void)mulib_mune_rt_bytes_ (s, &candidate, module.data, module.length);
          else (void)mulib_mune_rt_bytes_ (s, &candidate, paths + i, 1);
        if (!s->status && candidate.length && (file = fopen (candidate.data, "rb")) != NULL)
          { (void)fclose (file); *out = mulib_mune_rt_string_ (s, candidate.data, candidate.length); found = !s->status; break; }
        if (at == length) break;
        p = at + 1;
      }
    (void)mulib_mune_rt_memory_ (s, module.data, module.capacity, 0); (void)mulib_mune_rt_memory_ (s, candidate.data, candidate.capacity, 0);
    return found;
  }
  static const char *mulib_mune_rt_cstring_ (mulib_mune_state_t *, mulib_mune_value_t, size_t);
  static int
  mulib_mune_rt_loadnative_ (mulib_mune_state_t *s, const char *path, const char *symbol, mulib_mune_result_t *out)
  {
    mulib_mune_library_ *library = (mulib_mune_library_ *)mulib_mune_rt_memory_ (s, NULL, 0, sizeof (*library));
    char message[512]; void *address = NULL; mulib_mune_object_t *fn;
    int status;
    if (!library) return 0;
    memset (library, 0, sizeof (*library));
    status = mulib_dynload_open (&library->handle, path, message, sizeof (message));
    if (!status && strcmp (symbol, "*")) status = mulib_dynload_symbol (&library->handle, symbol, &address, message, sizeof (message));
    if (status)
      {
        const char *kind = library->handle.handle ? "init" : "open";
        (void)mulib_dynload_close (&library->handle, NULL, 0); (void)mulib_mune_rt_memory_ (s, library, sizeof (*library), 0);
        return mulib_mune_result_push (out, mulib_mune_rt_nil_ ()) && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, message, strlen (message)))
               && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, kind, 4));
      }
    library->next = s->libraries; s->libraries = library;
    if (!strcmp (symbol, "*")) return mulib_mune_result_push (out, mulib_mune_rt_boolean_ (1));
    if (!address || sizeof (address) != sizeof (mulib_mune_native_fn)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "native symbol ABI unsupported");
    fn = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0); if (!fn) return 0;
    memcpy (&fn->as.function.native, &address, sizeof (address)); return mulib_mune_result_push (out, mulib_mune_rt_object_ (fn));
  }
  static int
  mulib_mune_rt_package_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_value_t name = mulib_mune_rt_arg_ (args, n, 0), package = mulib_mune_rt_field_ (s->globals, "package", 7), table, loader, path;
    if (!mulib_mune_rt_text_ (s, name, 0, &name) || !mulib_mune_rt_need_ (s, package, MULIB_MUNE_TABLE, 1)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "module name and package table required");
    if (!mulib_mune_rt_pin_ (s, name) || !mulib_mune_rt_pin_ (s, package)) return 0;
    if (!strcmp (op, "package.loadlib"))
      {
        const char *file = mulib_mune_rt_cstring_ (s, name, 1), *symbol = mulib_mune_rt_cstring_ (s, mulib_mune_rt_arg_ (args, n, 1), 2);
        return file && symbol && mulib_mune_rt_loadnative_ (s, file, symbol, out);
      }
    if (!strcmp (op, "package.native_searcher"))
      {
        mulib_mune_bytes_ symbol = { NULL, 0, 0 };
        const char *text; size_t k, size;
        table = mulib_mune_rt_field_ (package.as.object, "cpath", 5);
        if (!mulib_mune_rt_searchpath_ (s, name, table, ".", "/", &path))
          return !s->status && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "native module not found", 23));
        if (!mulib_mune_result_push (out, path)) return 0;
        text = mulib_mune_value_string (name, &size);
        (void)mulib_mune_rt_bytes_ (s, &symbol, "muneopen_", 9);
        for (k = 0; k < size && !s->status; ++k)
          { char c = text[k] == '.' ? '_' : text[k]; (void)mulib_mune_rt_bytes_ (s, &symbol, &c, 1); }
        if (!s->status) (void)mulib_mune_rt_loadnative_ (s, mulib_mune_value_string (path, NULL), symbol.data, out);
        (void)mulib_mune_rt_memory_ (s, symbol.data, symbol.capacity, 0);
        if (s->status) return 0;
        loader = mulib_mune_result_get (out, 1);
        if (loader.type != MULIB_MUNE_FUNCTION) return mulib_mune_rt_raise_ (s, mulib_mune_result_get (out, 2));
        out->values[0] = loader; out->values[1] = path; out->count = 2; return 1;
      }
    if (!strcmp (op, "package.searchpath"))
      {
        const char *sep = n > 2 && args[2].type == MULIB_MUNE_STRING ? mulib_mune_value_string (args[2], NULL) : ".";
        const char *rep = n > 3 && args[3].type == MULIB_MUNE_STRING ? mulib_mune_value_string (args[3], NULL) : "/";
        if (mulib_mune_rt_searchpath_ (s, name, mulib_mune_rt_arg_ (args, n, 1), sep, rep, &path)) return mulib_mune_result_push (out, path);
        if (s->status) return 0;
        return mulib_mune_result_push (out, mulib_mune_rt_nil_ ()) && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "module file not found", 21));
      }
    if (!strcmp (op, "package.preload_searcher"))
      {
        table = mulib_mune_rt_field_ (package.as.object, "preload", 7);
        if (!mulib_mune_rt_need_ (s, table, MULIB_MUNE_TABLE, 1)) return 0;
        loader = mulib_mune_rt_rawget_ (table.as.object, name);
        if (loader.type == MULIB_MUNE_NIL) return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "no preloaded module", 19));
        return mulib_mune_result_push (out, loader) && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, ":preload:", 9));
      }
    if (!strcmp (op, "package.file_searcher"))
      {
        mulib_mune_chunk_t *chunk; mulib_mune_error_t error;
        table = mulib_mune_rt_field_ (package.as.object, "path", 4);
        if (!mulib_mune_rt_searchpath_ (s, name, table, ".", "/", &path))
          return !s->status && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "module file not found", 21));
        if (!mulib_mune_result_push (out, path)) return 0;
        {
          FILE *file = fopen (mulib_mune_value_string (path, NULL), "rb"); unsigned char magic[8]; size_t got = 0;
          if (file) { got = fread (magic, 1, sizeof (magic), file); (void)fclose (file); }
          if (got == 8 && !memcmp (magic, "MUNEBC\0", 7) && (magic[7] == 1 || magic[7] == 2))
            {
              if (mulib_mune_load_bundle_file (s, mulib_mune_value_string (path, NULL), &error) != MULIB_MUNE_OK) return 0;
              table = mulib_mune_rt_field_ (package.as.object, "preload", 7); loader = mulib_mune_rt_rawget_ (table.as.object, name);
              if (loader.type == MULIB_MUNE_NIL) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "bundle does not contain requested module");
              out->values[0] = loader; return mulib_mune_result_push (out, path);
            }
        }
        chunk = mulib_mune_parse_file (mulib_mune_value_string (path, NULL), NULL, &error);
        if (!chunk) return mulib_mune_rt_error_ (s, error.code, "%s", error.message);
        loader = mulib_mune_rt_loaded_ (s, chunk, mulib_mune_rt_object_ (s->globals));
        out->values[0] = loader; return !s->status && mulib_mune_result_push (out, path);
      }
    if (!strcmp (op, "require"))
      {
        mulib_mune_value_t loaded = mulib_mune_rt_field_ (package.as.object, "loaded", 6), searchers = mulib_mune_rt_field_ (package.as.object, "searchers", 9);
        int64_t index;
        if (!mulib_mune_rt_need_ (s, loaded, MULIB_MUNE_TABLE, 1) || !mulib_mune_rt_need_ (s, searchers, MULIB_MUNE_TABLE, 1)) return 0;
        if (!mulib_mune_rt_pin_ (s, loaded) || !mulib_mune_rt_pin_ (s, searchers)) return 0;
        loader = mulib_mune_rt_rawget_ (loaded.as.object, name);
        if (mulib_mune_rt_truth_ (loader)) return mulib_mune_result_push (out, loader);
        for (index = 1; index < 10000; ++index)
          {
            mulib_mune_result_t *found, *value;
            loader = mulib_mune_rt_rawget_ (searchers.as.object, mulib_mune_rt_integer_ (index));
            if (loader.type == MULIB_MUNE_NIL) break;
            found = mulib_mune_rt_metacall_ (s, loader, name, name, name, 1); if (!found) return 0;
            loader = mulib_mune_result_get (found, 0); path = mulib_mune_result_get (found, 1);
            if (loader.type != MULIB_MUNE_FUNCTION) { mulib_mune_result_destroy (found); continue; }
            value = mulib_mune_rt_metacall_ (s, loader, name, path, path, 2);
            if (!value) { mulib_mune_result_destroy (found); return 0; }
            loader = mulib_mune_result_get (value, 0);
            if (loader.type != MULIB_MUNE_NIL) (void)mulib_mune_rt_rawset_ (s, loaded.as.object, name, loader);
            else
              {
                loader = mulib_mune_rt_rawget_ (loaded.as.object, name);
                if (loader.type == MULIB_MUNE_NIL) { loader = mulib_mune_rt_boolean_ (1); (void)mulib_mune_rt_rawset_ (s, loaded.as.object, name, loader); }
              }
            (void)mulib_mune_result_push (out, loader); (void)mulib_mune_result_push (out, path);
            mulib_mune_result_destroy (value); mulib_mune_result_destroy (found); return !s->status;
          }
        return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "module '%s' not found", mulib_mune_value_string (name, NULL));
      }
    return mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "unknown package operation");
  }
  static int
  mulib_mune_bundle_uint_ (mulib_mir_write_fn writer, void *user, uint64_t value, size_t n)
  {
    char bytes[8]; size_t k; for (k = 0; k < n; ++k) bytes[k] = (char)(value >> (8 * k)); return writer (user, bytes, n);
  }
  static mulib_mune_status_t
  mulib_mune_bundle_prepare_ (mulib_mune_chunk_t *chunk)
  {
    mulib_mune_plan_ *p;
    if (!mulib_mune_prepare_blocks_ (chunk, chunk->walker->root)) return MULIB_MUNE_MEMORY;
    for (p = chunk->plans; p; p = p->next)
      {
        mulib_mir_status_t status = mulib_vmweave_compile (&p->source, 1000000, 1, &p->vm, NULL);
        if (status != MULIB_MIR_OK) return status == MULIB_MIR_NO_MEMORY ? MULIB_MUNE_MEMORY : MULIB_MUNE_LIMIT;
      }
    return MULIB_MUNE_OK;
  }
  static int
  mulib_mune_bundle_image_ (const mulib_mune_plan_ *p, mulib_mir_write_fn writer, void *user)
  {
    const mulib_mir_flow_program_t *image = mulib_vmweave_program_view (p->vm);
    size_t k, length = strlen (p->block->production);
    if (!mulib_mune_bundle_uint_ (writer, user, p->block->offset, 8)
        || !mulib_mune_bundle_uint_ (writer, user, length, 1) || !writer (user, p->block->production, length)
        || !mulib_mune_bundle_uint_ (writer, user, image->register_count, 8)
        || !mulib_mune_bundle_uint_ (writer, user, image->count, 8)
        || !mulib_mune_bundle_uint_ (writer, user, image->host_count, 8)) return 0;
    for (k = 0; k < image->register_count; ++k)
      if (!mulib_mune_bundle_uint_ (writer, user, image->register_types[k], 1)) return 0;
    for (k = 0; k < image->count; ++k)
      {
        const mulib_mir_flow_instruction_t *i = image->code + k;
        if (!mulib_mune_bundle_uint_ (writer, user, i->opcode, 1)
            || !mulib_mune_bundle_uint_ (writer, user, i->destination == MULIB_MIR_NONE ? UINT64_MAX : (uint64_t)i->destination, 8)
            || !mulib_mune_bundle_uint_ (writer, user, i->a == MULIB_MIR_NONE ? UINT64_MAX : (uint64_t)i->a, 8)
            || !mulib_mune_bundle_uint_ (writer, user, i->b == MULIB_MIR_NONE ? UINT64_MAX : (uint64_t)i->b, 8)
            || !mulib_mune_bundle_uint_ (writer, user, i->target == MULIB_MIR_NONE ? UINT64_MAX : (uint64_t)i->target, 8)
            || !mulib_mune_bundle_uint_ (writer, user, (uint64_t)i->immediate.integer, 8)) return 0;
      }
    return 1;
  }
  mulib_mune_status_t
  mulib_mune_bundle_write (const mulib_mune_bundle_module_t *modules, size_t count, mulib_mir_write_fn writer, void *user, mulib_mune_error_t *error)
  {
    size_t k, j; mulib_mune_error_t e = { MULIB_MUNE_OK, 0, "ok" };
    mulib_mune_chunk_t **chunks = NULL;
    if ((!modules && count) || !writer || count > UINT32_MAX || count > SIZE_MAX / sizeof (*chunks)) { e.code = MULIB_MUNE_INVALID; goto done; }
    if (count) { chunks = (mulib_mune_chunk_t **)calloc (count, sizeof (*chunks)); if (!chunks) { e.code = MULIB_MUNE_MEMORY; goto done; } }
    for (k = 0; k < count; ++k)
      {
        if (!modules[k].name || !*modules[k].name || strlen (modules[k].name) > UINT32_MAX) { e.code = MULIB_MUNE_INVALID; goto done; }
        for (j = 0; j < k; ++j) if (!strcmp (modules[j].name, modules[k].name)) { e.code = MULIB_MUNE_INVALID; goto done; }
        chunks[k] = mulib_mune_parse (modules[k].source, modules[k].length, NULL, &e); if (!chunks[k]) goto done;
        e.code = mulib_mune_bundle_prepare_ (chunks[k]); if (e.code) goto done;
      }
    if (!writer (user, "MUNEBC\0\2", 8) || !mulib_mune_bundle_uint_ (writer, user, count, 4)) { e.code = MULIB_MUNE_IO; goto done; }
    for (k = 0; k < count; ++k)
      {
        size_t length = strlen (modules[k].name), plans = 0;
        mulib_mune_plan_ *p;
        for (p = chunks[k]->plans; p; p = p->next) ++plans;
        if (!mulib_mune_bundle_uint_ (writer, user, length, 4) || !mulib_mune_bundle_uint_ (writer, user, modules[k].length, 8)
            || !writer (user, modules[k].name, length) || !writer (user, modules[k].source, modules[k].length)
            || !mulib_mune_bundle_uint_ (writer, user, plans, 8)) { e.code = MULIB_MUNE_IO; goto done; }
        for (p = chunks[k]->plans; p; p = p->next)
          if (!mulib_mune_bundle_image_ (p, writer, user)) { e.code = MULIB_MUNE_IO; goto done; }
      }
  done:
    if (chunks) { for (k = 0; k < count; ++k) mulib_mune_chunk_destroy (chunks[k]); free (chunks); }
    if (e.code == MULIB_MUNE_INVALID) e.message = "invalid module bundle description";
    if (e.code == MULIB_MUNE_IO) e.message = "bundle writer failed";
    if (e.code == MULIB_MUNE_MEMORY) e.message = "bundle compiler allocation failed";
    if (e.code == MULIB_MUNE_LIMIT) e.message = "bundle compilation limit";
    if (error) *error = e;
    return e.code;
  }
  static uint64_t
  mulib_mune_bundle_readuint_ (const unsigned char *bytes, size_t n)
  {
    uint64_t v = 0; size_t k; for (k = 0; k < n; ++k) v |= (uint64_t)bytes[k] << (k * 8); return v;
  }
  typedef struct mulib_mune_bundle_verify_
  {
    const unsigned char *bytes;
    size_t length, at;
  } mulib_mune_bundle_verify_;
  static int
  mulib_mune_bundle_compare_ (void *user, const char *bytes, size_t length)
  {
    mulib_mune_bundle_verify_ *v = (mulib_mune_bundle_verify_ *)user;
    if (length > v->length - v->at || memcmp (v->bytes + v->at, bytes, length)) return 0;
    v->at += length; return 1;
  }
  static mulib_mune_status_t
  mulib_mune_bundle_readimages_ (mulib_mune_chunk_t *chunk, const unsigned char *bytes, size_t length, size_t *offset)
  {
    mulib_mune_bundle_verify_ verify;
    mulib_mune_plan_ *p;
    mulib_mune_status_t status = mulib_mune_bundle_prepare_ (chunk);
    size_t plans = 0;
    if (status) return status;
    for (p = chunk->plans; p; p = p->next) ++plans;
    verify.bytes = bytes; verify.length = length; verify.at = *offset;
    if (!mulib_mune_bundle_uint_ (mulib_mune_bundle_compare_, &verify, plans, 8)) return MULIB_MUNE_INVALID;
    for (p = chunk->plans; p; p = p->next)
      {
        size_t at = verify.at + 33 + strlen (p->block->production) + p->source.register_count, k;
        /* Verify canonical typed code against the checked source/helper map.
         * Bytecode cannot select an unchecked AST node or forge a host slot.
         * Only verified counts are used for decoding or allocation. */
        if (!mulib_mune_bundle_image_ (p, mulib_mune_bundle_compare_, &verify)) return MULIB_MUNE_INVALID;
        for (k = 0; k < p->source.count; ++k)
          {
            mulib_mir_flow_instruction_t *i = p->code + k;
            i->opcode = (mulib_mir_flow_opcode_t)bytes[at++];
            i->destination = (size_t)mulib_mune_bundle_readuint_ (bytes + at, 8); at += 8;
            i->a = (size_t)mulib_mune_bundle_readuint_ (bytes + at, 8); at += 8;
            i->b = (size_t)mulib_mune_bundle_readuint_ (bytes + at, 8); at += 8;
            i->target = (size_t)mulib_mune_bundle_readuint_ (bytes + at, 8); at += 8;
            i->immediate.integer = mulib_mune_rt_signed_ (mulib_mune_bundle_readuint_ (bytes + at, 8)); at += 8;
          }
        mulib_vmweave_program_destroy (p->vm); p->vm = NULL;
      }
    *offset = verify.at; return MULIB_MUNE_OK;
  }
  mulib_mune_status_t
  mulib_mune_load_bundle (mulib_mune_state_t *s, const void *data, size_t length, mulib_mune_error_t *error)
  {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t at = 12, k, count;
    mulib_mune_result_t *roots = NULL;
    mulib_mune_value_t package, preload;
    if (!s || !bytes || length < 12 || memcmp (bytes, "MUNEBC\0", 7) || (bytes[7] != 1 && bytes[7] != 2))
      { if (error) { error->code = MULIB_MUNE_INVALID; error->offset = 0; error->message = "invalid bundle header"; } return MULIB_MUNE_INVALID; }
    if (!s->running) s->status = MULIB_MUNE_OK;
    count = (size_t)mulib_mune_bundle_readuint_ (bytes + 8, 4);
    if (count > (length - 12) / 13 || count > s->limits.results / 2) { mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "bundle module limit"); goto done; }
    roots = mulib_mune_rt_results_ (s); if (!roots) goto done;
    for (k = 0; k < count; ++k)
      {
        uint64_t source_length; size_t name_length, j;
        mulib_mune_chunk_t *chunk; mulib_mune_error_t e;
        mulib_mune_value_t name, fn;
        if (length - at < 12) goto malformed;
        name_length = (size_t)mulib_mune_bundle_readuint_ (bytes + at, 4); source_length = mulib_mune_bundle_readuint_ (bytes + at + 4, 8); at += 12;
        if (!name_length || name_length > length - at || source_length > length - at - name_length || memchr (bytes + at, 0, name_length)) goto malformed;
        name = mulib_mune_rt_string_ (s, (const char *)bytes + at, name_length); if (s->status) goto done;
        for (j = 0; j < roots->count; j += 2) if (mulib_mune_rt_equal_ (name, roots->values[j])) goto malformed;
        at += name_length;
        chunk = mulib_mune_parse ((const char *)bytes + at, (size_t)source_length, NULL, &e);
        if (!chunk) { mulib_mune_rt_error_ (s, e.code, "%s", e.message); goto done; }
        at += (size_t)source_length;
        if (bytes[7] == 2)
          {
            mulib_mune_status_t status = mulib_mune_bundle_readimages_ (chunk, bytes, length, &at);
            if (status)
              { mulib_mune_chunk_destroy (chunk); mulib_mune_rt_error_ (s, status, "invalid or unavailable compiled bundle image"); goto done; }
          }
        fn = mulib_mune_rt_loaded_ (s, chunk, mulib_mune_rt_object_ (s->globals));
        if (s->status || !mulib_mune_result_push (roots, name) || !mulib_mune_result_push (roots, fn)) goto done;
      }
    if (at != length) goto malformed;
    package = mulib_mune_rt_field_ (s->globals, "package", 7);
    if (!mulib_mune_rt_need_ (s, package, MULIB_MUNE_TABLE, 1)) goto done;
    preload = mulib_mune_rt_field_ (package.as.object, "preload", 7);
    if (!mulib_mune_rt_need_ (s, preload, MULIB_MUNE_TABLE, 1)) goto done;
    for (k = 0; k < roots->count; k += 2) if (!mulib_mune_rt_rawset_ (s, preload.as.object, roots->values[k], roots->values[k + 1])) goto done;
    goto done;
  malformed:
    mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "malformed module bundle");
  done:
    mulib_mune_result_destroy (roots);
    if (error) { error->code = s->status; error->offset = at; error->message = s->status ? s->message : "ok"; }
    return s->status;
  }
  mulib_mune_status_t
  mulib_mune_load_bundle_file (mulib_mune_state_t *s, const char *path, mulib_mune_error_t *error)
  {
    mulib_mune_bytes_ bytes = { NULL, 0, 0 }; mulib_mune_status_t status;
    if (!s || !path) return MULIB_MUNE_INVALID;
    if (!s->running) s->status = MULIB_MUNE_OK;
    if (mulib_mune_rt_readfile_ (s, path, &bytes)) status = mulib_mune_load_bundle (s, bytes.data, bytes.length, error);
    else { status = s->status; if (error) { error->code = status; error->offset = 0; error->message = s->message; } }
    (void)mulib_mune_rt_memory_ (s, bytes.data, bytes.capacity, 0); return status;
  }
  static size_t
  mulib_mune_rt_utfdecode_ (const unsigned char *text, size_t length, uint32_t *code, int lax)
  {
    uint32_t v, minimum; size_t n, k;
    if (!length) return 0;
    if (text[0] < 128) { *code = text[0]; return 1; }
    if (text[0] >= 0xc2 && text[0] < 0xe0) { n = 2; v = text[0] & 31; minimum = 128; }
    else if (text[0] < 0xf0 && text[0] >= 0xe0) { n = 3; v = text[0] & 15; minimum = 2048; }
    else if (text[0] < 0xf8 && text[0] >= 0xf0) { n = 4; v = text[0] & 7; minimum = 65536; }
    else if (text[0] < 0xfc && text[0] >= 0xf8) { n = 5; v = text[0] & 3; minimum = 0x200000; }
    else if (text[0] < 0xfe && text[0] >= 0xfc) { n = 6; v = text[0] & 1; minimum = 0x4000000; }
    else return 0;
    if (n > length) return 0;
    for (k = 1; k < n; ++k) { if ((text[k] & 0xc0) != 0x80) return 0; v = (v << 6) | (text[k] & 63); }
    if (v < minimum || (!lax && (v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff)))) return 0;
    *code = v; return n;
  }
  static int
  mulib_mune_rt_utf8_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0);
    const unsigned char *text; size_t length, at, size, count = 0;
    int64_t begin = 1, end, amount = 0;
    int lax = 0; uint32_t code;
    if (!strcmp (op, "char"))
      {
        mulib_mune_bytes_ bytes = { NULL, 0, 0 };
        for (at = 0; at < n && !s->status; ++at)
          {
            unsigned char encoded[6]; size_t used = 0, j; uint32_t value, limit;
            if (!mulib_mune_rt_need_int_ (s, args[at], &amount, at + 1)) break;
            if (amount < 0 || amount > INT32_MAX) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "codepoint out of range"); break; }
            value = (uint32_t)amount;
            if (value < 128) { encoded[0] = (unsigned char)value; used = 1; }
            else
              {
                unsigned char reverse[6]; limit = 31;
                do { reverse[used++] = (unsigned char)(0x80 | (value & 63)); value >>= 6; limit >>= 1; } while (value > limit);
                encoded[0] = (unsigned char)((0xffu << (7 - used)) | value);
                for (j = 0; j < used; ++j) encoded[j + 1] = reverse[used - j - 1];
                ++used;
              }
            (void)mulib_mune_rt_bytes_ (s, &bytes, (const char *)encoded, used);
          }
        if (!s->status) (void)mulib_mune_result_push (out, mulib_mune_rt_string_ (s, bytes.data, bytes.length));
        (void)mulib_mune_rt_memory_ (s, bytes.data, bytes.capacity, 0); return !s->status;
      }
    if (!mulib_mune_rt_text_ (s, a, 0, &a)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "string expected");
    text = (const unsigned char *)mulib_mune_value_string (a, &length); end = (int64_t)length;
    if (!strcmp (op, "codes"))
      {
        mulib_mune_object_t *f = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0); if (!f) return 0;
        f->as.function.native = mulib_mune_rt_builtin_; f->as.function.user = (void *)"utf8.iterator";
        f->as.function.context[0] = n > 1 ? args[1] : mulib_mune_rt_boolean_ (0);
        return mulib_mune_result_push (out, mulib_mune_rt_object_ (f)) && mulib_mune_result_push (out, a) && mulib_mune_result_push (out, mulib_mune_rt_integer_ (0));
      }
    if (!strcmp (op, "iterator"))
      {
        if (!mulib_mune_rt_need_int_ (s, mulib_mune_rt_arg_ (args, n, 1), &begin, 2) || begin < 0) return 0;
        at = (size_t)begin;
        if (at) while (at < length && (text[at] & 0xc0) == 0x80) ++at;
        if (at >= length) return 1;
        lax = mulib_mune_rt_truth_ (s->frame->function.as.object->as.function.context[0]);
        if (!mulib_mune_rt_utfdecode_ (text + at, length - at, &code, lax)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid UTF-8 code");
        return mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)at + 1)) && mulib_mune_result_push (out, mulib_mune_rt_integer_ (code));
      }
    if (!strcmp (op, "offset"))
      {
        if (!mulib_mune_rt_need_int_ (s, mulib_mune_rt_arg_ (args, n, 1), &amount, 2)) return 0;
        begin = amount >= 0 ? 1 : (int64_t)length + 1;
        if (n > 2 && args[2].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[2], &begin, 3)) return 0;
        begin = mulib_mune_rt_relative_ (begin, length);
        if (begin < 1 || (uint64_t)begin > length + 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "initial position out of range");
        at = (size_t)begin - 1;
        if (!amount) { while (at && at < length && (text[at] & 0xc0) == 0x80) --at; }
        else
          {
            if (at < length && (text[at] & 0xc0) == 0x80) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "initial position is a continuation byte");
            if (amount > 0)
              { --amount; while (amount > 0 && at < length) { do { ++at; } while (at < length && (text[at] & 0xc0) == 0x80); --amount; } }
            else while (amount < 0 && at) { do { --at; } while (at && (text[at] & 0xc0) == 0x80); ++amount; }
          }
        return mulib_mune_result_push (out, amount ? mulib_mune_rt_nil_ () : mulib_mune_rt_integer_ ((int64_t)at + 1));
      }
    if (n > 1 && args[1].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[1], &begin, 2)) return 0;
    if (!strcmp (op, "codepoint")) end = begin;
    if (n > 2 && args[2].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[2], &end, 3)) return 0;
    if (n > 3) lax = mulib_mune_rt_truth_ (args[3]);
    begin = mulib_mune_rt_relative_ (begin, length); end = mulib_mune_rt_relative_ (end, length);
    if (begin < 1 || (uint64_t)begin > length + 1 || end > (int64_t)length) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "UTF-8 range out of bounds");
    for (at = (size_t)begin - 1; end > 0 && at < (size_t)end; at += size)
      {
        size = mulib_mune_rt_utfdecode_ (text + at, length - at, &code, lax);
        if (!size)
          {
            if (strcmp (op, "len")) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid UTF-8 code");
            return mulib_mune_result_push (out, mulib_mune_rt_nil_ ()) && mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)at + 1));
          }
        ++count;
        if (!strcmp (op, "codepoint") && !mulib_mune_result_push (out, mulib_mune_rt_integer_ (code))) return 0;
      }
    return strcmp (op, "len") || mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)count));
  }
  static const char *
  mulib_mune_rt_cstring_ (mulib_mune_state_t *s, mulib_mune_value_t v, size_t i)
  {
    const char *text;
    if (v.type != MULIB_MUNE_STRING)
      {
        if (!mulib_mune_rt_text_ (s, v, 0, &v)) { (void)mulib_mune_rt_need_ (s, v, MULIB_MUNE_STRING, i); return NULL; }
        if (s->frame && !mulib_mune_rt_pin_ (s, v)) return NULL;
      }
    text = (const char *)(v.as.object + 1);
    if (memchr (text, 0, v.as.object->as.length)) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "zero byte in C string argument"); return NULL; }
    return text;
  }
  static int
  mulib_mune_rt_file_result_ (mulib_mune_state_t *s, mulib_mune_result_t *out, int ok, int error)
  {
    if (ok) return mulib_mune_result_push (out, mulib_mune_rt_boolean_ (1));
    return mulib_mune_result_push (out, mulib_mune_rt_nil_ ())
           && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, strerror (error), strlen (strerror (error))))
           && mulib_mune_result_push (out, mulib_mune_rt_integer_ (error));
  }
  static int
  mulib_mune_rt_tmfield_ (mulib_mune_state_t *s, mulib_mune_object_t *table, const char *name, int default_value, int required, int *out)
  {
    mulib_mune_value_t v = mulib_mune_rt_field_ (table, name, strlen (name)); int64_t i;
    if (v.type == MULIB_MUNE_NIL && !required) { *out = default_value; return 1; }
    if (!mulib_mune_rt_need_int_ (s, v, &i, 1)) return 0;
    if (i < INT_MIN / 2 || i > INT_MAX / 2) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "date field out of range");
    *out = (int)i; return 1;
  }
  static int
  mulib_mune_rt_date_table_ (mulib_mune_state_t *s, mulib_mune_object_t *t, const struct tm *tm)
  {
    static const char *const fields[] = {"year","month","day","hour","min","sec","wday","yday"};
    int values[8]; size_t k;
    values[0] = tm->tm_year + 1900; values[1] = tm->tm_mon + 1; values[2] = tm->tm_mday; values[3] = tm->tm_hour;
    values[4] = tm->tm_min; values[5] = tm->tm_sec; values[6] = tm->tm_wday + 1; values[7] = tm->tm_yday + 1;
    for (k = 0; k < 8; ++k) if (!mulib_mune_rt_setfield_ (s, t, fields[k], strlen (fields[k]), mulib_mune_rt_integer_ (values[k]))) return 0;
    return tm->tm_isdst < 0 || mulib_mune_rt_setfield_ (s, t, "isdst", 5, mulib_mune_rt_boolean_ (tm->tm_isdst));
  }
  static int
  mulib_mune_rt_os_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0), b = mulib_mune_rt_arg_ (args, n, 1);
    const char *text;
    if (!strcmp (op, "clock")) return mulib_mune_result_push (out, mulib_mune_rt_number_ ((double)clock () / CLOCKS_PER_SEC));
    if (!strcmp (op, "time"))
      {
        time_t value;
        if (a.type == MULIB_MUNE_NIL) value = time (NULL);
        else
          {
            struct tm tm; mulib_mune_value_t dst;
            if (!mulib_mune_rt_need_ (s, a, MULIB_MUNE_TABLE, 1)) return 0;
            memset (&tm, 0, sizeof (tm));
            if (!mulib_mune_rt_tmfield_ (s, a.as.object, "year", 0, 1, &tm.tm_year)
                || !mulib_mune_rt_tmfield_ (s, a.as.object, "month", 0, 1, &tm.tm_mon)
                || !mulib_mune_rt_tmfield_ (s, a.as.object, "day", 0, 1, &tm.tm_mday)
                || !mulib_mune_rt_tmfield_ (s, a.as.object, "hour", 12, 0, &tm.tm_hour)
                || !mulib_mune_rt_tmfield_ (s, a.as.object, "min", 0, 0, &tm.tm_min)
                || !mulib_mune_rt_tmfield_ (s, a.as.object, "sec", 0, 0, &tm.tm_sec)) return 0;
            tm.tm_year -= 1900; --tm.tm_mon; dst = mulib_mune_rt_field_ (a.as.object, "isdst", 5);
            tm.tm_isdst = dst.type == MULIB_MUNE_NIL ? -1 : mulib_mune_rt_truth_ (dst);
            value = mktime (&tm); if (!mulib_mune_rt_date_table_ (s, a.as.object, &tm)) return 0;
          }
        if (value == (time_t)-1) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "time outside supported range");
        return mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)value));
      }
    if (!strcmp (op, "date"))
      {
        struct tm *value, copy; time_t timestamp = time (NULL); int64_t number;
        const char *format = a.type == MULIB_MUNE_NIL ? "%c" : mulib_mune_rt_cstring_ (s, a, 1);
        if (!format) return 0;
        if (b.type != MULIB_MUNE_NIL) { if (!mulib_mune_rt_need_int_ (s, b, &number, 2)) return 0; timestamp = (time_t)number; }
        if (*format == '!') { ++format; value = gmtime (&timestamp); } else value = localtime (&timestamp);
        if (!value) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "date outside supported range");
        copy = *value;
        if (!strcmp (format, "*t"))
          { mulib_mune_object_t *t = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0); return t && mulib_mune_rt_date_table_ (s, t, &copy) && mulib_mune_result_push (out, mulib_mune_rt_object_ (t)); }
        else
          {
            char buffer[4096]; size_t length;
            if (!*format) return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "", 0));
            length = strftime (buffer, sizeof (buffer), format, &copy);
            if (!length) return mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "formatted date too long");
            return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, buffer, length));
          }
      }
    if (!strcmp (op, "difftime"))
      { int64_t x, y; return mulib_mune_rt_need_int_ (s, a, &x, 1) && mulib_mune_rt_need_int_ (s, b, &y, 2) && mulib_mune_result_push (out, mulib_mune_rt_number_ (difftime ((time_t)x, (time_t)y))); }
    if (!strcmp (op, "getenv"))
      { text = mulib_mune_rt_cstring_ (s, a, 1); if (!text) return 0; text = getenv (text); return mulib_mune_result_push (out, text ? mulib_mune_rt_string_ (s, text, strlen (text)) : mulib_mune_rt_nil_ ()); }
    if (!strcmp (op, "remove") || !strcmp (op, "rename"))
      {
        int result; text = mulib_mune_rt_cstring_ (s, a, 1); if (!text) return 0;
        if (!strcmp (op, "rename")) { const char *to = mulib_mune_rt_cstring_ (s, b, 2); if (!to) return 0; result = rename (text, to); }
        else result = remove (text);
        return mulib_mune_rt_file_result_ (s, out, result == 0, errno);
      }
    if (!strcmp (op, "execute"))
      {
        int result;
        if (a.type == MULIB_MUNE_NIL) return mulib_mune_result_push (out, mulib_mune_rt_boolean_ (system (NULL) != 0));
        text = mulib_mune_rt_cstring_ (s, a, 1); if (!text) return 0; result = system (text);
        if (result == -1) return mulib_mune_rt_file_result_ (s, out, 0, errno);
#if defined(__unix__)
        if (WIFSIGNALED (result))
          return mulib_mune_result_push (out, mulib_mune_rt_nil_ ()) && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "signal", 6)) && mulib_mune_result_push (out, mulib_mune_rt_integer_ (WTERMSIG (result)));
        result = WEXITSTATUS (result);
#endif
        return mulib_mune_result_push (out, result ? mulib_mune_rt_nil_ () : mulib_mune_rt_boolean_ (1)) && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "exit", 4)) && mulib_mune_result_push (out, mulib_mune_rt_integer_ (result));
      }
    if (!strcmp (op, "exit"))
      {
        int64_t code = 0;
        if (a.type == MULIB_MUNE_BOOLEAN) code = a.as.boolean ? EXIT_SUCCESS : EXIT_FAILURE;
        else if (a.type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, a, &code, 1)) return 0;
        if (mulib_mune_rt_truth_ (b))
          {
            mulib_mune_frame_ *f = s->current_thread == s->main_thread ? s->frame : s->main_thread->as.thread->frame;
            mulib_mune_object_t *o;
            mulib_mune_protected_ protect;
            protect.previous = NULL; protect.handler = mulib_mune_rt_nil_ (); protect.handled = 0;
            s->collecting = 1; s->protected_call = &protect;
            for (; f; f = f->previous)
              {
                s->frame = f; s->status = MULIB_MUNE_OK; s->has_error = 0; s->error_value = mulib_mune_rt_nil_ ();
                mulib_mune_rt_close_ (s, f->function.type == MULIB_MUNE_FUNCTION ? f->function.as.object->as.function.environment : NULL);
              }
            s->frame = NULL; s->status = MULIB_MUNE_OK; mulib_mune_rt_finalizers_ (s, 1);
            for (o = s->objects; o; o = o->next)
              if (o->kind == MULIB_MUNE_USERDATA && o->userdata_tag == 1 && o->as.file.stream && o->as.file.owned)
                (void)mulib_mune_rt_file_close_ (o);
          }
        exit ((int)code);
      }
    if (!strcmp (op, "setlocale"))
      {
        static const char *const names[] = { "all", "collate", "ctype", "monetary", "numeric", "time" };
        static const int categories[] = { LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY, LC_NUMERIC, LC_TIME };
        const char *category = b.type == MULIB_MUNE_NIL ? "all" : mulib_mune_rt_cstring_ (s, b, 2); size_t k;
        text = a.type == MULIB_MUNE_NIL ? NULL : mulib_mune_rt_cstring_ (s, a, 1); if (s->status) return 0;
        for (k = 0; k < 6; ++k) if (!strcmp (category, names[k])) break;
        if (k == 6) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid locale category");
        text = setlocale (categories[k], text); return mulib_mune_result_push (out, text ? mulib_mune_rt_string_ (s, text, strlen (text)) : mulib_mune_rt_nil_ ());
      }
    if (!strcmp (op, "tmpname"))
      {
        const char *directory = getenv ("TMPDIR"); char path[1024]; size_t k;
        if (!directory) directory = "/tmp";
        for (k = 0; k < 100; ++k)
          {
            FILE *file; int written = snprintf (path, sizeof (path), "%s/mune-%" PRIx64, directory, mulib_mune_rt_random_ (s));
            if (written < 0 || (size_t)written >= sizeof (path)) break;
            file = fopen (path, "wx"); if (file) { (void)fclose (file); return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, path, (size_t)written)); }
          }
        return mulib_mune_rt_error_ (s, MULIB_MUNE_IO, "could not create temporary name");
      }
    return mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "unknown OS function");
  }
#if defined(__unix__) || defined(__APPLE__)
  extern FILE *popen (const char *, const char *);
  extern int pclose (FILE *);
#endif
  static int
  mulib_mune_rt_file_close_ (mulib_mune_object_t *file)
  {
    int result;
    if (file->as.file.owned == 2)
      {
#if defined(__unix__) || defined(__APPLE__)
        result = pclose (file->as.file.stream);
#elif defined(_WIN32)
        result = _pclose (file->as.file.stream);
#else
        result = -1;
#endif
      }
    else result = fclose (file->as.file.stream);
    file->as.file.stream = NULL; file->as.file.closed = 1; return result;
  }
  static mulib_mune_value_t
  mulib_mune_rt_file_ (mulib_mune_state_t *s, FILE *stream, int owned)
  {
    mulib_mune_object_t *o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_USERDATA, 0);
    if (!o)
      {
        if (owned)
          {
            mulib_mune_object_t temporary;
            memset (&temporary, 0, sizeof (temporary)); temporary.as.file.stream = stream; temporary.as.file.owned = owned;
            (void)mulib_mune_rt_file_close_ (&temporary);
          }
        return mulib_mune_rt_nil_ ();
      }
    o->userdata_tag = 1; o->as.file.stream = stream; o->as.file.owned = owned; o->metatable = s->file_metatable;
    mulib_mune_rt_finalize_register_ (s, o); return mulib_mune_rt_object_ (o);
  }
  static int
  mulib_mune_rt_io_init_ (mulib_mune_state_t *s)
  {
    static const char *const methods[] = { "close", "flush", "lines", "read", "seek", "setvbuf", "write" };
    static const char *const operations[] = { "file.close", "file.flush", "file.lines", "file.read", "file.seek", "file.setvbuf", "file.write" };
    mulib_mune_object_t *meta = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0);
    mulib_mune_value_t io = mulib_mune_rt_field_ (s->globals, "io", 2); size_t k;
    if (!meta) return 0;
    s->file_metatable = meta;
    if (!mulib_mune_rt_setfield_ (s, meta, "__index", 7, mulib_mune_rt_object_ (meta))
        || !mulib_mune_rt_setfield_ (s, meta, "__name", 6, mulib_mune_rt_string_ (s, "FILE*", 5))
        || !mulib_mune_rt_install_ (s, meta, "__close", "file.finalize")
        || !mulib_mune_rt_install_ (s, meta, "__gc", "file.finalize")
        || !mulib_mune_rt_install_ (s, meta, "__tostring", "file.tostring")) return 0;
    for (k = 0; k < 7; ++k) if (!mulib_mune_rt_install_ (s, meta, methods[k], operations[k])) return 0;
    s->input = mulib_mune_rt_file_ (s, stdin, 0); s->output = mulib_mune_rt_file_ (s, stdout, 0);
    return !s->status && mulib_mune_rt_setfield_ (s, io.as.object, "stdin", 5, s->input)
        && mulib_mune_rt_setfield_ (s, io.as.object, "stdout", 6, s->output)
        && mulib_mune_rt_setfield_ (s, io.as.object, "stderr", 6, mulib_mune_rt_file_ (s, stderr, 0));
  }
  static int
  mulib_mune_rt_read_number_ (mulib_mune_state_t *s, FILE *file, mulib_mune_result_t *out)
  {
    mulib_mune_bytes_ bytes = { NULL, 0, 0 };
    mulib_mune_value_t value;
    int c, hex = 0;
    do { c = fgetc (file); } while (c != EOF && mulib_mune_space_ ((unsigned char)c));
#define MULIB_MUNE_READ_DIGIT_() do { char byte = (char)c; if (!mulib_mune_rt_bytes_ (s, &bytes, &byte, 1)) goto done; c = fgetc (file); } while (0)
    if (c == '-' || c == '+') MULIB_MUNE_READ_DIGIT_ ();
    if (c == '0') { MULIB_MUNE_READ_DIGIT_ (); if (c == 'x' || c == 'X') { hex = 1; MULIB_MUNE_READ_DIGIT_ (); } }
    while (c != EOF && (hex ? mulib_mune_hex_ ((unsigned char)c) >= 0 : mulib_mune_digit_ ((unsigned char)c))) MULIB_MUNE_READ_DIGIT_ ();
    if (c == '.')
      {
        MULIB_MUNE_READ_DIGIT_ ();
        while (c != EOF && (hex ? mulib_mune_hex_ ((unsigned char)c) >= 0 : mulib_mune_digit_ ((unsigned char)c))) MULIB_MUNE_READ_DIGIT_ ();
      }
    if (hex ? c == 'p' || c == 'P' : c == 'e' || c == 'E')
      {
        MULIB_MUNE_READ_DIGIT_ (); if (c == '-' || c == '+') MULIB_MUNE_READ_DIGIT_ ();
        while (c != EOF && mulib_mune_digit_ ((unsigned char)c)) MULIB_MUNE_READ_DIGIT_ ();
      }
    if (c != EOF) (void)ungetc (c, file);
    if (!bytes.length || !mulib_mune_rt_parse_number_ (bytes.data, bytes.length, &value)) value = mulib_mune_rt_nil_ ();
    (void)mulib_mune_result_push (out, value);
  done:
    (void)mulib_mune_rt_memory_ (s, bytes.data, bytes.capacity, 0); return !s->status;
#undef MULIB_MUNE_READ_DIGIT_
  }
  static int
  mulib_mune_rt_read_ (mulib_mune_state_t *s, FILE *file, mulib_mune_value_t format, mulib_mune_result_t *out)
  {
    mulib_mune_bytes_ bytes = { NULL, 0, 0 }; int c = EOF, eof = 0;
    int64_t amount = INT64_MAX; const char *what = "l";
    if (format.type == MULIB_MUNE_INTEGER || format.type == MULIB_MUNE_NUMBER)
      { if (!mulib_mune_rt_need_int_ (s, format, &amount, 1) || amount < 0) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid read count"); what = "bytes"; }
    else if (format.type != MULIB_MUNE_NIL)
      { what = mulib_mune_rt_cstring_ (s, format, 1); if (!what) return 0; if (*what == '*') ++what; }
    if (strcmp (what, "l") && strcmp (what, "L") && strcmp (what, "a") && strcmp (what, "n") && strcmp (what, "bytes"))
      return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid read format");
    if (!strcmp (what, "n")) return mulib_mune_rt_read_number_ (s, file, out);
    if (!amount)
      {
        c = fgetc (file); eof = c == EOF; if (!eof) (void)ungetc (c, file);
      }
    else
      {
        while ((uint64_t)amount > bytes.length && (c = fgetc (file)) != EOF)
          {
            char byte = (char)c;
            if (c == '\n' && !strcmp (what, "l")) break;
            if (!mulib_mune_rt_bytes_ (s, &bytes, &byte, 1)) break;
            if (c == '\n' && !strcmp (what, "L")) break;
          }
        eof = c == EOF && !bytes.length && strcmp (what, "a");
      }
    if (!s->status)
      {
        mulib_mune_value_t value;
        value = eof ? mulib_mune_rt_nil_ () : mulib_mune_rt_string_ (s, bytes.data, bytes.length);
        (void)mulib_mune_result_push (out, value);
      }
    (void)mulib_mune_rt_memory_ (s, bytes.data, bytes.capacity, 0); return !s->status;
  }
  static int
  mulib_mune_rt_io_ (mulib_mune_state_t *s, const char *operation, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    int method = !strncmp (operation, "file.", 5);
    const char *op = operation + (method ? 5 : 3);
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0), file;
    FILE *stream;
    size_t start = method ? 1 : 0, k;
    if (!strcmp (op, "type"))
      return mulib_mune_result_push (out, a.type != MULIB_MUNE_USERDATA || a.as.object->userdata_tag != 1 ? mulib_mune_rt_nil_ () : mulib_mune_rt_string_ (s, a.as.object->as.file.closed ? "closed file" : "file", a.as.object->as.file.closed ? 11 : 4));
    if (!strcmp (op, "popen"))
      {
        const char *command = mulib_mune_rt_cstring_ (s, a, 1), *mode = n > 1 && args[1].type != MULIB_MUNE_NIL ? mulib_mune_rt_cstring_ (s, args[1], 2) : "r";
        if (!command || !mode) return 0;
        if (strcmp (mode, "r") && strcmp (mode, "w")) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid pipe mode");
#if defined(__unix__) || defined(__APPLE__)
        stream = popen (command, mode);
#elif defined(_WIN32)
        stream = _popen (command, mode);
#else
        return mulib_mune_rt_error_ (s, MULIB_MUNE_IO, "pipes unsupported on this platform");
#endif
        if (!stream) return mulib_mune_rt_file_result_ (s, out, 0, errno);
        file = mulib_mune_rt_file_ (s, stream, 2); return !s->status && mulib_mune_result_push (out, file);
      }
    if (!strcmp (op, "open") || !strcmp (op, "tmpfile"))
      {
        if (!strcmp (op, "tmpfile")) stream = tmpfile ();
        else
          {
            const char *path = mulib_mune_rt_cstring_ (s, a, 1), *mode = n > 1 && args[1].type != MULIB_MUNE_NIL ? mulib_mune_rt_cstring_ (s, args[1], 2) : "r";
            if (!path || !mode) return 0;
            if (!*mode || !strchr ("rwa", mode[0]) || strspn (mode + 1, "b+") != strlen (mode + 1) || strlen (mode) > 3
                || (mode[1] && mode[2] && mode[1] == mode[2]))
              return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid file mode");
            stream = fopen (path, mode);
          }
        if (!stream) return mulib_mune_rt_file_result_ (s, out, 0, errno);
        file = mulib_mune_rt_file_ (s, stream, 1); return !s->status && mulib_mune_result_push (out, file);
      }
    if (!strcmp (op, "input") || !strcmp (op, "output"))
      {
        int input = !strcmp (op, "input");
        if (a.type == MULIB_MUNE_NIL) file = input ? s->input : s->output;
        else if (a.type == MULIB_MUNE_STRING || a.type == MULIB_MUNE_INTEGER || a.type == MULIB_MUNE_NUMBER)
          {
            const char *path = mulib_mune_rt_cstring_ (s, a, 1); if (!path) return 0;
            stream = fopen (path, input ? "r" : "w"); if (!stream) return mulib_mune_rt_error_ (s, MULIB_MUNE_IO, "%s", strerror (errno));
            file = mulib_mune_rt_file_ (s, stream, 1);
          }
        else file = a;
        if (s->status || !mulib_mune_rt_need_ (s, file, MULIB_MUNE_USERDATA, 1)) return 0;
        if (file.as.object->userdata_tag != 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "file expected");
        if (file.as.object->as.file.closed) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "closed file");
        if (input) s->input = file; else s->output = file;
        return mulib_mune_result_push (out, file);
      }
    if (!strcmp (op, "lines"))
      {
        mulib_mune_object_t *closure, *formats;
        int owned = 0;
        if (method) file = a;
        else if (a.type != MULIB_MUNE_NIL)
          { const char *path = mulib_mune_rt_cstring_ (s, a, 1); if (!path) return 0; stream = fopen (path, "r"); if (!stream) return mulib_mune_rt_error_ (s, MULIB_MUNE_IO, "%s", strerror (errno)); file = mulib_mune_rt_file_ (s, stream, 1); owned = 1; }
        else file = s->input;
        if (!mulib_mune_rt_need_ (s, file, MULIB_MUNE_USERDATA, 1)) return 0;
        if (file.as.object->userdata_tag != 1 || !file.as.object->as.file.stream) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "open file expected");
        closure = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0); formats = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0);
        if (!closure || !formats) return 0;
        if (n <= 1 && !mulib_mune_rt_rawset_ (s, formats, mulib_mune_rt_integer_ (1), mulib_mune_rt_string_ (s, "l", 1))) return 0;
        for (k = 1; k < n; ++k) if (!mulib_mune_rt_rawset_ (s, formats, mulib_mune_rt_integer_ ((int64_t)k), args[k])) return 0;
        closure->as.function.native = mulib_mune_rt_builtin_; closure->as.function.user = (void *)"io.iterator";
        closure->as.function.context[0] = file; closure->as.function.context[1] = mulib_mune_rt_object_ (formats); closure->as.function.context[2] = mulib_mune_rt_boolean_ (owned);
        closure->as.function.cursor = n > 1 ? n - 1 : 1;
        if (!mulib_mune_result_push (out, mulib_mune_rt_object_ (closure))) return 0;
        return !owned || (mulib_mune_result_push (out, mulib_mune_rt_nil_ ()) && mulib_mune_result_push (out, mulib_mune_rt_nil_ ()) && mulib_mune_result_push (out, file));
      }
    if (!strcmp (op, "iterator")) file = s->frame->function.as.object->as.function.context[0];
    else file = method || (!strcmp (op, "close") && a.type != MULIB_MUNE_NIL) ? a
                : !strcmp (op, "read") ? s->input : s->output;
    if (!mulib_mune_rt_need_ (s, file, MULIB_MUNE_USERDATA, 1)) return 0;
    if (file.as.object->userdata_tag != 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "file expected");
    stream = file.as.object->as.file.stream;
    if (!strcmp (op, "finalize"))
      { if (stream && file.as.object->as.file.owned) (void)mulib_mune_rt_file_close_ (file.as.object); return 1; }
    if (!strcmp (op, "tostring"))
      {
        char text[80]; int length;
        if (!stream) return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "file (closed)", 13));
        length = snprintf (text, sizeof (text), "file (%p)", (void *)stream);
        return length >= 0 && (size_t)length < sizeof (text) && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, text, (size_t)length));
      }
    if (!stream || file.as.object->as.file.closed)
      {
        return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "attempt to use a closed file");
      }
    if (!strcmp (op, "close"))
      {
        int result, owned = file.as.object->as.file.owned;
        if (!owned) return mulib_mune_result_push (out, mulib_mune_rt_nil_ ()) && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "cannot close standard file", 26));
        result = mulib_mune_rt_file_close_ (file.as.object);
        if (owned != 2 || result == -1) return mulib_mune_rt_file_result_ (s, out, result == 0, errno);
#if defined(__unix__) || defined(__APPLE__)
        if (WIFSIGNALED (result)) return mulib_mune_result_push (out, mulib_mune_rt_nil_ ())
          && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "signal", 6)) && mulib_mune_result_push (out, mulib_mune_rt_integer_ (WTERMSIG (result)));
        result = WEXITSTATUS (result);
#endif
        return mulib_mune_result_push (out, result ? mulib_mune_rt_nil_ () : mulib_mune_rt_boolean_ (1))
            && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "exit", 4)) && mulib_mune_result_push (out, mulib_mune_rt_integer_ (result));
      }
    if (!strcmp (op, "flush")) { int result = fflush (stream); return mulib_mune_rt_file_result_ (s, out, result == 0, errno); }
    if (!strcmp (op, "seek"))
      {
        const char *whence = n > 1 && args[1].type != MULIB_MUNE_NIL ? mulib_mune_rt_cstring_ (s, args[1], 2) : "cur";
        int64_t offset = 0; int origin; long position;
        if (!whence || (n > 2 && args[2].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[2], &offset, 3))) return 0;
        origin = !strcmp (whence, "set") ? SEEK_SET : !strcmp (whence, "cur") ? SEEK_CUR : !strcmp (whence, "end") ? SEEK_END : -1;
        if (origin == -1 || offset < LONG_MIN || offset > LONG_MAX) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid seek arguments");
        if (fseek (stream, (long)offset, origin) || (position = ftell (stream)) < 0) return mulib_mune_rt_file_result_ (s, out, 0, errno);
        return mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)position));
      }
    if (!strcmp (op, "setvbuf"))
      {
        const char *mode = mulib_mune_rt_cstring_ (s, mulib_mune_rt_arg_ (args, n, 1), 2); int how; int64_t size = BUFSIZ;
        if (!mode || (n > 2 && args[2].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[2], &size, 3))) return 0;
        how = !strcmp (mode, "no") ? _IONBF : !strcmp (mode, "full") ? _IOFBF : !strcmp (mode, "line") ? _IOLBF : -1;
        if (how == -1 || size < 0 || (uint64_t)size > s->limits.memory_bytes) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid buffering arguments");
        return mulib_mune_rt_file_result_ (s, out, setvbuf (stream, NULL, how, (size_t)size) == 0, errno);
      }
    if (!strcmp (op, "write"))
      {
        for (k = start; k < n; ++k)
          {
            mulib_mune_value_t v;
            if (!mulib_mune_rt_text_ (s, args[k], 0, &v)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "string or number expected");
            if (fwrite (v.as.object + 1, 1, v.as.object->as.length, stream) != v.as.object->as.length) return mulib_mune_rt_file_result_ (s, out, 0, errno);
          }
        return mulib_mune_result_push (out, file);
      }
    if (!strcmp (op, "read") || !strcmp (op, "iterator"))
      {
        mulib_mune_object_t *closure = s->frame->function.as.object;
        size_t formats = !strcmp (op, "iterator") ? closure->as.function.cursor : n > start ? n - start : 1;
        for (k = 0; k < formats; ++k)
          {
            mulib_mune_value_t format = !strcmp (op, "iterator") ? mulib_mune_rt_rawget_ (closure->as.function.context[1].as.object, mulib_mune_rt_integer_ ((int64_t)k + 1)) : mulib_mune_rt_arg_ (args, n, start + k);
            if (!strcmp (op, "read") && n <= start) format = mulib_mune_rt_string_ (s, "l", 1);
            if (format.type == MULIB_MUNE_NIL) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "read format expected");
            if (!mulib_mune_rt_read_ (s, stream, format, out)) return 0;
            if (mulib_mune_result_get (out, out->count - 1).type == MULIB_MUNE_NIL) break;
          }
        if (ferror (stream)) { out->count = 0; return mulib_mune_rt_file_result_ (s, out, 0, errno); }
        if (!strcmp (op, "iterator") && mulib_mune_result_get (out, 0).type == MULIB_MUNE_NIL && mulib_mune_rt_truth_ (closure->as.function.context[2]))
          { (void)fclose (stream); file.as.object->as.file.stream = NULL; file.as.object->as.file.closed = 1; }
        return 1;
      }
    return mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "unknown I/O operation");
  }
  static void
  mulib_mune_rt_thread_error_ (mulib_mune_state_t *s)
  {
#ifdef MULIB_MUNE_THREADS_
    mulib_mune_coroutine_ *co = s->current_thread ? s->current_thread->as.thread : NULL;
    if (!co || co->main || co->close_requested || s->protected_call || !s->status) return;
    co->status = s->status; co->error = s->error_value;
    if (!s->has_error)
      { s->status = MULIB_MUNE_OK; co->error = mulib_mune_rt_string_ (s, s->message, strlen (s->message)); }
    s->status = co->status; s->has_error = 1; s->error_value = co->error;
    (void)mtx_lock (&co->mutex);
    co->frame = s->frame; co->depth = s->depth; co->state = MULIB_MUNE_CO_FAILED_; co->response = 1;
    (void)cnd_signal (&co->condition);
    while (!co->request) (void)cnd_wait (&co->condition, &co->mutex);
    co->request = 0; co->frame = NULL;
    (void)mtx_unlock (&co->mutex);
    s->status = co->status; s->error_value = co->error; s->has_error = 1;
#else
    (void)s;
#endif
  }
#ifdef MULIB_MUNE_THREADS_
  static int
  mulib_mune_rt_thread_entry_ (void *user)
  {
    mulib_mune_coroutine_ *co = (mulib_mune_coroutine_ *)user;
    mulib_mune_state_t *s = co->object->owner;
    mulib_mune_result_t *args, *result = NULL;
    (void)mtx_lock (&co->mutex);
    while (!co->request) (void)cnd_wait (&co->condition, &co->mutex);
    co->request = 0; args = co->incoming; co->incoming = NULL;
    (void)mtx_unlock (&co->mutex);
    if (!co->close_requested) result = mulib_mune_rt_call_ (s, co->function, args);
    mulib_mune_result_destroy (args);
    co->status = s->status;
    if (co->close_requested && s->status == MULIB_MUNE_RUNTIME && !strcmp (s->message, "coroutine closed")) co->status = MULIB_MUNE_OK;
    if (co->status)
      {
        co->error = s->error_value;
        s->status = MULIB_MUNE_OK;
        if (!s->has_error) co->error = mulib_mune_rt_string_ (s, s->message, strlen (s->message));
      }
    co->frame = NULL; co->depth = 0; co->outgoing = result; co->function = mulib_mune_rt_nil_ ();
    (void)mtx_lock (&co->mutex); co->state = MULIB_MUNE_CO_DEAD_; co->response = 1;
    (void)cnd_signal (&co->condition); (void)mtx_unlock (&co->mutex); return 0;
  }
  static int
  mulib_mune_rt_switch_ (mulib_mune_state_t *s, mulib_mune_coroutine_ *co)
  {
    mulib_mune_frame_ *frame = s->frame;
    mulib_mune_protected_ *protect = s->protected_call;
    mulib_mune_object_t *current = s->current_thread;
    mulib_mune_value_t error = s->error_value;
    mulib_mune_status_t status = s->status;
    int has_error = s->has_error;
    size_t depth = s->depth, location = s->location;
    char message[sizeof (s->message)];
    memcpy (message, s->message, sizeof (message));
    if (!co->started)
      {
        if (thrd_create (&co->worker, mulib_mune_rt_thread_entry_, co) != thrd_success)
          return mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "coroutine thread creation failed");
        co->started = 1;
      }
    (void)mtx_lock (&co->mutex);
    if (current && current->as.thread) { current->as.thread->frame = frame; current->as.thread->depth = depth; current->as.thread->protected_call = protect; }
    co->parent = current; s->current_thread = co->object; s->frame = co->frame; s->depth = co->depth;
    s->protected_call = co->protected_call;
    s->status = MULIB_MUNE_OK; s->has_error = 0; s->error_value = mulib_mune_rt_nil_ ();
    co->state = MULIB_MUNE_CO_RUNNING_; co->request = 1; co->response = 0;
    (void)cnd_signal (&co->condition);
    while (!co->response) (void)cnd_wait (&co->condition, &co->mutex);
    co->parent = NULL; s->current_thread = current; s->frame = frame; s->depth = depth;
    co->protected_call = s->protected_call; s->protected_call = protect;
    s->status = status; s->has_error = has_error; s->error_value = error; s->location = location; memcpy (s->message, message, sizeof (message));
    if (current && current->as.thread) { current->as.thread->frame = NULL; current->as.thread->protected_call = NULL; }
    (void)mtx_unlock (&co->mutex);
    if (co->state == MULIB_MUNE_CO_DEAD_) { int result; (void)thrd_join (co->worker, &result); co->started = 0; }
    return 1;
  }
#endif
  static mulib_mune_object_t *
  mulib_mune_rt_thread_create_ (mulib_mune_state_t *s, mulib_mune_value_t function, int main)
  {
    mulib_mune_object_t *o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_THREAD, sizeof (mulib_mune_coroutine_));
    mulib_mune_coroutine_ *co;
    if (!o) return NULL;
    co = (mulib_mune_coroutine_ *)(o + 1); memset (co, 0, sizeof (*co));
    co->object = o; co->function = function; co->main = main; co->state = main ? MULIB_MUNE_CO_RUNNING_ : MULIB_MUNE_CO_NEW_;
    o->as.thread = co;
#ifdef MULIB_MUNE_THREADS_
    if (!main)
      {
        if (mtx_init (&co->mutex, mtx_plain) != thrd_success) { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "coroutine mutex creation failed"); return NULL; }
        if (cnd_init (&co->condition) != thrd_success) { mtx_destroy (&co->mutex); mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "coroutine condition creation failed"); return NULL; }
        co->initialized = 1;
      }
#else
    if (!main) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "coroutines require C11 threads"); return NULL; }
#endif
    return o;
  }
  static void
  mulib_mune_rt_thread_free_ (mulib_mune_state_t *s, mulib_mune_object_t *o)
  {
    mulib_mune_coroutine_ *co = o->as.thread;
    if (!co) return;
#ifdef MULIB_MUNE_THREADS_
    if (co->started && (co->state == MULIB_MUNE_CO_SUSPENDED_ || co->state == MULIB_MUNE_CO_FAILED_))
      { co->discard = 1; co->close_requested = 1; (void)mulib_mune_rt_switch_ (s, co); }
    if (co->initialized) { cnd_destroy (&co->condition); mtx_destroy (&co->mutex); }
#else
    (void)s;
#endif
    mulib_mune_result_destroy (co->incoming); mulib_mune_result_destroy (co->outgoing);
    o->as.thread = NULL;
  }
  static int
  mulib_mune_rt_coroutine_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0);
    mulib_mune_coroutine_ *co;
    if (!strcmp (op, "create") || !strcmp (op, "wrap"))
      {
        mulib_mune_object_t *o, *wrapper;
        if (!mulib_mune_rt_need_ (s, a, MULIB_MUNE_FUNCTION, 1)) return 0;
        o = mulib_mune_rt_thread_create_ (s, a, 0); if (!o) return 0;
        if (!strcmp (op, "create")) return mulib_mune_result_push (out, mulib_mune_rt_object_ (o));
        wrapper = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0); if (!wrapper) return 0;
        wrapper->as.function.native = mulib_mune_rt_builtin_; wrapper->as.function.user = (void *)"coroutine.wrapped";
        wrapper->as.function.context[0] = mulib_mune_rt_object_ (o); return mulib_mune_result_push (out, mulib_mune_rt_object_ (wrapper));
      }
    if (!strcmp (op, "running"))
      return mulib_mune_result_push (out, mulib_mune_rt_object_ (s->current_thread)) && mulib_mune_result_push (out, mulib_mune_rt_boolean_ (s->current_thread == s->main_thread));
    if (!strcmp (op, "isyieldable"))
      {
        if (a.type == MULIB_MUNE_NIL) a = mulib_mune_rt_object_ (s->current_thread);
        return mulib_mune_rt_need_ (s, a, MULIB_MUNE_THREAD, 1) && mulib_mune_result_push (out, mulib_mune_rt_boolean_ (a.as.object != s->main_thread && !s->collecting));
      }
    if (!strcmp (op, "yield"))
      {
#ifdef MULIB_MUNE_THREADS_
        size_t k;
        co = s->current_thread->as.thread;
        if (co->main || s->collecting || co->close_requested || co->in_hook) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "cannot yield in this context");
        co->outgoing = mulib_mune_rt_results_ (s); if (!co->outgoing) return 0;
        for (k = 0; k < n; ++k) if (!mulib_mune_result_push (co->outgoing, args[k]))
          { mulib_mune_result_destroy (co->outgoing); co->outgoing = NULL; return 0; }
        (void)mtx_lock (&co->mutex);
        co->frame = s->frame; co->depth = s->depth; co->state = MULIB_MUNE_CO_SUSPENDED_; co->response = 1;
        (void)cnd_signal (&co->condition);
        while (!co->request) (void)cnd_wait (&co->condition, &co->mutex);
        co->request = 0; co->frame = NULL;
        (void)mtx_unlock (&co->mutex);
        if (co->close_requested) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "coroutine closed");
        if (co->incoming) { (void)mulib_mune_rt_append_ (out, co->incoming, 0); mulib_mune_result_destroy (co->incoming); co->incoming = NULL; }
        return !s->status;
#else
        return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "coroutines require C11 threads");
#endif
      }
    if (!strcmp (op, "wrapped")) a = s->frame->function.as.object->as.function.context[0];
    if (!mulib_mune_rt_need_ (s, a, MULIB_MUNE_THREAD, 1)) return 0;
    co = a.as.object->as.thread;
    if (!strcmp (op, "status"))
      {
        const char *status = co->state == MULIB_MUNE_CO_DEAD_ || co->state == MULIB_MUNE_CO_FAILED_ ? "dead" : co->state == MULIB_MUNE_CO_RUNNING_ ? a.as.object == s->current_thread ? "running" : "normal" : "suspended";
        return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, status, strlen (status)));
      }
#ifdef MULIB_MUNE_THREADS_
    if (!strcmp (op, "close"))
      {
        if (co->state == MULIB_MUNE_CO_RUNNING_) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "cannot close a running coroutine");
        co->close_requested = 1;
        if ((co->state == MULIB_MUNE_CO_SUSPENDED_ || co->state == MULIB_MUNE_CO_FAILED_) && !mulib_mune_rt_switch_ (s, co)) return 0;
        co->state = MULIB_MUNE_CO_DEAD_;
        if (!mulib_mune_result_push (out, mulib_mune_rt_boolean_ (!co->status))) return 0;
        if (co->status && !mulib_mune_result_push (out, co->error)) return 0;
        co->status = MULIB_MUNE_OK; co->error = mulib_mune_rt_nil_ (); return 1;
      }
    if (!strcmp (op, "resume") || !strcmp (op, "wrapped"))
      {
        size_t k = !strcmp (op, "wrapped") ? 0 : 1;
        int wrapped = k == 0;
        if (co->state == MULIB_MUNE_CO_RUNNING_ || co->state == MULIB_MUNE_CO_DEAD_ || co->state == MULIB_MUNE_CO_FAILED_)
          {
            mulib_mune_value_t error = mulib_mune_rt_string_ (s, "cannot resume non-suspended coroutine", 37);
            if (wrapped) return mulib_mune_rt_raise_ (s, error);
            return mulib_mune_result_push (out, mulib_mune_rt_boolean_ (0)) && mulib_mune_result_push (out, error);
          }
        co->incoming = mulib_mune_rt_results_ (s); if (!co->incoming) return 0;
        co->incoming->thread = co->object;
        for (; k < n; ++k) if (!mulib_mune_result_push (co->incoming, args[k])) break;
        if (s->status || !mulib_mune_rt_switch_ (s, co))
          { mulib_mune_result_destroy (co->incoming); co->incoming = NULL; return 0; }
        if (co->status)
          {
            if (wrapped)
              {
                co->close_requested = 1;
                if (co->state == MULIB_MUNE_CO_FAILED_ && !mulib_mune_rt_switch_ (s, co)) return 0;
                return mulib_mune_rt_raise_ (s, co->error);
              }
            return mulib_mune_result_push (out, mulib_mune_rt_boolean_ (0)) && mulib_mune_result_push (out, co->error);
          }
        if (!wrapped && !mulib_mune_result_push (out, mulib_mune_rt_boolean_ (1))) return 0;
        if (co->outgoing) { (void)mulib_mune_rt_append_ (out, co->outgoing, 0); mulib_mune_result_destroy (co->outgoing); co->outgoing = NULL; }
        return !s->status;
      }
#endif
    return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "unsupported coroutine operation");
  }
  typedef struct mulib_mune_packfmt_
  {
    const char *text;
    size_t at, length, alignment;
    int little;
  } mulib_mune_packfmt_;
  static size_t
  mulib_mune_rt_pack_count_ (mulib_mune_state_t *s, mulib_mune_packfmt_ *f, size_t fallback)
  {
    size_t result = 0, start = f->at;
    while (f->at < f->length && mulib_mune_digit_ ((unsigned char)f->text[f->at]))
      {
        unsigned digit = (unsigned)(f->text[f->at++] - '0');
        if (result > (SIZE_MAX - digit) / 10) { mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "packing size overflow"); return 0; }
        result = result * 10 + digit;
      }
    return f->at == start ? fallback : result;
  }
  static int
  mulib_mune_rt_pack_spec_ (mulib_mune_state_t *s, mulib_mune_packfmt_ *f, char *kind, size_t *size, size_t *align)
  {
    int skip = 0;
    uint16_t endian = 1;
    for (;;)
      {
        char c;
        if (f->at == f->length) { if (skip) mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "missing alignment option"); return 0; }
        c = f->text[f->at++]; *align = 1;
        if (skip && (c == ' ' || c == '<' || c == '>' || c == '=' || c == '!')) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid alignment option");
        if (c == ' ') continue;
        if (c == '<' || c == '>' || c == '=') { f->little = c == '<' || (c == '=' && *(unsigned char *)&endian); continue; }
        if (c == '!')
          {
            f->alignment = mulib_mune_rt_pack_count_ (s, f, 8);
            if (!f->alignment || f->alignment > 16) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid maximum alignment");
            continue;
          }
        if (c == 'X') { if (skip) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid alignment option"); skip = 1; continue; }
        *kind = c;
        switch (c)
          {
          case 'b': case 'B': case 'x': *size = 1; break;
          case 'h': case 'H': *size = sizeof (short); break;
          case 'l': case 'L': *size = sizeof (long); break;
          case 'j': case 'J': *size = sizeof (int64_t); break;
          case 'T': *size = sizeof (size_t); break;
          case 'i': case 'I': *size = mulib_mune_rt_pack_count_ (s, f, sizeof (int)); break;
          case 'f': *size = sizeof (float); break;
          case 'd': case 'n': *size = sizeof (double); break;
          case 'c': *size = mulib_mune_rt_pack_count_ (s, f, SIZE_MAX); if (*size == SIZE_MAX) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "c needs a size"); break;
          case 's': *size = mulib_mune_rt_pack_count_ (s, f, sizeof (size_t)); break;
          case 'z': *size = 0; break;
          default: return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid packing option");
          }
        if (s->status) return 0;
        if (c != 'c' && c != 'z' && (!*size || *size > 16)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "integer packing size out of range");
        if (c != 'c' && c != 'z' && c != 'x') *align = *size < f->alignment ? *size : f->alignment;
        if (*align & (*align - 1)) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "alignment is not a power of two");
        if (skip)
          {
            if (c == 'c' || c == 'z') return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid alignment operand");
            *kind = 'X'; *size = 0;
          }
        return 1;
      }
  }
  static int
  mulib_mune_rt_pack_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_packfmt_ f;
    mulib_mune_bytes_ bytes = { NULL, 0, 0 };
    mulib_mune_value_t format = mulib_mune_rt_arg_ (args, n, 0), data = mulib_mune_rt_arg_ (args, n, 1);
    const unsigned char *input = NULL;
    size_t length = 0, at = 0, argument = 1, size = 0, align = 1;
    char kind = 0;
    uint16_t endian = 1;
    int unpack = !strcmp (op, "unpack"), sizeonly = !strcmp (op, "packsize"), native_little = *(unsigned char *)&endian;
    memset (&f, 0, sizeof (f)); f.text = mulib_mune_rt_cstring_ (s, format, 1); if (!f.text) return 0;
    f.length = strlen (f.text); f.alignment = 1; f.little = native_little;
    if (unpack)
      {
        int64_t position = 1;
        if (!mulib_mune_rt_need_ (s, data, MULIB_MUNE_STRING, 2) || (n > 2 && args[2].type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, args[2], &position, 3))) return 0;
        input = (const unsigned char *)mulib_mune_value_string (data, &length); position = mulib_mune_rt_relative_ (position, length);
        if (position < 1 || (uint64_t)position > length + 1) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "unpack position out of range");
        at = (size_t)position - 1;
      }
    while (mulib_mune_rt_pack_spec_ (s, &f, &kind, &size, &align))
      {
        size_t padding = (align - at % align) % align, k;
        unsigned char raw[16] = {0};
        mulib_mune_value_t v = mulib_mune_rt_nil_ ();
        int signed_value = kind == 'b' || kind == 'h' || kind == 'l' || kind == 'j' || kind == 'i';
        int numeric = kind && strchr ("bBhHlLjJTiIs", kind) != NULL;
        if (padding > SIZE_MAX - at || size > SIZE_MAX - at - padding) { mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "packed size overflow"); break; }
        if (sizeonly)
          {
            if (kind == 's' || kind == 'z') { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "variable-length format has no fixed size"); break; }
            at += padding + size; continue;
          }
        if (unpack)
          {
            if (padding > length - at || size > length - at - padding) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "data string too short"); break; }
            at += padding;
            if (numeric)
              {
                uint64_t integer = 0;
                int negative = signed_value && (input[at + (f.little ? size - 1 : 0)] & 128);
                for (k = 0; k < size; ++k)
                  {
                    unsigned byte = input[at + (f.little ? k : size - k - 1)];
                    if (k < 8) integer |= (uint64_t)byte << (k * 8);
                    else if (byte != (negative ? 255u : 0u)) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "unpacked integer does not fit"); break; }
                  }
                if (s->status) break;
                if (signed_value && size > 8 && negative != (int)(integer >> 63))
                  { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "unpacked integer does not fit"); break; }
                if (negative && size < 8) integer |= UINT64_MAX << (size * 8);
                v = mulib_mune_rt_integer_ (mulib_mune_rt_signed_ (integer)); at += size;
                if (kind == 's')
                  {
                    if (integer > length - at) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "length-prefixed string too short"); break; }
                    v = mulib_mune_rt_string_ (s, (const char *)input + at, (size_t)integer); at += (size_t)integer;
                  }
              }
            else if (kind == 'f' || kind == 'd' || kind == 'n')
              {
                for (k = 0; k < size; ++k) raw[k] = input[at + (f.little == native_little ? k : size - k - 1)];
                if (kind == 'f') { float value; memcpy (&value, raw, sizeof (value)); v = mulib_mune_rt_number_ (value); }
                else { double value; memcpy (&value, raw, sizeof (value)); v = mulib_mune_rt_number_ (value); }
                at += size;
              }
            else if (kind == 'c') { v = mulib_mune_rt_string_ (s, (const char *)input + at, size); at += size; }
            else if (kind == 'z')
              {
                const unsigned char *end = (const unsigned char *)memchr (input + at, 0, length - at);
                if (!end) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "unfinished zero-terminated string"); break; }
                size = (size_t)(end - (input + at)); v = mulib_mune_rt_string_ (s, (const char *)input + at, size); at += size + 1;
              }
            else at += size;
            if (kind != 'x' && kind != 'X' && !mulib_mune_result_push (out, v)) break;
          }
        else
          {
            const char *text = NULL; size_t text_length = 0;
            if (!mulib_mune_rt_bytes_ (s, &bytes, (const char *)raw, padding)) break;
            at += padding;
            if (kind != 'x' && kind != 'X')
              { if (argument == n) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "missing packing argument"); break; } v = args[argument++]; }
            if (kind == 's' || kind == 'z' || kind == 'c')
              { if (!mulib_mune_rt_text_ (s, v, 0, &v)) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "string expected for packing"); break; } text = mulib_mune_value_string (v, &text_length); }
            if (numeric)
              {
                uint64_t integer; int64_t value = 0;
                if (kind == 's') integer = text_length;
                else { if (!mulib_mune_rt_need_int_ (s, v, &value, argument)) break; integer = (uint64_t)value; }
                if (size < 8)
                  {
                    unsigned bits = (unsigned)size * 8;
                    if ((signed_value && (value < -(INT64_C (1) << (bits - 1)) || value >= (INT64_C (1) << (bits - 1))))
                        || (!signed_value && integer >= (UINT64_C (1) << bits))) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "integer does not fit packing size"); break; }
                  }
                for (k = 0; k < size; ++k) raw[f.little ? k : size - k - 1] = k < 8 ? (unsigned char)(integer >> (k * 8)) : (unsigned char)(signed_value && value < 0 ? 255 : 0);
                if (!mulib_mune_rt_bytes_ (s, &bytes, (const char *)raw, size)) break;
                if (kind == 's' && !mulib_mune_rt_bytes_ (s, &bytes, text, text_length)) break;
              }
            else if (kind == 'f' || kind == 'd' || kind == 'n')
              {
                unsigned char native[16];
                if (!mulib_mune_rt_numeric_ (v, &v)) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "number expected for packing"); break; }
                if (kind == 'f') { float value = (float)mulib_mune_rt_double_ (v); memcpy (native, &value, sizeof (value)); }
                else { double value = mulib_mune_rt_double_ (v); memcpy (native, &value, sizeof (value)); }
                for (k = 0; k < size; ++k) raw[k] = native[f.little == native_little ? k : size - k - 1];
                if (!mulib_mune_rt_bytes_ (s, &bytes, (const char *)raw, size)) break;
              }
            else if (kind == 'z')
              {
                if (memchr (text, 0, text_length)) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "zero in zero-terminated string"); break; }
                if (!mulib_mune_rt_bytes_ (s, &bytes, text, text_length + 1)) break;
              }
            else if (kind == 'c')
              {
                if (text_length > size) { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "fixed string too long"); break; }
                if (!mulib_mune_rt_bytes_ (s, &bytes, text, text_length)) break;
                for (k = text_length; k < size && !s->status; ++k) (void)mulib_mune_rt_bytes_ (s, &bytes, "", 1);
              }
            else if (!mulib_mune_rt_bytes_ (s, &bytes, (const char *)raw, size)) break;
            at = bytes.length;
          }
        if (s->status) break;
      }
    if (!s->status)
      {
        if (sizeonly || unpack)
          { if (at > (size_t)INT64_MAX - (unpack ? 1u : 0u)) mulib_mune_rt_error_ (s, MULIB_MUNE_LIMIT, "packed length overflow"); else (void)mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)at + (unpack ? 1 : 0))); }
        else (void)mulib_mune_result_push (out, mulib_mune_rt_string_ (s, bytes.data, bytes.length));
      }
    (void)mulib_mune_rt_memory_ (s, bytes.data, bytes.capacity, 0); return !s->status;
  }
  typedef struct mulib_mune_write_context_ { mulib_mune_state_t *state; mulib_mune_bytes_ *bytes; } mulib_mune_write_context_;
  static int
  mulib_mune_rt_buffer_write_ (void *user, const char *text, size_t length)
  {
    mulib_mune_write_context_ *c = (mulib_mune_write_context_ *)user;
    return mulib_mune_rt_bytes_ (c->state, c->bytes, text, length);
  }
  static int
  mulib_mune_rt_loadlib_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_bytes_ bytes = { NULL, 0, 0 };
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0), fn = mulib_mune_rt_nil_ ();
    mulib_mune_chunk_t *chunk = NULL;
    mulib_mune_error_t error = { MULIB_MUNE_OK, 0, "ok" };
    const char *source = NULL, *mode = "bt", *name = NULL;
    size_t length = 0;
    uint64_t offset = UINT64_MAX;
    unsigned method = 0;
    int file = strcmp (op, "load") != 0, dump = !strcmp (op, "dump"), execute = !strcmp (op, "dofile");
    mulib_mune_value_t env = n > (file ? 2u : 3u) ? args[file ? 2 : 3] : mulib_mune_rt_object_ (s->globals);
    if (dump)
      {
        mulib_mune_write_context_ context = { s, &bytes };
        const mulib_ast_t *body, *root;
        if (!mulib_mune_rt_need_ (s, a, MULIB_MUNE_FUNCTION, 1)) return 0;
        if (a.as.object->as.function.native) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "cannot dump a native function");
        body = a.as.object->as.function.body; root = a.as.object->as.function.chunk->walker->root;
        method = (unsigned)a.as.object->as.function.method;
        if (mulib_mune_rt_bytes_ (s, &bytes, "\033MUNE\0\1\0", 8)
            && mulib_mune_bundle_uint_ (mulib_mune_rt_buffer_write_, &context, body == root ? UINT64_MAX : body->offset, 8)
            && mulib_mune_bundle_uint_ (mulib_mune_rt_buffer_write_, &context, root->length, 8)
            && mulib_mune_bundle_uint_ (mulib_mune_rt_buffer_write_, &context, method, 1)
            && mulib_mune_rt_bytes_ (s, &bytes, root->text, root->length))
          (void)mulib_mune_result_push (out, mulib_mune_rt_string_ (s, bytes.data, bytes.length));
        goto done;
      }
    if (!execute && n > (file ? 1u : 2u) && args[file ? 1 : 2].type != MULIB_MUNE_NIL)
      { mode = mulib_mune_rt_cstring_ (s, args[file ? 1 : 2], file ? 2 : 3); if (!mode) goto done; }
    if (!file && n > 1 && args[1].type != MULIB_MUNE_NIL)
      { name = mulib_mune_rt_cstring_ (s, args[1], 2); if (!name) goto done; }
    if (file)
      {
        FILE *stream = stdin; int c;
        if (a.type != MULIB_MUNE_NIL)
          {
            name = mulib_mune_rt_cstring_ (s, a, 1); if (!name) goto done;
            stream = fopen (name, "rb");
            if (!stream) { error.code = MULIB_MUNE_IO; error.message = "could not open Lua file"; goto failed; }
          }
        while ((c = fgetc (stream)) != EOF)
          { char byte = (char)c; if (!mulib_mune_rt_bytes_ (s, &bytes, &byte, 1)) break; }
        if (ferror (stream)) { error.code = MULIB_MUNE_IO; error.message = "could not read Lua file"; }
        if (stream != stdin && fclose (stream)) { error.code = MULIB_MUNE_IO; error.message = "could not close Lua file"; }
        if (s->status) goto done;
        if (error.code) goto failed;
        if (bytes.length >= 3 && !memcmp (bytes.data, "\xef\xbb\xbf", 3)) memset (bytes.data, ' ', 3);
        { size_t at = 0; while (at < bytes.length && bytes.data[at] == ' ') ++at;
          if (at < bytes.length && bytes.data[at] == '#') while (at < bytes.length && bytes.data[at] != '\n') bytes.data[at++] = ' '; }
        source = bytes.data; length = bytes.length;
      }
    else if (a.type == MULIB_MUNE_FUNCTION)
      {
        for (;;)
          {
            mulib_mune_result_t *empty = mulib_mune_rt_results_ (s), *r;
            mulib_mune_protected_ protect;
            mulib_mune_value_t part;
            protect.previous = s->protected_call; protect.handler = mulib_mune_rt_nil_ (); protect.handled = 0; s->protected_call = &protect;
            r = empty ? mulib_mune_rt_call_ (s, a, empty) : NULL;
            s->protected_call = protect.previous;
            mulib_mune_result_destroy (empty);
            if (!r)
              {
                if (s->status != MULIB_MUNE_MEMORY && s->status != MULIB_MUNE_LIMIT)
                  {
                    part = s->error_value; s->status = MULIB_MUNE_OK;
                    if (!s->has_error) part = mulib_mune_rt_string_ (s, s->message, strlen (s->message));
                    (void)mulib_mune_result_push (out, mulib_mune_rt_nil_ ()); (void)mulib_mune_result_push (out, part);
                    s->has_error = 0; s->error_value = mulib_mune_rt_nil_ ();
                  }
                goto done;
              }
            part = mulib_mune_result_get (r, 0);
            if (part.type == MULIB_MUNE_NIL || (part.type == MULIB_MUNE_STRING && !part.as.object->as.length)) { mulib_mune_result_destroy (r); break; }
            if (!mulib_mune_rt_text_ (s, part, 0, &part))
              { mulib_mune_result_destroy (r); error.code = MULIB_MUNE_RUNTIME; error.message = "reader must return a string"; goto failed; }
            (void)mulib_mune_rt_bytes_ (s, &bytes, (const char *)(part.as.object + 1), part.as.object->as.length);
            mulib_mune_result_destroy (r); if (s->status) goto done;
          }
        source = bytes.data; length = bytes.length;
      }
    else
      {
        if (!mulib_mune_rt_text_ (s, a, 0, &a))
          { mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "string or reader function expected"); goto done; }
        source = mulib_mune_value_string (a, &length);
      }
    if (length && (unsigned char)source[0] == 27)
      {
        if (!strchr (mode, 'b')) { error.code = MULIB_MUNE_SYNTAX; error.message = "binary chunk prohibited by mode"; goto failed; }
        if (length < 25 || memcmp (source, "\033MUNE\0\1\0", 8)
            || mulib_mune_bundle_readuint_ ((const unsigned char *)source + 16, 8) != length - 25 || (unsigned char)source[24] > 1)
          { error.code = MULIB_MUNE_SYNTAX; error.message = "invalid Mune function image"; goto failed; }
        offset = mulib_mune_bundle_readuint_ ((const unsigned char *)source + 8, 8); method = (unsigned char)source[24];
        source += 25; length -= 25;
      }
    else if (!strchr (mode, 't')) { error.code = MULIB_MUNE_SYNTAX; error.message = "text chunk prohibited by mode"; goto failed; }
    chunk = mulib_mune_parse (source, length, NULL, &error); if (!chunk) goto failed;
    if (name)
      {
        size_t size = strlen (name);
        chunk->source_name = (char *)malloc (size + 2);
        if (!chunk->source_name) { mulib_mune_rt_error_ (s, MULIB_MUNE_MEMORY, "source name allocation failed"); goto done; }
        if (file) { chunk->source_name[0] = '@'; memcpy (chunk->source_name + 1, name, size + 1); }
        else memcpy (chunk->source_name, name, size + 1);
      }
    if (offset == UINT64_MAX) { fn = mulib_mune_rt_loaded_ (s, chunk, env); chunk = NULL; }
    else
      {
        mulib_mune_prototype_ *p;
        mulib_mune_capture_name_ *c;
        mulib_mune_object_t *f, **tail;
        for (p = chunk->prototypes; p && p->body->offset != offset; p = p->next) {}
        if (!p) { error.code = MULIB_MUNE_SYNTAX; error.message = "invalid function offset"; goto failed; }
        f = mulib_mune_rt_allocate_ (s, MULIB_MUNE_FUNCTION, 0); if (!f) goto done;
        f->as.function.chunk = chunk; chunk = NULL; f->as.function.body = p->body; f->as.function.method = (int)method;
        tail = &f->as.function.environment;
        for (c = p->captures; c; c = c->next)
          {
            *tail = mulib_mune_rt_environment_ (s, NULL, c->name->text, c->name->length,
                                               c == p->captures ? env : mulib_mune_rt_nil_ (), c->name->offset, 0);
            if (!*tail) goto done;
            tail = &(*tail)->as.environment.parent;
          }
        fn = mulib_mune_rt_object_ (f);
      }
    if (s->status) goto done;
    if (execute)
      {
        mulib_mune_result_t *empty = mulib_mune_rt_results_ (s), *r = empty ? mulib_mune_rt_call_ (s, fn, empty) : NULL;
        if (r) { (void)mulib_mune_rt_append_ (out, r, 0); mulib_mune_result_destroy (r); }
        mulib_mune_result_destroy (empty);
      }
    else (void)mulib_mune_result_push (out, fn);
    goto done;
  failed:
    if (execute || error.code == MULIB_MUNE_MEMORY) mulib_mune_rt_error_ (s, error.code, "%s", error.message);
    else { (void)mulib_mune_result_push (out, mulib_mune_rt_nil_ ()); (void)mulib_mune_result_push (out, mulib_mune_rt_string_ (s, error.message, strlen (error.message))); }
  done:
    mulib_mune_chunk_destroy (chunk);
    (void)mulib_mune_rt_memory_ (s, bytes.data, bytes.capacity, 0); return !s->status;
  }
  mulib_mune_status_t
  mulib_mune_call (mulib_mune_state_t *s, mulib_mune_value_t fn, const mulib_mune_value_t *values,
                   size_t count, mulib_mune_result_t **out, mulib_mune_error_t *error)
  {
    mulib_mune_result_t *args, *r = NULL; size_t k; int running;
    if (!s || !out || *out || (!values && count) || !mulib_mune_rt_valid_ (s, fn)) return MULIB_MUNE_INVALID;
    for (k = 0; k < count; ++k) if (!mulib_mune_rt_valid_ (s, values[k])) return MULIB_MUNE_INVALID;
    running = s->running;
    if (!running) { s->status = MULIB_MUNE_OK; s->has_error = 0; s->steps = 0; s->error_value = mulib_mune_rt_nil_ (); }
    s->running = 1; args = mulib_mune_rt_results_ (s);
    if (args)
      { for (k = 0; k < count; ++k) if (!mulib_mune_result_push (args, values[k])) break; r = mulib_mune_rt_call_ (s, fn, args); }
    mulib_mune_result_destroy (args); s->running = running;
    if (r) { r->thread = NULL; *out = r; }
    if (error) { error->code = s->status; error->offset = s->location; error->message = s->status ? s->message : "ok"; }
    return s->status;
  }
  mulib_mune_status_t
  mulib_mune_make_table (mulib_mune_state_t *s, mulib_mune_value_t *out)
  {
    mulib_mune_object_t *o;
    if (!s || !out) return MULIB_MUNE_INVALID;
    if (!s->running) s->status = MULIB_MUNE_OK;
    o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0); if (!o) return s->status;
    *out = mulib_mune_rt_object_ (o); return MULIB_MUNE_OK;
  }
  mulib_mune_status_t
  mulib_mune_raw_get (mulib_mune_state_t *s, mulib_mune_value_t table, mulib_mune_value_t key, mulib_mune_value_t *out)
  {
    if (!s || !out || table.type != MULIB_MUNE_TABLE || !mulib_mune_rt_valid_ (s, table) || !mulib_mune_rt_valid_ (s, key)) return MULIB_MUNE_INVALID;
    *out = mulib_mune_rt_rawget_ (table.as.object, key); return MULIB_MUNE_OK;
  }
  mulib_mune_status_t
  mulib_mune_raw_set (mulib_mune_state_t *s, mulib_mune_value_t table, mulib_mune_value_t key, mulib_mune_value_t value)
  {
    if (!s || table.type != MULIB_MUNE_TABLE || !mulib_mune_rt_valid_ (s, table) || !mulib_mune_rt_valid_ (s, key) || !mulib_mune_rt_valid_ (s, value)) return MULIB_MUNE_INVALID;
    if (!s->running) s->status = MULIB_MUNE_OK;
    return mulib_mune_rt_rawset_ (s, table.as.object, key, value) ? MULIB_MUNE_OK : s->status;
  }
  mulib_mune_status_t
  mulib_mune_set_metatable (mulib_mune_state_t *s, mulib_mune_value_t value, mulib_mune_value_t meta)
  {
    mulib_mune_object_t *m;
    if (!s || !mulib_mune_rt_valid_ (s, value) || !mulib_mune_rt_valid_ (s, meta) || (meta.type != MULIB_MUNE_NIL && meta.type != MULIB_MUNE_TABLE)) return MULIB_MUNE_INVALID;
    m = meta.type == MULIB_MUNE_NIL ? NULL : meta.as.object;
    if (value.type == MULIB_MUNE_TABLE || (value.type == MULIB_MUNE_USERDATA && value.as.object->userdata_tag != 3))
      {
        value.as.object->metatable = m;
        mulib_mune_rt_finalize_register_ (s, value.as.object);
      }
    else s->type_metatables[value.type == MULIB_MUNE_INTEGER ? MULIB_MUNE_NUMBER : value.type] = m;
    return MULIB_MUNE_OK;
  }
  mulib_mune_status_t
  mulib_mune_make_userdata (mulib_mune_state_t *s, size_t bytes, mulib_mune_value_t *value, void **data)
  {
    mulib_mune_object_t *o;
    if (!s || !value || !data) return MULIB_MUNE_INVALID;
    if (!s->running) s->status = MULIB_MUNE_OK;
    o = mulib_mune_rt_allocate_ (s, MULIB_MUNE_USERDATA, bytes); if (!o) return s->status;
    o->userdata_tag = 2; o->as.userdata.length = bytes; memset (o + 1, 0, bytes);
    o->as.userdata.values = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0); if (!o->as.userdata.values) return s->status;
    *value = mulib_mune_rt_object_ (o); *data = o + 1; return MULIB_MUNE_OK;
  }
  void *
  mulib_mune_value_userdata (mulib_mune_value_t v, size_t *bytes)
  {
    if (bytes) *bytes = 0;
    if (v.type != MULIB_MUNE_USERDATA || !v.as.object || v.as.object->userdata_tag != 2) return NULL;
    if (bytes) *bytes = v.as.object->as.userdata.length;
    return v.as.object + 1;
  }
  static void
  mulib_mune_rt_handle_error_ (mulib_mune_state_t *s)
  {
    mulib_mune_protected_ *p = s->protected_call;
    mulib_mune_status_t status = s->status;
    mulib_mune_value_t error = s->error_value;
    mulib_mune_result_t *r;
    if (!status) return;
    if (s->current_thread && s->current_thread->as.thread->close_requested && !s->current_thread->as.thread->closing_callbacks) return;
    if (!p) { mulib_mune_rt_thread_error_ (s); return; }
    if (p->handled || p->handler.type == MULIB_MUNE_NIL) return;
    p->handled = 1; s->status = MULIB_MUNE_OK;
    if (!s->has_error) error = mulib_mune_rt_string_ (s, s->message, strlen (s->message));
    r = mulib_mune_rt_metacall_ (s, p->handler, error, error, error, 1);
    if (r) { error = mulib_mune_result_get (r, 0); mulib_mune_result_destroy (r); }
    else { s->status = MULIB_MUNE_OK; error = mulib_mune_rt_string_ (s, "error in error handling", sizeof ("error in error handling") - 1); }
    if (!s->status) { (void)mulib_mune_rt_raise_ (s, error); s->status = status; }
  }
  static int
  mulib_mune_rt_hook_ (mulib_mune_state_t *s, const char *event, size_t line)
  {
    mulib_mune_coroutine_ *co = s->current_thread ? s->current_thread->as.thread : NULL;
    mulib_mune_result_t *r;
    unsigned mask = !strcmp (event, "call") || !strcmp (event, "tail call") ? 1u : !strcmp (event, "return") ? 2u : !strcmp (event, "line") ? 4u : 0;
    if (!co || co->in_hook || co->hook.type != MULIB_MUNE_FUNCTION || (mask && !(co->hook_mask & mask)) || s->status) return !s->status;
    co->in_hook = 1;
    r = mulib_mune_rt_metacall_ (s, co->hook, mulib_mune_rt_string_ (s, event, strlen (event)),
                                line ? mulib_mune_rt_integer_ ((int64_t)line) : mulib_mune_rt_nil_ (), mulib_mune_rt_nil_ (), 2);
    mulib_mune_result_destroy (r); co->in_hook = 0; return !s->status;
  }
  static mulib_mune_object_t *
  mulib_mune_rt_upvalue_ (mulib_mune_value_t fn, int64_t index)
  {
    mulib_mune_object_t *e;
    if (fn.type != MULIB_MUNE_FUNCTION || fn.as.object->as.function.native || index < 1) return NULL;
    for (e = fn.as.object->as.function.environment; e && --index; e = e->as.environment.parent) {}
    return e;
  }
  static mulib_mune_frame_ *
  mulib_mune_rt_debug_frame_ (mulib_mune_state_t *s, mulib_mune_object_t *thread, int64_t level)
  {
    mulib_mune_frame_ *f = thread == s->current_thread ? s->frame : thread->as.thread->frame;
    if (level < 0) return NULL;
    while (f && level--) f = f->previous;
    return f;
  }
  static const char *
  mulib_mune_rt_source_name_ (const mulib_mune_chunk_t *chunk)
  {
    return !chunk ? "=[C]" : chunk->source_name ? chunk->source_name : chunk->walker->root->text;
  }
  static int
  mulib_mune_rt_active_lines_ (mulib_mune_state_t *s, const mulib_ast_t *n, mulib_mune_chunk_t *chunk, mulib_mune_object_t *table)
  {
    const mulib_ast_t *c;
    if (mulib_mune_is_ (n, "function_body")) return 1;
    if (mulib_mune_is_ (n, "statement") || mulib_mune_is_ (n, "return_statement"))
      if (!mulib_mune_rt_rawset_ (s, table, mulib_mune_rt_integer_ ((int64_t)mulib_mune_rt_line_ (chunk, n->offset)), mulib_mune_rt_boolean_ (1))) return 0;
    for (c = n->child; c; c = c->next) if (!mulib_mune_rt_active_lines_ (s, c, chunk, table)) return 0;
    return 1;
  }
  static int
  mulib_mune_rt_debug_ (mulib_mune_state_t *s, const char *op, const mulib_mune_value_t *args, size_t n, mulib_mune_result_t *out)
  {
    mulib_mune_value_t a = mulib_mune_rt_arg_ (args, n, 0), b, c;
    mulib_mune_object_t *thread = s->current_thread;
    if (a.type == MULIB_MUNE_THREAD && (!strcmp (op, "getinfo") || !strcmp (op, "getlocal") || !strcmp (op, "setlocal")
        || !strcmp (op, "gethook") || !strcmp (op, "sethook") || !strcmp (op, "traceback")))
      { thread = a.as.object; ++args; --n; a = mulib_mune_rt_arg_ (args, n, 0); }
    b = mulib_mune_rt_arg_ (args, n, 1); c = mulib_mune_rt_arg_ (args, n, 2);
    if (!strcmp (op, "getregistry"))
      {
        if (!s->registry)
          {
            s->registry = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0); if (!s->registry) return 0;
            if (!mulib_mune_rt_rawset_ (s, s->registry, mulib_mune_rt_integer_ (1), mulib_mune_rt_object_ (s->main_thread))
                || !mulib_mune_rt_rawset_ (s, s->registry, mulib_mune_rt_integer_ (2), mulib_mune_rt_object_ (s->globals))) return 0;
          }
        return mulib_mune_result_push (out, mulib_mune_rt_object_ (s->registry));
      }
    if (!strcmp (op, "getmetatable"))
      {
        if (!n) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "value expected");
        mulib_mune_object_t *meta = a.type >= MULIB_MUNE_STRING ? a.as.object->metatable : NULL;
        if (!meta && a.type != MULIB_MUNE_TABLE && (a.type != MULIB_MUNE_USERDATA || a.as.object->userdata_tag == 3))
          meta = s->type_metatables[a.type == MULIB_MUNE_INTEGER ? MULIB_MUNE_NUMBER : a.type];
        return mulib_mune_result_push (out, mulib_mune_rt_object_ (meta));
      }
    if (!strcmp (op, "setmetatable"))
      {
        if (n < 2) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "metatable expected");
        if (mulib_mune_set_metatable (s, a, b) != MULIB_MUNE_OK) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "metatable must be nil or a table");
        return mulib_mune_result_push (out, a);
      }
    if (!strcmp (op, "getuservalue") || !strcmp (op, "setuservalue"))
      {
        int set = !strcmp (op, "setuservalue"); int64_t index = 1;
        mulib_mune_value_t v = set ? c : b;
        if (v.type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, v, &index, set ? 3 : 2)) return 0;
        if (a.type != MULIB_MUNE_USERDATA || a.as.object->userdata_tag != 2 || index < 1)
          return mulib_mune_result_push (out, mulib_mune_rt_nil_ ());
        if (set)
          return mulib_mune_rt_rawset_ (s, a.as.object->as.userdata.values, mulib_mune_rt_integer_ (index), b) && mulib_mune_result_push (out, a);
        return mulib_mune_result_push (out, mulib_mune_rt_rawget_ (a.as.object->as.userdata.values, mulib_mune_rt_integer_ (index)))
               && mulib_mune_result_push (out, mulib_mune_rt_boolean_ (1));
      }
    if (!strcmp (op, "getupvalue") || !strcmp (op, "setupvalue") || !strcmp (op, "upvalueid") || !strcmp (op, "upvaluejoin"))
      {
        int64_t index; mulib_mune_object_t *e, *cell;
        if (!mulib_mune_rt_need_ (s, a, MULIB_MUNE_FUNCTION, 1) || !mulib_mune_rt_need_int_ (s, b, &index, 2)) return 0;
        e = mulib_mune_rt_upvalue_ (a, index);
        if (!e)
          {
            if (!strcmp (op, "getupvalue") || !strcmp (op, "setupvalue")) return 1;
            return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid upvalue index");
          }
        cell = e->as.environment.cell ? e->as.environment.cell : e;
        if (!strcmp (op, "upvaluejoin"))
          {
            mulib_mune_object_t *other;
            if (!mulib_mune_rt_need_ (s, c, MULIB_MUNE_FUNCTION, 3)
                || !mulib_mune_rt_need_int_ (s, mulib_mune_rt_arg_ (args, n, 3), &index, 4)) return 0;
            other = mulib_mune_rt_upvalue_ (c, index);
            if (!other) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid upvalue index");
            e->as.environment.cell = other->as.environment.cell ? other->as.environment.cell : other; return 1;
          }
        if (!strcmp (op, "upvalueid"))
          {
            mulib_mune_object_t *id = mulib_mune_rt_allocate_ (s, MULIB_MUNE_USERDATA, 0); if (!id) return 0;
            id->userdata_tag = 3; id->as.userdata.identity = (uintptr_t)cell; return mulib_mune_result_push (out, mulib_mune_rt_object_ (id));
          }
        if (!strcmp (op, "setupvalue")) cell->as.environment.value = c;
        if (!mulib_mune_result_push (out, mulib_mune_rt_string_ (s, (const char *)(e + 1), e->as.environment.length))) return 0;
        return strcmp (op, "getupvalue") || mulib_mune_result_push (out, cell->as.environment.value);
      }
    if (!strcmp (op, "getlocal") || !strcmp (op, "setlocal"))
      {
        int64_t level, index; mulib_mune_frame_ *f;
        mulib_mune_object_t *e, *base; size_t count = 0;
        if (!mulib_mune_rt_need_int_ (s, b, &index, 2)) return 0;
        if (a.type == MULIB_MUNE_FUNCTION && !strcmp (op, "getlocal"))
          {
            const mulib_ast_t *params = mulib_mune_child_ (a.as.object->as.function.body, "parameters"), *names = mulib_mune_child_ (params, "names"), *name;
            if (index < 1 || a.as.object->as.function.native) return 1;
            if (a.as.object->as.function.method && !--index) return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "self", 4));
            for (name = names ? names->child : NULL; name && --index; name = name->next) {}
            return !name || mulib_mune_result_push (out, mulib_mune_rt_string_ (s, name->text, name->length));
          }
        if (!mulib_mune_rt_need_int_ (s, a, &level, 1)) return 0;
        f = mulib_mune_rt_debug_frame_ (s, thread, level);
        if (!f) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "stack level out of range");
        if (index < 0)
          {
            uint64_t at = UINT64_C (0) - (uint64_t)index - 1;
            if (!f->varargs || at >= f->varargs->count) return 1;
            if (!strcmp (op, "setlocal")) f->varargs->values[at] = c;
            return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, "(vararg)", 8))
                   && (!strcmp (op, "setlocal") || mulib_mune_result_push (out, f->varargs->values[at]));
          }
        base = f->function.type == MULIB_MUNE_FUNCTION ? f->function.as.object->as.function.environment : NULL;
        for (e = f->environment; e && e != base; e = e->as.environment.parent) ++count;
        if (index < 1 || (uint64_t)index > count) return 1;
        for (e = f->environment, count -= (size_t)index; count; --count) e = e->as.environment.parent;
        if (!strcmp (op, "setlocal")) e->as.environment.value = c;
        return mulib_mune_result_push (out, mulib_mune_rt_string_ (s, (const char *)(e + 1), e->as.environment.length))
               && (!strcmp (op, "setlocal") || mulib_mune_result_push (out, e->as.environment.value));
      }
    if (!strcmp (op, "gethook") || !strcmp (op, "sethook"))
      {
        mulib_mune_coroutine_ *co = thread->as.thread;
        if (!strcmp (op, "sethook"))
          {
            int64_t count = 0; const char *mask;
            if (a.type == MULIB_MUNE_NIL) { co->hook = a; co->hook_mask = 0; co->hook_count = 0; return 1; }
            if (!mulib_mune_rt_need_ (s, a, MULIB_MUNE_FUNCTION, 1) || !(mask = mulib_mune_rt_cstring_ (s, b, 2))
                || (c.type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, c, &count, 3))) return 0;
            if (count < 0) return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "negative hook count");
            co->hook = a; co->hook_count = co->hook_remaining = (size_t)count; co->hook_line = 0;
            co->hook_mask = (strchr (mask, 'c') ? 1u : 0u) | (strchr (mask, 'r') ? 2u : 0u) | (strchr (mask, 'l') ? 4u : 0u); return 1;
          }
        else
          {
            char mask[3]; size_t k = 0;
            if (co->hook_mask & 1) mask[k++] = 'c';
            if (co->hook_mask & 2) mask[k++] = 'r';
            if (co->hook_mask & 4) mask[k++] = 'l';
            return mulib_mune_result_push (out, co->hook) && mulib_mune_result_push (out, mulib_mune_rt_string_ (s, mask, k)) && mulib_mune_result_push (out, mulib_mune_rt_integer_ ((int64_t)co->hook_count));
          }
      }
    if (!strcmp (op, "traceback"))
      {
        mulib_mune_bytes_ bytes = { NULL, 0, 0 }; int64_t level = thread == s->current_thread ? 1 : 0;
        mulib_mune_frame_ *f; char line[256];
        if (a.type != MULIB_MUNE_NIL && a.type != MULIB_MUNE_STRING && a.type != MULIB_MUNE_NUMBER && a.type != MULIB_MUNE_INTEGER) return mulib_mune_result_push (out, a);
        if (b.type != MULIB_MUNE_NIL && !mulib_mune_rt_need_int_ (s, b, &level, 2)) return 0;
        if (a.type != MULIB_MUNE_NIL && mulib_mune_rt_text_ (s, a, 0, &a))
          { (void)mulib_mune_rt_bytes_ (s, &bytes, (const char *)(a.as.object + 1), a.as.object->as.length); (void)mulib_mune_rt_bytes_ (s, &bytes, "\n", 1); }
        (void)mulib_mune_rt_bytes_ (s, &bytes, "stack traceback:", 16);
        for (f = mulib_mune_rt_debug_frame_ (s, thread, level); f && !s->status; f = f->previous)
          {
            int size = snprintf (line, sizeof (line), "\n\t%.180s:%zu: in %s", mulib_mune_rt_source_name_ (f->chunk), mulib_mune_rt_line_ (f->chunk, f->location), f->chunk ? "Lua function" : "native function");
            if (size > 0 && (size_t)size < sizeof (line)) (void)mulib_mune_rt_bytes_ (s, &bytes, line, (size_t)size);
          }
        if (!s->status) (void)mulib_mune_result_push (out, mulib_mune_rt_string_ (s, bytes.data, bytes.length));
        (void)mulib_mune_rt_memory_ (s, bytes.data, bytes.capacity, 0); return !s->status;
      }
    if (!strcmp (op, "getinfo"))
      {
        mulib_mune_frame_ *f = NULL; mulib_mune_object_t *info, *fn, *e;
        const mulib_ast_t *body, *params, *names, *name;
        const char *what = b.type == MULIB_MUNE_NIL ? "flnSrtu" : mulib_mune_rt_cstring_ (s, b, 2), *source;
        size_t upvalues = 0, parameters = 0, k;
        if (!what) return 0;
        if (a.type != MULIB_MUNE_FUNCTION)
          { int64_t level; if (!mulib_mune_rt_need_int_ (s, a, &level, 1)) return 0; f = mulib_mune_rt_debug_frame_ (s, thread, level); if (!f) return mulib_mune_result_push (out, mulib_mune_rt_nil_ ()); a = f->function; }
        fn = a.as.object; body = fn->as.function.body;
        params = mulib_mune_child_ (body, "parameters"); names = mulib_mune_child_ (params, "names");
        for (name = names ? names->child : NULL; name; name = name->next) ++parameters;
        if (fn->as.function.method) ++parameters;
        for (e = fn->as.function.environment; e; e = e->as.environment.parent) ++upvalues;
        info = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0); if (!info) return 0;
        if (!mulib_mune_result_push (out, mulib_mune_rt_object_ (info))) return 0;
#define MULIB_MUNE_INFO_(key,value) do { if (!mulib_mune_rt_setfield_ (s, info, key, sizeof (key)-1, value)) return 0; } while (0)
        for (k = 0; what[k]; ++k)
          switch (what[k])
            {
            case 'f': MULIB_MUNE_INFO_ ("func", a); break;
            case 'l': MULIB_MUNE_INFO_ ("currentline", mulib_mune_rt_integer_ (f && f->chunk ? (int64_t)mulib_mune_rt_line_ (f->chunk, f->location) : -1)); break;
            case 'n': MULIB_MUNE_INFO_ ("namewhat", mulib_mune_rt_string_ (s, "", 0)); break;
            case 'S':
              source = mulib_mune_rt_source_name_ (fn->as.function.chunk);
              MULIB_MUNE_INFO_ ("source", mulib_mune_rt_string_ (s, source, strlen (source)));
              { char short_source[80];
                if (fn->as.function.chunk) mulib_mune_rt_short_source_ (fn->as.function.chunk, short_source); else memcpy (short_source, "[C]", 4);
                MULIB_MUNE_INFO_ ("short_src", mulib_mune_rt_string_ (s, short_source, strlen (short_source))); }
              MULIB_MUNE_INFO_ ("what", mulib_mune_rt_string_ (s, !body ? "C" : mulib_mune_is_ (body, "chunk") ? "main" : "Lua", !body ? 1 : mulib_mune_is_ (body, "chunk") ? 4 : 3));
              MULIB_MUNE_INFO_ ("linedefined", mulib_mune_rt_integer_ (!body ? -1 : mulib_mune_is_ (body, "chunk") ? 0 : (int64_t)mulib_mune_rt_line_ (fn->as.function.chunk, body->offset)));
              MULIB_MUNE_INFO_ ("lastlinedefined", mulib_mune_rt_integer_ (!body ? -1 : mulib_mune_is_ (body, "chunk") ? 0 : (int64_t)mulib_mune_rt_line_ (fn->as.function.chunk, body->offset + body->length))); break;
            case 'u':
              MULIB_MUNE_INFO_ ("nups", mulib_mune_rt_integer_ ((int64_t)upvalues)); MULIB_MUNE_INFO_ ("nparams", mulib_mune_rt_integer_ ((int64_t)parameters));
              MULIB_MUNE_INFO_ ("isvararg", mulib_mune_rt_boolean_ (!body || mulib_mune_is_ (body, "chunk") || mulib_mune_child_ (params, "vararg") != NULL)); break;
            case 't': MULIB_MUNE_INFO_ ("istailcall", mulib_mune_rt_boolean_ (f && f->tail_call)); break;
            case 'r': MULIB_MUNE_INFO_ ("ftransfer", mulib_mune_rt_integer_ (0)); MULIB_MUNE_INFO_ ("ntransfer", mulib_mune_rt_integer_ (0)); break;
            case 'L':
              if (body)
                {
                  mulib_mune_object_t *lines = mulib_mune_rt_allocate_ (s, MULIB_MUNE_TABLE, 0); if (!lines) return 0;
                  if (!mulib_mune_rt_active_lines_ (s, mulib_mune_is_ (body, "chunk") ? body : mulib_mune_child_ (body, "block"), fn->as.function.chunk, lines)) return 0;
                  MULIB_MUNE_INFO_ ("activelines", mulib_mune_rt_object_ (lines));
                }
              break;
            default: return mulib_mune_rt_error_ (s, MULIB_MUNE_RUNTIME, "invalid debug information option");
            }
#undef MULIB_MUNE_INFO_
        return 1;
      }
    if (!strcmp (op, "debug"))
      {
        char line[4096];
        while (fputs ("lua_debug> ", stderr) >= 0 && fgets (line, sizeof (line), stdin))
          {
            mulib_mune_chunk_t *chunk; mulib_mune_error_t error; mulib_mune_result_t *empty, *r;
            if (!strcmp (line, "cont\n")) break;
            chunk = mulib_mune_parse (line, strlen (line), NULL, &error);
            if (!chunk) { fprintf (stderr, "%s\n", error.message); continue; }
            a = mulib_mune_rt_loaded_ (s, chunk, mulib_mune_rt_object_ (s->globals)); empty = mulib_mune_rt_results_ (s);
            r = empty ? mulib_mune_rt_call_ (s, a, empty) : NULL;
            mulib_mune_result_destroy (r); mulib_mune_result_destroy (empty);
            if (s->status) { fprintf (stderr, "%s\n", s->message); s->status = MULIB_MUNE_OK; s->error_value = mulib_mune_rt_nil_ (); s->has_error = 0; }
          }
        return 1;
      }
    return mulib_mune_rt_error_ (s, MULIB_MUNE_INVALID, "unknown debug operation");
  }
#ifdef __cplusplus
}
#endif
#endif
