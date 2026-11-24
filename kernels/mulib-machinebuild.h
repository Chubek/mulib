/**
 * @file mulib-machinebuild.h
 * @brief Parse a bounded automaton DSL and generate standalone JavaScript.
 * Define MULIB_IMPL_MACHINEBUILD, MULIB_IMPL_FST, MULIB_IMPL_VEC and
 * MULIB_IMPL_STRBUF. This module is unrelated to compiler-target machine.h.
 * See manual/mulib-machinebuild.md for grammar and generated runtime contracts.
 */
#ifndef MULIB_MACHINEBUILD_H
#define MULIB_MACHINEBUILD_H
#include "mulib-fst.h"
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Parse/generation results; diagnostics use byte offsets and 1-based positions. */
typedef enum mulib_machinebuild_status {
  MULIB_MACHINEBUILD_OK = 0, MULIB_MACHINEBUILD_INVALID = -1,
  MULIB_MACHINEBUILD_NOMEM = -2, MULIB_MACHINEBUILD_LIMIT = -3,
  MULIB_MACHINEBUILD_SYNTAX = -4, MULIB_MACHINEBUILD_REFERENCE = -5,
  MULIB_MACHINEBUILD_CONFLICT = -6
} mulib_machinebuild_status_t;
/** Zero fields select defaults: 1 MiB source, 4096 states, 16384 edges. */
typedef struct mulib_machinebuild_limits {
  size_t source_bytes, states, edges;
} mulib_machinebuild_limits_t;
/** message is a borrowed static string; diagnostic itself owns no storage. */
typedef struct mulib_machinebuild_diagnostic {
  int status;
  size_t offset, line, column;
  const char *message;
} mulib_machinebuild_diagnostic_t;
/** Owned parsed machine, independent of source storage. */
typedef struct mulib_machinebuild mulib_machinebuild_t;
/** Parse exactly length bytes (no terminating NUL required). out is required
 * and unchanged on failure. On success caller owns *out; no previous *out is
 * freed. Forward state references are supported. limits/diagnostic optional. */
int mulib_machinebuild_parse (const char *source, size_t length,
                              const mulib_machinebuild_limits_t *limits,
                              mulib_machinebuild_t **out,
                              mulib_machinebuild_diagnostic_t *diagnostic);
/** Release parsed machine, NULL allowed. */
void mulib_machinebuild_destroy (mulib_machinebuild_t *machine);
/** Borrowed read-only graph/name, valid until destroy; NULL for NULL machine. */
const mulib_fst_t *mulib_machinebuild_fst (const mulib_machinebuild_t *machine);
/** Start state ID or MULIB_FST_NONE for NULL. */
size_t mulib_machinebuild_start (const mulib_machinebuild_t *machine);
/** Borrowed machine name. */
const char *mulib_machinebuild_name (const mulib_machinebuild_t *machine);
/** Borrowed state name, NULL for missing ID. */
const char *mulib_machinebuild_state_name (const mulib_machinebuild_t *machine, size_t state);
/** Append an ES2015 factory expression to initialized strbuf. Transactional on
 * failure. Assign/use the expression in a browser or Node; no eval/dependencies
 * appear in generated code. Each factory call returns independent state. */
int mulib_machinebuild_emit_js (const mulib_machinebuild_t *machine, mulib_strbuf_t *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_MACHINEBUILD) && !defined(MULIB_MACHINEBUILD_IMPLEMENTED)
#define MULIB_MACHINEBUILD_IMPLEMENTED
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_machinebuild_name_ { char text[64]; } mulib_machinebuild_name_;
struct mulib_machinebuild {
  mulib_fst_t *fst;
  mulib_vec_t names;
  size_t start;
  char name[64];
};
enum { MULIB_MACHINEBUILD_ID_ = 256, MULIB_MACHINEBUILD_NUMBER_,
       MULIB_MACHINEBUILD_ARROW_, MULIB_MACHINEBUILD_DOTS_, MULIB_MACHINEBUILD_BAD_ };
typedef struct mulib_machinebuild_parser_ {
  const char *source;
  size_t length, pos, offset, token_length;
  int token, status, pass, has_start;
  uint32_t number;
  const char *message;
  mulib_machinebuild_t *machine;
} mulib_machinebuild_parser_;
static int mulib_machinebuild_alpha_ (unsigned char c)
{ return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
static int mulib_machinebuild_digit_ (unsigned char c) { return c >= '0' && c <= '9'; }
static void mulib_machinebuild_next_ (mulib_machinebuild_parser_ *p)
{
  unsigned char c;
  for (;;)
    {
      while (p->pos < p->length)
        {
          c = (unsigned char)p->source[p->pos];
          if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
          ++p->pos;
        }
      if (p->pos < p->length && (p->source[p->pos] == '#'
          || (p->source[p->pos] == '/' && p->length - p->pos > 1
              && p->source[p->pos + 1] == '/')))
        { while (p->pos < p->length && p->source[p->pos] != '\n') ++p->pos; }
      else break;
    }
  p->offset = p->pos; p->token_length = 0; p->number = 0;
  if (p->pos == p->length) { p->token = 0; return; }
  c = (unsigned char)p->source[p->pos++];
  p->token = MULIB_MACHINEBUILD_BAD_;
  if (mulib_machinebuild_alpha_ (c))
    {
      while (p->pos < p->length && (mulib_machinebuild_alpha_ ((unsigned char)p->source[p->pos])
             || mulib_machinebuild_digit_ ((unsigned char)p->source[p->pos]))) ++p->pos;
      p->token = p->pos - p->offset <= 63 ? MULIB_MACHINEBUILD_ID_ : MULIB_MACHINEBUILD_BAD_;
    }
  else if (mulib_machinebuild_digit_ (c))
    {
      int overflow = 0;
      --p->pos;
      while (p->pos < p->length && mulib_machinebuild_digit_ ((unsigned char)p->source[p->pos]))
        {
          unsigned digit = (unsigned)(p->source[p->pos++] - '0');
          if (p->number > (UINT32_MAX - digit) / 10) overflow = 1;
          if (!overflow) p->number = p->number * 10 + digit;
        }
      p->token = overflow ? MULIB_MACHINEBUILD_BAD_ : MULIB_MACHINEBUILD_NUMBER_;
    }
  else if ((c == '-' || c == '.') && p->pos < p->length
           && p->source[p->pos] == (c == '-' ? '>' : '.'))
    { ++p->pos; p->token = c == '-' ? MULIB_MACHINEBUILD_ARROW_ : MULIB_MACHINEBUILD_DOTS_; }
  else if (c == '{' || c == '}' || c == ';') p->token = c;
  p->token_length = p->pos - p->offset;
}
static int mulib_machinebuild_fail_ (mulib_machinebuild_parser_ *p, int status, const char *message)
{ if (!p->status) { p->status = status; p->message = message; } return 0; }
static int mulib_machinebuild_word_ (const mulib_machinebuild_parser_ *p, const char *word)
{
  return p->token == MULIB_MACHINEBUILD_ID_ && p->token_length == strlen (word)
         && !memcmp (p->source + p->offset, word, p->token_length);
}
static int mulib_machinebuild_expect_ (mulib_machinebuild_parser_ *p, int token)
{
  if (p->token != token) return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_SYNTAX,
                                                       "unexpected token, identifier too long or invalid uint32");
  mulib_machinebuild_next_ (p); return 1;
}
static int mulib_machinebuild_identifier_ (mulib_machinebuild_parser_ *p, char name[64])
{
  if (p->token != MULIB_MACHINEBUILD_ID_) return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_SYNTAX,
                                                                        "expected identifier (1..63 ASCII characters)");
  memcpy (name, p->source + p->offset, p->token_length); name[p->token_length] = 0;
  mulib_machinebuild_next_ (p); return 1;
}
static size_t mulib_machinebuild_find_ (const mulib_machinebuild_t *m, const char *name)
{
  size_t k;
  for (k = 0; k < m->names.length; ++k)
    if (!strcmp (((const mulib_machinebuild_name_ *)m->names.data)[k].text, name)) return k;
  return MULIB_FST_NONE;
}
static int mulib_machinebuild_fst_status_ (mulib_machinebuild_parser_ *p, int status)
{
  if (status == MULIB_FST_OK) return 1;
  return mulib_machinebuild_fail_ (p,
      status == MULIB_FST_NOMEM ? MULIB_MACHINEBUILD_NOMEM :
      status == MULIB_FST_LIMIT ? MULIB_MACHINEBUILD_LIMIT :
      status == MULIB_FST_CONFLICT ? MULIB_MACHINEBUILD_CONFLICT : MULIB_MACHINEBUILD_SYNTAX,
      status == MULIB_FST_NOMEM ? "allocation failed" :
      status == MULIB_FST_LIMIT ? "graph resource limit" :
      status == MULIB_FST_CONFLICT ? "overlapping transition ranges" : "reversed input range");
}
static int mulib_machinebuild_body_ (mulib_machinebuild_parser_ *p)
{
  mulib_machinebuild_t *m = p->machine;
  char name[64], target[64];
  if (!mulib_machinebuild_word_ (p, "machine"))
    return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_SYNTAX, "expected machine");
  mulib_machinebuild_next_ (p);
  if (!mulib_machinebuild_identifier_ (p, name) || !mulib_machinebuild_expect_ (p, '{')) return 0;
  if (!p->pass) strcpy (m->name, name);
  while (p->token != '}')
    {
      size_t statement = p->offset;
      if (mulib_machinebuild_word_ (p, "state"))
        {
          int accepting = 0;
          size_t id;
          mulib_machinebuild_name_ stored;
          mulib_machinebuild_next_ (p);
          if (!mulib_machinebuild_identifier_ (p, stored.text)) return 0;
          if (mulib_machinebuild_word_ (p, "accept")) { accepting = 1; mulib_machinebuild_next_ (p); }
          if (!mulib_machinebuild_expect_ (p, ';')) return 0;
          if (!p->pass)
            {
              p->offset = statement;
              if (mulib_machinebuild_find_ (m, stored.text) != MULIB_FST_NONE)
                return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_CONFLICT, "duplicate state");
              if (!mulib_machinebuild_fst_status_ (p, mulib_fst_add_state (m->fst, accepting, &id))) return 0;
              if (!mulib_vec_push (&m->names, &stored))
                return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_NOMEM, "allocation failed");
            }
        }
      else if (mulib_machinebuild_word_ (p, "start"))
        {
          mulib_machinebuild_next_ (p);
          if (!mulib_machinebuild_identifier_ (p, name) || !mulib_machinebuild_expect_ (p, ';')) return 0;
          if (p->pass)
            {
              p->offset = statement;
              if (p->has_start) return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_CONFLICT, "duplicate start");
              m->start = mulib_machinebuild_find_ (m, name); p->has_start = 1;
              if (m->start == MULIB_FST_NONE)
                return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_REFERENCE, "unknown start state");
            }
        }
      else if (mulib_machinebuild_word_ (p, "transition"))
        {
          uint32_t lower, upper, output = 0;
          int emits = 0;
          size_t from, to;
          mulib_machinebuild_next_ (p);
          if (!mulib_machinebuild_identifier_ (p, name)
              || !mulib_machinebuild_expect_ (p, MULIB_MACHINEBUILD_ARROW_)
              || !mulib_machinebuild_identifier_ (p, target)) return 0;
          if (!mulib_machinebuild_word_ (p, "on"))
            return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_SYNTAX, "expected on");
          mulib_machinebuild_next_ (p); lower = upper = p->number;
          if (!mulib_machinebuild_expect_ (p, MULIB_MACHINEBUILD_NUMBER_)) return 0;
          if (p->token == MULIB_MACHINEBUILD_DOTS_)
            { mulib_machinebuild_next_ (p); upper = p->number;
              if (!mulib_machinebuild_expect_ (p, MULIB_MACHINEBUILD_NUMBER_)) return 0; }
          if (mulib_machinebuild_word_ (p, "emit"))
            { emits = 1; mulib_machinebuild_next_ (p); output = p->number;
              if (!mulib_machinebuild_expect_ (p, MULIB_MACHINEBUILD_NUMBER_)) return 0; }
          if (!mulib_machinebuild_expect_ (p, ';')) return 0;
          if (p->pass)
            {
              p->offset = statement;
              from = mulib_machinebuild_find_ (m, name); to = mulib_machinebuild_find_ (m, target);
              if (from == MULIB_FST_NONE || to == MULIB_FST_NONE)
                return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_REFERENCE, "unknown transition state");
              if (!mulib_machinebuild_fst_status_ (p, mulib_fst_add_range (m->fst, from, to,
                                                        lower, upper, emits, output, NULL))) return 0;
            }
        }
      else return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_SYNTAX, "expected state, start or transition");
      /* Semantic checks temporarily locate errors at the statement start.
       * Restore the already lexed token's offset before continuing. */
      p->offset = p->pos - p->token_length;
    }
  if (!mulib_machinebuild_expect_ (p, '}') || !mulib_machinebuild_expect_ (p, 0)) return 0;
  if (p->pass && !p->has_start)
    return mulib_machinebuild_fail_ (p, MULIB_MACHINEBUILD_REFERENCE, "missing start declaration");
  return 1;
}
int mulib_machinebuild_parse (const char *source, size_t length,
                              const mulib_machinebuild_limits_t *limits,
                              mulib_machinebuild_t **out, mulib_machinebuild_diagnostic_t *diagnostic)
{
  mulib_machinebuild_parser_ p;
  size_t k, max_source = limits && limits->source_bytes ? limits->source_bytes : 1048576;
  memset (&p, 0, sizeof (p)); p.source = source; p.length = length;
  p.message = "success";
  if (!source || !out)
    mulib_machinebuild_fail_ (&p, MULIB_MACHINEBUILD_INVALID, "NULL source or output");
  else if (length > max_source)
    mulib_machinebuild_fail_ (&p, MULIB_MACHINEBUILD_LIMIT, "source byte limit");
  else
    {
      p.machine = (mulib_machinebuild_t *)calloc (1, sizeof (*p.machine));
      if (p.machine)
        {
          mulib_vec_init (&p.machine->names, sizeof (mulib_machinebuild_name_));
          p.machine->start = MULIB_FST_NONE;
          p.machine->fst = mulib_fst_create (limits && limits->states ? limits->states : 4096,
                                           limits && limits->edges ? limits->edges : 16384);
        }
      if (!p.machine || !p.machine->fst)
        mulib_machinebuild_fail_ (&p, MULIB_MACHINEBUILD_NOMEM, "allocation failed");
      else
        for (p.pass = 0; p.pass < 2; ++p.pass)
          { p.pos = 0; mulib_machinebuild_next_ (&p); if (!mulib_machinebuild_body_ (&p)) break; }
    }
  if (diagnostic)
    {
      diagnostic->status = p.status; diagnostic->offset = p.offset;
      diagnostic->line = diagnostic->column = 1; diagnostic->message = p.message;
      for (k = 0; source && k < p.offset; ++k)
        if (source[k] == '\n') { ++diagnostic->line; diagnostic->column = 1; }
        else ++diagnostic->column;
    }
  if (p.status) mulib_machinebuild_destroy (p.machine);
  else *out = p.machine;
  return p.status;
}
void mulib_machinebuild_destroy (mulib_machinebuild_t *m)
{ if (m) { mulib_fst_destroy (m->fst); mulib_vec_free (&m->names); free (m); } }
const mulib_fst_t *mulib_machinebuild_fst (const mulib_machinebuild_t *m) { return m ? m->fst : NULL; }
size_t mulib_machinebuild_start (const mulib_machinebuild_t *m) { return m ? m->start : MULIB_FST_NONE; }
const char *mulib_machinebuild_name (const mulib_machinebuild_t *m) { return m ? m->name : NULL; }
const char *mulib_machinebuild_state_name (const mulib_machinebuild_t *m, size_t id)
{ return m && id < m->names.length ? ((const mulib_machinebuild_name_ *)m->names.data)[id].text : NULL; }
static int mulib_machinebuild_number_ (mulib_strbuf_t *s, size_t n)
{
  char text[3 * sizeof (size_t) + 1];
  int length = snprintf (text, sizeof (text), "%zu", n);
  return length > 0 && (size_t)length < sizeof (text) && mulib_strbuf_append_n (s, text, (size_t)length);
}
static int mulib_machinebuild_u32_ (mulib_strbuf_t *s, uint32_t n)
{
  char text[11];
  int length = snprintf (text, sizeof (text), "%" PRIu32, n);
  return length > 0 && (size_t)length < sizeof (text) && mulib_strbuf_append_n (s, text, (size_t)length);
}
int mulib_machinebuild_emit_js (const mulib_machinebuild_t *m, mulib_strbuf_t *out)
{
  mulib_strbuf_t s;
  size_t i, k;
  int status = MULIB_MACHINEBUILD_NOMEM;
  if (!m || !out) return MULIB_MACHINEBUILD_INVALID;
  mulib_strbuf_init (&s);
#define MULIB_MACHINEBUILD_TEXT_(text) do { if (!mulib_strbuf_append (&s, (text))) goto done; } while (0)
  MULIB_MACHINEBUILD_TEXT_("(() => {\n\"use strict\";\nconst names = [");
  for (i = 0; i < m->names.length; ++i)
    {
      if (i) MULIB_MACHINEBUILD_TEXT_(",");
      MULIB_MACHINEBUILD_TEXT_("\"");
      MULIB_MACHINEBUILD_TEXT_(mulib_machinebuild_state_name (m, i));
      MULIB_MACHINEBUILD_TEXT_("\"");
    }
  MULIB_MACHINEBUILD_TEXT_("], accepting = [");
  for (i = 0; i < m->names.length; ++i)
    {
      mulib_fst_state_t state;
      if (mulib_fst_state (m->fst, i, &state) != MULIB_FST_OK)
        { status = MULIB_MACHINEBUILD_INVALID; goto done; }
      if (i) MULIB_MACHINEBUILD_TEXT_(",");
      MULIB_MACHINEBUILD_TEXT_(state.accepting ? "true" : "false");
    }
  MULIB_MACHINEBUILD_TEXT_("], edges = [\n");
  for (i = 0; i < m->names.length; ++i)
    {
      mulib_fst_state_t state;
      int first = 1;
      if (mulib_fst_state (m->fst, i, &state) != MULIB_FST_OK)
        { status = MULIB_MACHINEBUILD_INVALID; goto done; }
      if (i) MULIB_MACHINEBUILD_TEXT_(",\n");
      MULIB_MACHINEBUILD_TEXT_("[");
      for (k = state.first_edge; k != MULIB_FST_NONE; )
        {
          mulib_fst_edge_t e;
          if (mulib_fst_edge (m->fst, k, &e) != MULIB_FST_OK)
            { status = MULIB_MACHINEBUILD_INVALID; goto done; }
          if (!first) MULIB_MACHINEBUILD_TEXT_(",");
          first = 0;
          MULIB_MACHINEBUILD_TEXT_("[");
          if (!mulib_machinebuild_u32_ (&s, e.lower)) goto done;
          MULIB_MACHINEBUILD_TEXT_(",");
          if (!mulib_machinebuild_u32_ (&s, e.upper)) goto done;
          MULIB_MACHINEBUILD_TEXT_(",");
          if (!mulib_machinebuild_number_ (&s, e.to)) goto done;
          MULIB_MACHINEBUILD_TEXT_(",");
          if (e.emits) { if (!mulib_machinebuild_u32_ (&s, e.output)) goto done; }
          else MULIB_MACHINEBUILD_TEXT_("null");
          MULIB_MACHINEBUILD_TEXT_("]"); k = e.next;
        }
      MULIB_MACHINEBUILD_TEXT_("]");
    }
  MULIB_MACHINEBUILD_TEXT_("\n], start = ");
  if (!mulib_machinebuild_number_ (&s, m->start)) goto done;
  MULIB_MACHINEBUILD_TEXT_(
      ";\nfunction check(token) {\n"
      "  if (!Number.isInteger(token) || token < 0 || token > 4294967295)\n"
      "    throw new RangeError('expected uint32 token');\n"
      "}\nfunction move(state, token) {\n"
      "  for (const e of edges[state]) if (token >= e[0] && token <= e[1]) return e;\n"
      "  return null;\n}\nreturn function () {\n"
      "  let state = start;\n"
      "  return {\n"
      "    get state() { return names[state]; },\n"
      "    get accepting() { return accepting[state]; },\n"
      "    reset() { state = start; },\n"
      "    step(token) {\n"
      "      check(token);\n"
      "      const e = move(state, token);\n"
      "      if (e) state = e[2];\n"
      "      return {matched: e !== null, output: e && e[3] !== null ? [e[3]] : [],\n"
      "              state: names[state], accepting: accepting[state]};\n"
      "    },\n"
      "    run(tokens) {\n"
      "      if (!Array.isArray(tokens) && !(ArrayBuffer.isView(tokens) &&\n"
      "          typeof tokens.length === 'number')) throw new TypeError('expected token array');\n"
      "      for (const token of tokens) check(token);\n"
      "      let current = start; const output = [];\n"
      "      for (const token of tokens) {\n"
      "        const e = move(current, token);\n"
      "        if (!e) return {accepted: false, output: [], state: names[current]};\n"
      "        current = e[2]; if (e[3] !== null) output.push(e[3]);\n"
      "      }\n"
      "      return {accepted: accepting[current], output: accepting[current] ? output : [],\n"
      "              state: names[current]};\n"
      "    }\n  };\n};\n})()\n");
  if (mulib_strbuf_append_n (out, s.data, s.length)) status = MULIB_MACHINEBUILD_OK;
done:
#undef MULIB_MACHINEBUILD_TEXT_
  mulib_strbuf_free (&s); return status;
}
#ifdef __cplusplus
}
#endif
#endif
