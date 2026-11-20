/**
 * @file mulib-termscript-ast.h
 * @brief Bounded scanner and owned syntax tree for terse Termscript.
 * Define MULIB_IMPL_TERMSCRIPT_AST in one translation unit and provide the
 * PARSEC and UTF8 implementations. Parsing does not execute any syntax.
 */
#ifndef MULIB_TERMSCRIPT_AST_H
#define MULIB_TERMSCRIPT_AST_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    MULIB_TERMSCRIPT_WORD,
    MULIB_TERMSCRIPT_INTEGER,
    MULIB_TERMSCRIPT_STRING,
    MULIB_TERMSCRIPT_COMMAND,
    MULIB_TERMSCRIPT_CHARACTER,
    MULIB_TERMSCRIPT_SYMBOL,
    MULIB_TERMSCRIPT_ARRAY_SYMBOL,
    MULIB_TERMSCRIPT_RECORD_SYMBOL,
    MULIB_TERMSCRIPT_PARAMETER,
    MULIB_TERMSCRIPT_KEY,
    MULIB_TERMSCRIPT_BLOCK,
    MULIB_TERMSCRIPT_TUPLE,
    MULIB_TERMSCRIPT_ARRAY,
    MULIB_TERMSCRIPT_RECORD,
    MULIB_TERMSCRIPT_FIELD,
    MULIB_TERMSCRIPT_DIRECTIVE,
    MULIB_TERMSCRIPT_ESCAPE
  } mulib_termscript_kind_t;

  typedef struct
  {
    mulib_termscript_kind_t kind;
    size_t offset, length, first, next;
    int64_t integer;
  } mulib_termscript_node_t;

  typedef struct
  {
    char *source;
    size_t length;
    mulib_termscript_node_t *nodes;
    size_t count, root;
  } mulib_termscript_program_t;

  typedef struct
  {
    size_t offset;
    const char *message; /* Static. NULL on success. */
  } mulib_termscript_error_t;

  /** Parse exactly length bytes (NULL source is allowed only at length zero).
   * Return 1 with owned source and nodes, or 0 leaving out unchanged. Start
   * with a zero-initialized out; free before reuse. Optional error offsets
   * are byte offsets, possibly at EOF. Limits: 1 MiB, 65536 nodes and 128
   * nested containers. Embedded NUL and invalid UTF-8 are rejected.
   *
   * root is the first top-level sibling. root, first and next use SIZE_MAX
   * for no index. Containers and character nodes span their whole spelling;
   * string and command spans omit the quotes and preserve raw escapes.
   * Reference and key spans omit the sigil. A field spans its bare name and
   * has exactly one child (its value). A directive spans from '%' up to but
   * not including the line ending; its interpretation is left to the caller.
   * An escape literal spans only its case-sensitive symbolic name, without
   * the delimiters or surrounding whitespace; parsing never emits an escape.
   * integer holds the signed literal or decoded Unicode scalar as applicable.
   *
   * ASCII whitespace and semicolons separate values. "% " starts a line
   * comment. Records require commas between fields; tuples and arrays allow
   * them, and all three allow a trailing comma. Quoted escapes are backslash,
   * double quote, backtick, a/b/e/f/n/r/t/v/0, and x followed by two hex digits.
   * [[name]] accepts 1..63 ASCII letters, digits, underscores, or hyphens,
   * with optional inner ASCII whitespace before and after the name. Use
   * whitespace between the openers of nested arrays (e.g. [ [1], [2] ]).
   */
  int mulib_termscript_parse (const char *source, size_t length,
                              mulib_termscript_program_t *out,
                              mulib_termscript_error_t *error);
  /** Free a program and clear it. NULL is harmless. */
  void mulib_termscript_program_free (mulib_termscript_program_t *program);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TERMSCRIPT_AST_H */

#if defined(MULIB_IMPL_TERMSCRIPT_AST) && !defined(MULIB_TERMSCRIPT_AST_IMPLEMENTED)
#define MULIB_TERMSCRIPT_AST_IMPLEMENTED
#include "mulib-parsec.h"
#include "mulib-utf8.h"
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_TERMSCRIPT_SOURCE_LIMIT_ = 1024 * 1024,
    MULIB_TERMSCRIPT_NODE_LIMIT_ = 65536,
    MULIB_TERMSCRIPT_DEPTH_LIMIT_ = 128,
    MULIB_TERMSCRIPT_ESCAPE_NAME_LIMIT_ = 63
  };

  typedef struct
  {
    mulib_parsec_parser_t items[2], body[2], escape[2], each, root;
  } mulib_termscript_quote_rule_;

  typedef struct
  {
    mulib_termscript_program_t program;
    mulib_termscript_error_t *error;
    size_t pos, capacity;
    unsigned char double_quote, backtick;
    mulib_termscript_quote_rule_ quotes[2];
    mulib_parsec_parser_t atom_byte, field_byte, line_byte, atom, field, line;
    mulib_parsec_parser_t escape_byte, escape_name, escape_end;
  } mulib_termscript_reader_;

  static int
  mulib_termscript_parse_fail_ (mulib_termscript_error_t *e, size_t pos,
                                 const char *msg)
  {
    if (e)
      {
        e->offset = pos;
        e->message = msg;
      }
    return 0;
  }

  static int
  mulib_termscript_space_ (unsigned char c)
  {
    return c == ' ' || (c >= '\t' && c <= '\r');
  }

  static int
  mulib_termscript_atom_byte_ (unsigned char c, void *user)
  {
    (void)user;
    if (c <= ' ' || c == 0x7f)
      return 0;
    switch (c)
      {
      case '{': case '}': case '(': case ')': case '[': case ']':
      case '<': case '>': case ',': case ';': case '"': case '`':
      case '#': case '@': case '%': case '$': case '!': case '\\':
        return 0;
      default:
        return 1;
      }
  }

  static int
  mulib_termscript_field_byte_ (unsigned char c, void *user)
  {
    return c != ':' && mulib_termscript_atom_byte_ (c, user);
  }

  static int
  mulib_termscript_line_byte_ (unsigned char c, void *user)
  {
    (void)user;
    return c != '\r' && c != '\n';
  }

  static int
  mulib_termscript_escape_byte_ (unsigned char c, void *user)
  {
    (void)user;
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
           || (c >= '0' && c <= '9') || c == '_' || c == '-';
  }

  static int
  mulib_termscript_quote_byte_ (unsigned char c, void *user)
  {
    return c != *(const unsigned char *)user && c != '\\';
  }

  static void
  mulib_termscript_quote_rule_init_ (mulib_termscript_quote_rule_ *q,
                                     unsigned char *quote)
  {
    /* Scan the inner text as either a regular byte or an escaped byte. The
     * escape validator checks the spelling after both delimiters match. */
    q->escape[0] = mulib_parsec_byte ('\\');
    q->escape[1] = mulib_parsec_any ();
    q->body[0] = mulib_parsec_satisfy (mulib_termscript_quote_byte_, quote,
                                      "quoted byte");
    q->body[1] = mulib_parsec_sequence (q->escape, 2);
    q->each = mulib_parsec_choice (q->body, 2);
    q->items[0] = mulib_parsec_repeat (&q->each, 0, SIZE_MAX);
    q->items[1] = mulib_parsec_byte (*quote);
    q->root = mulib_parsec_sequence (q->items, 2);
  }

  static void
  mulib_termscript_rules_init_ (mulib_termscript_reader_ *r)
  {
    r->double_quote = '"';
    r->backtick = '`';
    mulib_termscript_quote_rule_init_ (&r->quotes[0], &r->double_quote);
    mulib_termscript_quote_rule_init_ (&r->quotes[1], &r->backtick);
    r->atom_byte = mulib_parsec_satisfy (mulib_termscript_atom_byte_, NULL,
                                         "word byte");
    r->field_byte = mulib_parsec_satisfy (mulib_termscript_field_byte_, NULL,
                                          "field name byte");
    r->line_byte = mulib_parsec_satisfy (mulib_termscript_line_byte_, NULL,
                                         "line byte");
    r->escape_byte = mulib_parsec_satisfy (mulib_termscript_escape_byte_, NULL,
                                           "escape name byte");
    r->atom = mulib_parsec_repeat (&r->atom_byte, 1, SIZE_MAX);
    r->field = mulib_parsec_repeat (&r->field_byte, 1, SIZE_MAX);
    r->line = mulib_parsec_repeat (&r->line_byte, 0, SIZE_MAX);
    r->escape_name = mulib_parsec_repeat (&r->escape_byte, 1,
                                          MULIB_TERMSCRIPT_ESCAPE_NAME_LIMIT_);
    r->escape_end = mulib_parsec_literal ("]]", 2);
  }

  static int
  mulib_termscript_scan_ (mulib_termscript_reader_ *r,
                           const mulib_parsec_parser_t *rule,
                           const char *message)
  {
    const size_t remain = r->program.length - r->pos;
    /* All lexical rules are shallow and take at most a constant amount of
     * work per byte. Neither their repeats nor the AST builder backtrack. */
    const size_t work = 4 * remain + 32;
    const mulib_parsec_options_t options = { 16, work, 1 };
    mulib_parsec_result_t result;
    int ok = mulib_parsec_run_bounded (rule, r->program.source + r->pos,
                                      remain, &options, work, &result);
    if (ok == 1)
      r->pos += result.consumed;
    else
      mulib_termscript_parse_fail_ (
          r->error, r->pos + result.error.offset,
          ok < 0 ? "Termscript scanner resource limit" : message);
    mulib_parsec_result_free (&result);
    return ok == 1;
  }

  static unsigned char
  mulib_termscript_at_ (const mulib_termscript_reader_ *r)
  {
    return (unsigned char)r->program.source[r->pos];
  }

  static int
  mulib_termscript_prefix_ (const mulib_termscript_reader_ *r,
                             const char *prefix, size_t length)
  {
    return length <= r->program.length - r->pos
           && !memcmp (r->program.source + r->pos, prefix, length);
  }

  static int
  mulib_termscript_skip_ (mulib_termscript_reader_ *r)
  {
    for (;;)
      {
        while (mulib_termscript_space_ (mulib_termscript_at_ (r))
               || mulib_termscript_at_ (r) == ';')
          ++r->pos;
        if (!mulib_termscript_prefix_ (r, "% ", 2))
          return 1;
        if (!mulib_termscript_scan_ (r, &r->line, "invalid comment"))
          return 0;
      }
  }

  static size_t
  mulib_termscript_add_ (mulib_termscript_reader_ *r,
                         mulib_termscript_kind_t kind, size_t offset,
                         size_t length)
  {
    size_t i = r->program.count;
    mulib_termscript_node_t *n;
    if (i == MULIB_TERMSCRIPT_NODE_LIMIT_)
      {
        mulib_termscript_parse_fail_ (r->error, offset, "Termscript node limit");
        return SIZE_MAX;
      }
    if (i == r->capacity)
      {
        size_t capacity = r->capacity ? 2 * r->capacity : 32;
        n = capacity <= SIZE_MAX / sizeof (*n)
                ? (mulib_termscript_node_t *)realloc (r->program.nodes,
                                                      capacity * sizeof (*n))
                : NULL;
        if (!n)
          {
            mulib_termscript_parse_fail_ (r->error, offset, "out of memory");
            return SIZE_MAX;
          }
        r->program.nodes = n;
        r->capacity = capacity;
      }
    n = &r->program.nodes[i];
    n->kind = kind;
    n->offset = offset;
    n->length = length;
    n->first = n->next = SIZE_MAX;
    n->integer = 0;
    ++r->program.count;
    return i;
  }

  static int
  mulib_termscript_digit_ (unsigned char c)
  {
    return c >= '0' && c <= '9';
  }

  static int
  mulib_termscript_hex_ (unsigned char c)
  {
    return mulib_termscript_digit_ (c) || (c >= 'A' && c <= 'F')
           || (c >= 'a' && c <= 'f');
  }

  /* Return 1 for a decimal integer, 0 for a nonnumeric word, -1 on overflow.
   * Recognize the entire spelling before checking its numeric magnitude. */
  static int
  mulib_termscript_integer_ (const char *s, size_t n, int64_t *out)
  {
    const int negative = s[0] == '-';
    const size_t first = negative || s[0] == '+';
    const uint64_t limit = (uint64_t)INT64_MAX + (unsigned)negative;
    uint64_t value = 0;
    size_t i;
    if (first == n)
      return 0;
    for (i = first; i < n; ++i)
      if (!mulib_termscript_digit_ ((unsigned char)s[i]))
        return 0;
    for (i = first; i < n; ++i)
      {
        unsigned digit = (unsigned)(s[i] - '0');
        if (value > (limit - digit) / 10)
          return -1;
        value = 10 * value + digit;
      }
    *out = !negative ? (int64_t)value
                     : value == limit ? INT64_MIN : -(int64_t)value;
    return 1;
  }

  static int
  mulib_termscript_validate_quote_ (mulib_termscript_reader_ *r, size_t begin,
                                     size_t end)
  {
    const char *s = r->program.source;
    size_t i;
    for (i = begin; i < end; ++i)
      if (s[i] == '\\')
        {
          size_t escape = i++;
          switch (s[i])
            {
            case '\\': case '"': case '`':
            case 'a': case 'b': case 'e': case 'f': case 'n':
            case 'r': case 't': case 'v': case '0':
              break;
            case 'x':
              if (end - i < 3 || !mulib_termscript_hex_ ((unsigned char)s[i+1])
                  || !mulib_termscript_hex_ ((unsigned char)s[i+2]))
                return mulib_termscript_parse_fail_ (r->error, escape,
                                                     "expected two hex digits");
              i += 2;
              break;
            default:
              return mulib_termscript_parse_fail_ (r->error, escape,
                                                   "unknown quoted escape");
            }
        }
    return 1;
  }

  static size_t
  mulib_termscript_character_ (mulib_termscript_reader_ *r)
  {
    static const struct
    {
      const char *name;
      uint32_t scalar;
    } names[] = {
      { "space", ' ' }, { "tab", '\t' }, { "newline", '\n' },
      { "return", '\r' }, { "escape", 27 }, { "nul", 0 },
      { "backspace", '\b' }, { "delete", 127 }
    };
    size_t begin = r->pos, start = begin + 2, length, used, i, node;
    uint32_t scalar;
    unsigned char c;
    r->pos = start;
    c = mulib_termscript_at_ (r);
    if (!c || mulib_termscript_space_ (c))
      {
        mulib_termscript_parse_fail_ (r->error, start, "expected character");
        return SIZE_MAX;
      }
    if (mulib_termscript_atom_byte_ (c, NULL))
      {
        if (!mulib_termscript_scan_ (r, &r->atom, "expected character"))
          return SIZE_MAX;
      }
    else
      ++r->pos; /* A delimiter or sigil can itself be the character. */
    length = r->pos - start;
    if (!mulib_utf8_decode ((const unsigned char *)r->program.source + start,
                            length, &scalar, &used)
        || used != length)
      {
        for (i = 0; i < sizeof names / sizeof names[0]; ++i)
          if (strlen (names[i].name) == length
              && !memcmp (r->program.source + start, names[i].name, length))
            break;
        if (i == sizeof names / sizeof names[0])
          {
            mulib_termscript_parse_fail_ (r->error, start,
                                           "expected one Unicode character");
            return SIZE_MAX;
          }
        scalar = names[i].scalar;
      }
    node = mulib_termscript_add_ (r, MULIB_TERMSCRIPT_CHARACTER, begin,
                                  r->pos - begin);
    if (node != SIZE_MAX)
      r->program.nodes[node].integer = scalar;
    return node;
  }

  static size_t
  mulib_termscript_escape_ (mulib_termscript_reader_ *r)
  {
    size_t name, length;
    r->pos += 2;
    while (mulib_termscript_space_ (mulib_termscript_at_ (r)))
      ++r->pos;
    name = r->pos;
    if (!mulib_termscript_scan_ (r, &r->escape_name, "expected escape name"))
      return SIZE_MAX;
    length = r->pos - name;
    if (mulib_termscript_escape_byte_ (mulib_termscript_at_ (r), NULL))
      {
        mulib_termscript_parse_fail_ (r->error, r->pos, "Termscript escape name limit");
        return SIZE_MAX;
      }
    while (mulib_termscript_space_ (mulib_termscript_at_ (r)))
      ++r->pos;
    if (!mulib_termscript_scan_ (r, &r->escape_end, "expected ']]' after escape name"))
      return SIZE_MAX;
    return mulib_termscript_add_ (r, MULIB_TERMSCRIPT_ESCAPE, name, length);
  }

  static size_t mulib_termscript_value_ (mulib_termscript_reader_ *, size_t);

  static size_t
  mulib_termscript_record_field_ (mulib_termscript_reader_ *r, size_t depth)
  {
    size_t offset = r->pos, length, field, value;
    if (!mulib_termscript_scan_ (r, &r->field, "expected record field name"))
      return SIZE_MAX;
    length = r->pos - offset;
    if (!mulib_termscript_skip_ (r))
      return SIZE_MAX;
    if (mulib_termscript_at_ (r) != ':')
      {
        mulib_termscript_parse_fail_ (r->error, r->pos, "expected ':' after field");
        return SIZE_MAX;
      }
    ++r->pos;
    if (!mulib_termscript_skip_ (r))
      return SIZE_MAX;
    field = mulib_termscript_add_ (r, MULIB_TERMSCRIPT_FIELD, offset, length);
    if (field == SIZE_MAX)
      return SIZE_MAX;
    value = mulib_termscript_value_ (r, depth);
    if (value == SIZE_MAX)
      return SIZE_MAX;
    r->program.nodes[field].first = value;
    return field;
  }

  static void
  mulib_termscript_link_ (mulib_termscript_reader_ *r, size_t parent,
                          size_t *tail, size_t child)
  {
    if (*tail != SIZE_MAX)
      r->program.nodes[*tail].next = child;
    else if (parent != SIZE_MAX)
      r->program.nodes[parent].first = child;
    else
      r->program.root = child;
    *tail = child;
  }

  static size_t
  mulib_termscript_container_ (mulib_termscript_reader_ *r,
                                mulib_termscript_kind_t kind,
                                unsigned char close, size_t depth)
  {
    size_t offset = r->pos, node, tail = SIZE_MAX;
    int record = kind == MULIB_TERMSCRIPT_RECORD;
    if (depth == MULIB_TERMSCRIPT_DEPTH_LIMIT_)
      {
        mulib_termscript_parse_fail_ (r->error, offset, "Termscript nesting limit");
        return SIZE_MAX;
      }
    node = mulib_termscript_add_ (r, kind, offset, 0);
    if (node == SIZE_MAX)
      return SIZE_MAX;
    ++r->pos;
    if (!mulib_termscript_skip_ (r))
      return SIZE_MAX;
    while (mulib_termscript_at_ (r) != close)
      {
        size_t child;
        if (!mulib_termscript_at_ (r))
          {
            mulib_termscript_parse_fail_ (r->error, r->pos, "unclosed container");
            return SIZE_MAX;
          }
        child = record ? mulib_termscript_record_field_ (r, depth + 1)
                       : mulib_termscript_value_ (r, depth + 1);
        if (child == SIZE_MAX)
          return SIZE_MAX;
        mulib_termscript_link_ (r, node, &tail, child);
        if (!mulib_termscript_skip_ (r))
          return SIZE_MAX;
        if (kind != MULIB_TERMSCRIPT_BLOCK && mulib_termscript_at_ (r) == ',')
          {
            ++r->pos;
            if (!mulib_termscript_skip_ (r))
              return SIZE_MAX;
          }
        else if (record && mulib_termscript_at_ (r)
                 && mulib_termscript_at_ (r) != close)
          {
            mulib_termscript_parse_fail_ (r->error, r->pos,
                                           "expected ',' or '>' after field");
            return SIZE_MAX;
          }
      }
    r->program.nodes[node].length = ++r->pos - offset;
    return node;
  }

  static size_t
  mulib_termscript_value_ (mulib_termscript_reader_ *r, size_t depth)
  {
    mulib_termscript_kind_t kind = MULIB_TERMSCRIPT_WORD;
    size_t start = r->pos, i;
    unsigned char c = mulib_termscript_at_ (r);
    int64_t integer = 0;
    int numeric;
    switch (c)
      {
      case '{': return mulib_termscript_container_ (r, MULIB_TERMSCRIPT_BLOCK, '}', depth);
      case '(': return mulib_termscript_container_ (r, MULIB_TERMSCRIPT_TUPLE, ')', depth);
      case '[':
        if (mulib_termscript_prefix_ (r, "[[", 2))
          return mulib_termscript_escape_ (r);
        return mulib_termscript_container_ (r, MULIB_TERMSCRIPT_ARRAY, ']', depth);
      case '<': return mulib_termscript_container_ (r, MULIB_TERMSCRIPT_RECORD, '>', depth);
      case '"':
      case '`':
        kind = c == '"' ? MULIB_TERMSCRIPT_STRING : MULIB_TERMSCRIPT_COMMAND;
        start = ++r->pos;
        if (!mulib_termscript_scan_ (r, &r->quotes[c == '`'].root,
                                     "unterminated quoted text")
            || !mulib_termscript_validate_quote_ (r, start, r->pos - 1))
          return SIZE_MAX;
        return mulib_termscript_add_ (r, kind, start, r->pos - start - 1);
      case '#':
        if (mulib_termscript_prefix_ (r, "#\\", 2))
          return mulib_termscript_character_ (r);
        kind = MULIB_TERMSCRIPT_SYMBOL;
        break;
      case '@': kind = MULIB_TERMSCRIPT_ARRAY_SYMBOL; break;
      case '$': kind = MULIB_TERMSCRIPT_PARAMETER; break;
      case '!': kind = MULIB_TERMSCRIPT_KEY; break;
      case '%':
        if (mulib_termscript_prefix_ (r, "%module:", 8)
            || mulib_termscript_prefix_ (r, "%if-term:", 9)
            || mulib_termscript_prefix_ (r, "%else:", 6)
            || mulib_termscript_prefix_ (r, "%end:", 5))
          {
            if (!mulib_termscript_scan_ (r, &r->line, "invalid directive"))
              return SIZE_MAX;
            return mulib_termscript_add_ (r, MULIB_TERMSCRIPT_DIRECTIVE, start,
                                          r->pos - start);
          }
        kind = MULIB_TERMSCRIPT_RECORD_SYMBOL;
        break;
      case '}': case ')': case ']': case '>':
        mulib_termscript_parse_fail_ (r->error, start, "unexpected closing delimiter");
        return SIZE_MAX;
      case ',':
        mulib_termscript_parse_fail_ (r->error, start, "unexpected ','");
        return SIZE_MAX;
      default: break;
      }
    if (kind != MULIB_TERMSCRIPT_WORD)
      start = ++r->pos;
    if (!mulib_termscript_scan_ (r, &r->atom,
                                 kind == MULIB_TERMSCRIPT_WORD
                                     ? "expected Termscript value"
                                     : "expected name after sigil"))
      return SIZE_MAX;
    if (kind == MULIB_TERMSCRIPT_WORD)
      {
        numeric = mulib_termscript_integer_ (r->program.source + start,
                                             r->pos - start, &integer);
        if (numeric < 0)
          {
            mulib_termscript_parse_fail_ (r->error, start, "integer out of range");
            return SIZE_MAX;
          }
        if (numeric)
          kind = MULIB_TERMSCRIPT_INTEGER;
      }
    i = mulib_termscript_add_ (r, kind, start, r->pos - start);
    if (i != SIZE_MAX)
      r->program.nodes[i].integer = integer;
    return i;
  }

  void
  mulib_termscript_program_free (mulib_termscript_program_t *program)
  {
    if (program)
      {
        free (program->source);
        free (program->nodes);
        memset (program, 0, sizeof (*program));
      }
  }

  int
  mulib_termscript_parse (const char *source, size_t length,
                          mulib_termscript_program_t *out,
                          mulib_termscript_error_t *error)
  {
    mulib_termscript_reader_ r;
    size_t i = 0, tail = SIZE_MAX;
    if (error)
      {
        error->offset = 0;
        error->message = NULL;
      }
    if (!out || (!source && length))
      return mulib_termscript_parse_fail_ (error, 0, "invalid Termscript argument");
    if (length > MULIB_TERMSCRIPT_SOURCE_LIMIT_)
      return mulib_termscript_parse_fail_ (error, MULIB_TERMSCRIPT_SOURCE_LIMIT_,
                                           "Termscript source limit");
    while (i < length)
      {
        unsigned char c = (unsigned char)source[i];
        if (!c)
          return mulib_termscript_parse_fail_ (error, i, "NUL in Termscript source");
        if (c < 0x80)
          ++i;
        else
          {
            uint32_t scalar;
            size_t used;
            if (!mulib_utf8_decode ((const unsigned char *)source + i,
                                    length - i, &scalar, &used))
              return mulib_termscript_parse_fail_ (error, i, "invalid UTF-8");
            i += used;
          }
      }
    memset (&r, 0, sizeof r);
    r.error = error;
    r.program.root = SIZE_MAX;
    r.program.length = length;
    r.program.source = (char *)malloc (length + 1);
    if (!r.program.source)
      return mulib_termscript_parse_fail_ (error, 0, "out of memory");
    if (length)
      memcpy (r.program.source, source, length);
    r.program.source[length] = 0;
    mulib_termscript_rules_init_ (&r);
    for (;;)
      {
        size_t child;
        if (!mulib_termscript_skip_ (&r))
          break;
        if (r.pos == length)
          {
            *out = r.program;
            return 1;
          }
        child = mulib_termscript_value_ (&r, 0);
        if (child == SIZE_MAX)
          break;
        mulib_termscript_link_ (&r, SIZE_MAX, &tail, child);
      }
    mulib_termscript_program_free (&r.program);
    return 0;
  }
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_TERMSCRIPT_AST */
