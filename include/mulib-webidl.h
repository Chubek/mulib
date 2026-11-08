/** @file mulib-webidl.h
 * @brief WebIDL source compiler: S-expression CST/AST, CodeYAML and C99.
 * Define MULIB_IMPL_WEBIDL, DBSCHEMA, BUFFER and JSON dependencies.
 * See manual/mulib-webidl.md for grammar, output schemas and C ABI mapping.
 * The older flat-schema dictionary generator remains available. */
#ifndef MULIB_WEBIDL_H
#define MULIB_WEBIDL_H
#include "mulib-buffer.h"
#include "mulib-dbschema.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Atomic text append, no trailing NUL. */
  int mulib_webidl_generate (const mulib_dbschema_t *schema,
                             mulib_buffer_t *out);
  /** Output language. CodeYAML is a YAML representation of the semantic AST.
   */
  typedef enum mulib_webidl_format
  {
    MULIB_WEBIDL_SEXPR = 0,
    MULIB_WEBIDL_CODEYAML = 1,
    MULIB_WEBIDL_C99 = 2
  } mulib_webidl_format_t;
  typedef enum mulib_webidl_tree
  {
    MULIB_WEBIDL_AST = 0,
    MULIB_WEBIDL_CST = 1
  } mulib_webidl_tree_t;
  /** Zero initialization selects S-expression AST. CST applies only to SEXPR.
   */
  typedef struct mulib_webidl_options
  {
    mulib_webidl_format_t format;
    mulib_webidl_tree_t tree;
  } mulib_webidl_options_t;
  /** Borrowed static message, byte offset and one-based byte line/column.
   * Cleared on success. May be NULL. */
  typedef struct mulib_webidl_error
  {
    const char *message;
    size_t offset, line, column;
  } mulib_webidl_error_t;
  /** Compile a byte span (NULL is allowed only for zero length).
   * Appends atomically without a NUL; returns 1 on success, 0 on any failure.
   * Input may alias out. NULL options selects S-expression AST.
   * No filesystem access, retained input, global state or generated bindings.
   * Limits: 16 MiB input, 64 MiB output, 262144 tokens, 65536 AST nodes,
   * nesting depth 64; see the manual for work/cache limits. */
  int mulib_webidl_compile (const char *source, size_t length,
                            const mulib_webidl_options_t *options,
                            mulib_buffer_t *out, mulib_webidl_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBIDL) && !defined(MULIB_WEBIDL_IMPLEMENTED)
#define MULIB_WEBIDL_IMPLEMENTED
static int
mulib_webidl_schema_text_ (mulib_buffer_t *b, const char *s)
{
  return mulib_buffer_append (b, s, strlen (s));
}
static int
mulib_webidl_schema_name_ (const char *s)
{
  return s[0] != '_' && strcmp (s, "constructor") && strcmp (s, "toString");
}
int
mulib_webidl_generate (const mulib_dbschema_t *s, mulib_buffer_t *out)
{
  static const char *const types[]
      = { NULL, "boolean", "double", "DOMString", "sequence<any>", "object" };
  mulib_buffer_t b = { 0 };
  int ok = 0;
  if (!out || !mulib_dbschema_validate (s)
      || !mulib_webidl_schema_name_ (s->name))
    return 0;
  if (!mulib_webidl_schema_text_ (&b, "dictionary _")
      || !mulib_webidl_schema_text_ (&b, s->name)
      || !mulib_webidl_schema_text_ (&b, " {\n"))
    goto done;
  for (size_t i = 0; i < s->count; ++i)
    {
      const mulib_dbschema_field_t *f = s->fields + i;
      if (!types[f->type] || !mulib_webidl_schema_name_ (f->name)
          || !mulib_webidl_schema_text_ (&b, "  ")
          || (f->required && !mulib_webidl_schema_text_ (&b, "required "))
          || !mulib_webidl_schema_text_ (&b, types[f->type])
          || (f->nullable && !mulib_webidl_schema_text_ (&b, "?"))
          || !mulib_webidl_schema_text_ (&b, " _")
          || !mulib_webidl_schema_text_ (&b, f->name)
          || !mulib_webidl_schema_text_ (&b, ";\n"))
        goto done;
    }
  ok = mulib_webidl_schema_text_ (&b, "};\n")
       && mulib_buffer_append (out, b.data, b.length);
done:
  mulib_buffer_free (&b);
  return ok;
}

/* Bounded compiler staging buffers; the legacy schema API is unchanged. */
static int
mulib_webidl_bytes_ (mulib_buffer_t *b, const void *s, size_t n)
{
  return b->length <= 67108864 && n <= 67108864 - b->length
         && mulib_buffer_append (b, s, n);
}
static int
mulib_webidl_text_ (mulib_buffer_t *b, const char *s)
{
  return mulib_webidl_bytes_ (b, s, strlen (s));
}
/* The compiler owns all nodes; slices always refer to the caller's input. */
typedef struct mulib_webidl_slice_
{
  const char *p;
  size_t n;
} mulib_webidl_slice_;
typedef struct mulib_webidl_token_
{
  const char *kind;
  size_t start, end;
} mulib_webidl_token_;
typedef struct mulib_webidl_node_
{
  const char *kind;
  mulib_webidl_slice_ value;
  size_t offset;
  struct mulib_webidl_node_ *child, *tail, *next;
} mulib_webidl_node_;
typedef struct mulib_webidl_parser_
{
  const char *source;
  size_t length, pos, count, nodes, depth, checks;
  mulib_buffer_t tokens;
  mulib_webidl_node_ *root;
  const char *message;
  size_t error;
} mulib_webidl_parser_;

static int
mulib_webidl_fail_ (mulib_webidl_parser_ *p, size_t offset,
                    const char *message)
{
  if (!p->message)
    {
      p->message = message;
      p->error = offset;
    }
  return 0;
}
static int
mulib_webidl_alpha_ (unsigned char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int
mulib_webidl_digit_ (unsigned char c)
{
  return c >= '0' && c <= '9';
}
static int
mulib_webidl_lex_ (mulib_webidl_parser_ *p)
{
  size_t i = 0;
  const char *s = p->source;
  while (i < p->length)
    {
      mulib_webidl_token_ t;
      unsigned char c = (unsigned char)s[i];
      t.start = i;
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
          t.kind = "whitespace";
          do
            {
              ++i;
            }
          while (i < p->length
                 && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n'
                     || s[i] == '\r'));
        }
      else if (c == '/' && i + 1 < p->length && s[i + 1] == '/')
        {
          t.kind = "comment";
          i += 2;
          while (i < p->length && s[i] != '\n' && s[i] != '\r')
            {
              if (!s[i])
                return mulib_webidl_fail_ (p, i, "NUL in source");
              ++i;
            }
        }
      else if (c == '/' && i + 1 < p->length && s[i + 1] == '*')
        {
          t.kind = "comment";
          i += 2;
          while (i + 1 < p->length && !(s[i] == '*' && s[i + 1] == '/'))
            {
              if (!s[i])
                return mulib_webidl_fail_ (p, i, "NUL in source");
              ++i;
            }
          if (i + 1 >= p->length)
            return mulib_webidl_fail_ (p, t.start, "unterminated comment");
          i += 2;
        }
      else if ((mulib_webidl_alpha_ (c) && c != '_')
               || ((c == '_' || c == '-') && i + 1 < p->length
                   && mulib_webidl_alpha_ ((unsigned char)s[i + 1])
                   && s[i + 1] != '_'))
        {
          t.kind = "identifier";
          while (++i < p->length
                 && (mulib_webidl_alpha_ ((unsigned char)s[i])
                     || mulib_webidl_digit_ ((unsigned char)s[i])
                     || s[i] == '-'))
            ;
        }
      else if (c == '"')
        {
          t.kind = "string";
          ++i;
          while (i < p->length && s[i] != '"')
            {
              if (!s[i])
                return mulib_webidl_fail_ (p, i, "invalid string literal");
              ++i;
            }
          if (i == p->length)
            return mulib_webidl_fail_ (p, t.start, "unterminated string");
          ++i;
        }
      else if (mulib_webidl_digit_ (c)
               || (c == '.' && i + 1 < p->length
                   && mulib_webidl_digit_ ((unsigned char)s[i + 1]))
               || (c == '-' && i + 1 < p->length
                   && (mulib_webidl_digit_ ((unsigned char)s[i + 1])
                       || s[i + 1] == '.')))
        {
          int fraction = 0;
          t.kind = "number";
          if (s[i] == '-')
            ++i;
          if (i + 1 < p->length && s[i] == '0'
              && (s[i + 1] == 'x' || s[i + 1] == 'X'))
            {
              size_t start;
              i += 2;
              start = i;
              while (i < p->length
                     && (mulib_webidl_digit_ ((unsigned char)s[i])
                         || (s[i] >= 'a' && s[i] <= 'f')
                         || (s[i] >= 'A' && s[i] <= 'F')))
                ++i;
              if (i == start)
                return mulib_webidl_fail_ (p, i,
                                           "expected hexadecimal digits");
            }
          else
            {
              size_t digits = 0;
              while (i < p->length
                     && mulib_webidl_digit_ ((unsigned char)s[i]))
                {
                  ++i;
                  ++digits;
                }
              if (i < p->length && s[i] == '.')
                {
                  fraction = 1;
                  ++i;
                  while (i < p->length
                         && mulib_webidl_digit_ ((unsigned char)s[i]))
                    {
                      ++i;
                      ++digits;
                    }
                }
              if (!digits)
                return mulib_webidl_fail_ (p, i, "expected number");
              if (i < p->length && (s[i] == 'e' || s[i] == 'E'))
                {
                  size_t start;
                  fraction = 1;
                  ++i;
                  if (i < p->length && (s[i] == '+' || s[i] == '-'))
                    ++i;
                  start = i;
                  while (i < p->length
                         && mulib_webidl_digit_ ((unsigned char)s[i]))
                    ++i;
                  if (i == start)
                    return mulib_webidl_fail_ (p, i,
                                               "expected exponent digits");
                }
              if (!fraction)
                {
                  size_t j = t.start + (s[t.start] == '-');
                  if (i - j > 1 && s[j] == '0')
                    for (++j; j < i; ++j)
                      if (s[j] > '7')
                        return mulib_webidl_fail_ (p, j,
                                                   "invalid octal integer");
                }
            }
        }
      else if (c == '.' && i + 2 < p->length && s[i + 1] == '.'
               && s[i + 2] == '.')
        {
          t.kind = "punctuation";
          i += 3;
        }
      else if (c && strchr ("{}()[]<>,;:=?*-.", c))
        {
          t.kind = "punctuation";
          ++i;
        }
      else if (c)
        {
          uint32_t scalar;
          size_t used;
          t.kind = "other";
          if (!mulib_utf8_decode ((const unsigned char *)s + i, p->length - i,
                                  &scalar, &used))
            return mulib_webidl_fail_ (p, i, "invalid UTF-8 source");
          i += used;
        }
      else
        return mulib_webidl_fail_ (p, i, "NUL in source");
      t.end = i;
      if (p->count == 262144)
        return mulib_webidl_fail_ (p, t.start, "token limit exceeded");
      if (!mulib_buffer_append (&p->tokens, &t, sizeof t))
        return mulib_webidl_fail_ (p, t.start, "allocation failed");
      ++p->count;
    }
  return 1;
}
static mulib_webidl_token_ *
mulib_webidl_peek_ (mulib_webidl_parser_ *p)
{
  mulib_webidl_token_ *ts = (mulib_webidl_token_ *)p->tokens.data;
  while (p->pos < p->count
         && (!strcmp (ts[p->pos].kind, "whitespace")
             || !strcmp (ts[p->pos].kind, "comment")))
    ++p->pos;
  return p->pos < p->count ? ts + p->pos : NULL;
}
static size_t
mulib_webidl_offset_ (mulib_webidl_parser_ *p)
{
  mulib_webidl_token_ *t = mulib_webidl_peek_ (p);
  return t ? t->start : p->length;
}
static int
mulib_webidl_is_ (mulib_webidl_parser_ *p, const char *s)
{
  mulib_webidl_token_ *t = mulib_webidl_peek_ (p);
  return t && t->end - t->start == strlen (s)
         && !memcmp (p->source + t->start, s, t->end - t->start);
}
static int
mulib_webidl_take_ (mulib_webidl_parser_ *p, const char *s)
{
  if (!mulib_webidl_is_ (p, s))
    return 0;
  ++p->pos;
  return 1;
}
static int
mulib_webidl_expect_ (mulib_webidl_parser_ *p, const char *s)
{
  return mulib_webidl_take_ (p, s)
         || mulib_webidl_fail_ (p, mulib_webidl_offset_ (p), s);
}
static mulib_webidl_node_ *
mulib_webidl_node_new_ (mulib_webidl_parser_ *p, mulib_webidl_node_ *parent,
                        const char *kind, const char *s, size_t n, size_t off)
{
  mulib_webidl_node_ *v;
  if (p->message)
    return NULL;
  if (p->nodes == 65536)
    {
      mulib_webidl_fail_ (p, off, "AST node limit exceeded");
      return NULL;
    }
  v = (mulib_webidl_node_ *)calloc (1, sizeof *v);
  if (!v)
    {
      mulib_webidl_fail_ (p, off, "allocation failed");
      return NULL;
    }
  ++p->nodes;
  v->kind = kind;
  v->value.p = s;
  v->value.n = n;
  v->offset = off;
  if (parent)
    {
      if (parent->tail)
        parent->tail->next = v;
      else
        parent->child = v;
      parent->tail = v;
    }
  return v;
}
static mulib_webidl_node_ *
mulib_webidl_add_ (mulib_webidl_parser_ *p, mulib_webidl_node_ *parent,
                   const char *kind)
{
  return mulib_webidl_node_new_ (p, parent, kind, NULL, 0,
                                 mulib_webidl_offset_ (p));
}
static void
mulib_webidl_nodes_free_ (mulib_webidl_node_ *v)
{
  while (v)
    {
      mulib_webidl_node_ *next = v->next;
      mulib_webidl_nodes_free_ (v->child);
      free (v);
      v = next;
    }
}
static int
mulib_webidl_name_ (mulib_webidl_parser_ *p, mulib_webidl_node_ *parent,
                    const char *kind)
{
  static const char *const keywords[] = { "attribute",
                                          "callback",
                                          "const",
                                          "constructor",
                                          "deleter",
                                          "dictionary",
                                          "enum",
                                          "getter",
                                          "includes",
                                          "inherit",
                                          "interface",
                                          "iterable",
                                          "maplike",
                                          "mixin",
                                          "namespace",
                                          "partial",
                                          "readonly",
                                          "required",
                                          "setlike",
                                          "setter",
                                          "static",
                                          "stringifier",
                                          "typedef",
                                          "unrestricted",
                                          "any",
                                          "async_iterable",
                                          "async_sequence",
                                          "boolean",
                                          "byte",
                                          "octet",
                                          "short",
                                          "long",
                                          "unsigned",
                                          "float",
                                          "double",
                                          "bigint",
                                          "symbol",
                                          "object",
                                          "undefined",
                                          "void",
                                          "DOMString",
                                          "ByteString",
                                          "USVString",
                                          "sequence",
                                          "record",
                                          "Promise",
                                          "FrozenArray",
                                          "ObservableArray",
                                          "optional",
                                          "or",
                                          "true",
                                          "false",
                                          "null",
                                          "Infinity",
                                          "NaN",
                                          "-Infinity",
                                          "ArrayBuffer",
                                          "SharedArrayBuffer",
                                          "DataView",
                                          "Int8Array",
                                          "Int16Array",
                                          "Int32Array",
                                          "Uint8Array",
                                          "Uint16Array",
                                          "Uint32Array",
                                          "Uint8ClampedArray",
                                          "BigInt64Array",
                                          "BigUint64Array",
                                          "Float16Array",
                                          "Float32Array",
                                          "Float64Array" };
  mulib_webidl_token_ *t = mulib_webidl_peek_ (p);
  size_t start, i;
  if (!t || strcmp (t->kind, "identifier"))
    return mulib_webidl_fail_ (p, mulib_webidl_offset_ (p),
                               "expected identifier");
  start = t->start;
  for (i = 0; i < sizeof keywords / sizeof *keywords; ++i)
    if (mulib_webidl_is_ (p, keywords[i])
        && !(i < 24 && !strcmp (parent->kind, "argument"))
        && !(i == 17 && !strcmp (parent->kind, "attribute"))
        && !(i == 8 && !strcmp (parent->kind, "operation")))
      return mulib_webidl_fail_ (p, start,
                                 "keyword requires identifier escape");
  /* WebIDL's leading underscore escapes an identifier, it is not its name. */
  if (p->source[start] == '_' && start + 1 < t->end)
    ++start;
  if (strcmp (parent->kind, "argument")
      && ((t->end - start == 11
           && !memcmp (p->source + start, "constructor", 11))
          || (t->end - start == 8
              && !memcmp (p->source + start, "toString", 8))))
    return mulib_webidl_fail_ (p, start, "reserved identifier");
  ++p->pos;
  return mulib_webidl_node_new_ (p, parent, kind, p->source + start,
                                 t->end - start, t->start)
         != NULL;
}
/* Extended attributes retain a balanced token tree, including unknown vendor
 * attributes. No attribute semantics are inferred by the declaration backend.
 */
static int
mulib_webidl_attribute_tokens_ (mulib_webidl_parser_ *p,
                                mulib_webidl_node_ *parent, const char *close)
{
  if (++p->depth > 64)
    {
      --p->depth;
      return mulib_webidl_fail_ (p, mulib_webidl_offset_ (p),
                                 "nesting limit exceeded");
    }
  while (!p->message && !mulib_webidl_is_ (p, close))
    {
      mulib_webidl_token_ *t = mulib_webidl_peek_ (p);
      const char *end = NULL;
      mulib_webidl_node_ *v;
      if (!t)
        {
          mulib_webidl_fail_ (p, p->length, "unterminated extended attribute");
          break;
        }
      if (mulib_webidl_is_ (p, "("))
        end = ")";
      else if (mulib_webidl_is_ (p, "["))
        end = "]";
      else if (mulib_webidl_is_ (p, "{"))
        end = "}";
      else if (mulib_webidl_is_ (p, ")") || mulib_webidl_is_ (p, "]")
               || mulib_webidl_is_ (p, "}"))
        {
          mulib_webidl_fail_ (p, t->start, "mismatched attribute delimiter");
          break;
        }
      ++p->pos;
      v = mulib_webidl_node_new_ (p, parent, end ? "group" : t->kind,
                                  p->source + t->start, t->end - t->start,
                                  t->start);
      if (!v || (end && !mulib_webidl_attribute_tokens_ (p, v, end)))
        break;
    }
  --p->depth;
  return !p->message && mulib_webidl_expect_ (p, close);
}
static int
mulib_webidl_attributes_ (mulib_webidl_parser_ *p, mulib_webidl_node_ *parent)
{
  mulib_webidl_node_ *v;
  if (!mulib_webidl_take_ (p, "["))
    return 1;
  v = mulib_webidl_add_ (p, parent, "extended-attributes");
  if (!v)
    return 0;
  if (mulib_webidl_is_ (p, "]"))
    return mulib_webidl_fail_ (p, mulib_webidl_offset_ (p),
                               "empty extended attributes");
  return mulib_webidl_attribute_tokens_ (p, v, "]");
}
static int mulib_webidl_type_ (mulib_webidl_parser_ *, mulib_webidl_node_ *,
                               int);
static int
mulib_webidl_type_ (mulib_webidl_parser_ *p, mulib_webidl_node_ *parent,
                    int allow_void)
{
  mulib_webidl_node_ *v = mulib_webidl_add_ (p, parent, "type");
  size_t start, end;
  int ok = 0, named = 0;
  if (!v)
    return 0;
  if (++p->depth > 64)
    {
      --p->depth;
      return mulib_webidl_fail_ (p, v->offset, "nesting limit exceeded");
    }
  if (!mulib_webidl_attributes_ (p, v))
    goto done;
  start = mulib_webidl_offset_ (p);
  if (mulib_webidl_take_ (p, "("))
    {
      v->value.p = "union";
      v->value.n = 5;
      if (!mulib_webidl_type_ (p, v, 0) || !mulib_webidl_expect_ (p, "or"))
        goto done;
      do
        {
          if (!mulib_webidl_type_ (p, v, 0))
            goto done;
        }
      while (mulib_webidl_take_ (p, "or"));
      if (!mulib_webidl_expect_ (p, ")"))
        goto done;
    }
  else
    {
      mulib_webidl_token_ *t = mulib_webidl_peek_ (p);
      const char *generic = NULL;
      if (!t || strcmp (t->kind, "identifier"))
        {
          mulib_webidl_fail_ (p, start, "expected type");
          goto done;
        }
      if (mulib_webidl_is_ (p, "sequence"))
        generic = "sequence";
      else if (mulib_webidl_is_ (p, "async_sequence"))
        generic = "async_sequence";
      else if (mulib_webidl_is_ (p, "FrozenArray"))
        generic = "FrozenArray";
      else if (mulib_webidl_is_ (p, "ObservableArray"))
        generic = "ObservableArray";
      else if (mulib_webidl_is_ (p, "Promise"))
        generic = "Promise";
      else if (mulib_webidl_is_ (p, "record"))
        generic = "record";
      if (generic)
        {
          ++p->pos;
          v->value.p = generic;
          v->value.n = strlen (generic);
          if (!mulib_webidl_expect_ (p, "<"))
            goto done;
          if (!strcmp (generic, "record"))
            {
              if (!(mulib_webidl_is_ (p, "DOMString")
                    || mulib_webidl_is_ (p, "ByteString")
                    || mulib_webidl_is_ (p, "USVString")))
                {
                  mulib_webidl_fail_ (p, mulib_webidl_offset_ (p),
                                      "record key must be a string type");
                  goto done;
                }
              if (!mulib_webidl_type_ (p, v, 0)
                  || !mulib_webidl_expect_ (p, ","))
                goto done;
            }
          if (!mulib_webidl_type_ (p, v, !strcmp (generic, "Promise"))
              || !mulib_webidl_expect_ (p, ">"))
            goto done;
        }
      else
        {
          int multi = 0;
          size_t first_token = p->pos;
          if (mulib_webidl_take_ (p, "unsigned"))
            {
              multi = 1;
              if (!mulib_webidl_take_ (p, "short"))
                {
                  if (!mulib_webidl_expect_ (p, "long"))
                    goto done;
                  (void)mulib_webidl_take_ (p, "long");
                }
            }
          else if (mulib_webidl_take_ (p, "unrestricted"))
            {
              multi = 1;
              if (!mulib_webidl_take_ (p, "float")
                  && !mulib_webidl_expect_ (p, "double"))
                goto done;
            }
          else if (mulib_webidl_take_ (p, "long"))
            {
              multi = 1;
              (void)mulib_webidl_take_ (p, "long");
            }
          else
            {
              static const char *const reserved[] = {
                "attribute", "callback",       "const",     "constructor",
                "deleter",   "dictionary",     "enum",      "getter",
                "includes",  "inherit",        "interface", "iterable",
                "maplike",   "mixin",          "namespace", "optional",
                "or",        "partial",        "readonly",  "required",
                "setlike",   "setter",         "static",    "stringifier",
                "typedef",   "async",          "true",      "false",
                "null",      "async_iterable", "Infinity",  "NaN",
                "-Infinity"
              };
              size_t k;
              for (k = 0; k < sizeof reserved / sizeof *reserved; ++k)
                if (mulib_webidl_is_ (p, reserved[k]))
                  {
                    mulib_webidl_fail_ (p, start,
                                        "expected type, not keyword");
                    goto done;
                  }
              if (!allow_void && mulib_webidl_is_ (p, "void"))
                {
                  mulib_webidl_fail_ (p, start,
                                      "legacy void is only a return type");
                  goto done;
                }
              ++p->pos;
            }
          /* Failed optional lookahead can skip trivia. Find the last consumed
           * significant token rather than treating that trivia as a type word.
           */
          size_t last_token = p->pos;
          mulib_webidl_token_ *ts = (mulib_webidl_token_ *)p->tokens.data;
          while (last_token > first_token
                 && (!strcmp (ts[last_token - 1].kind, "whitespace")
                     || !strcmp (ts[last_token - 1].kind, "comment")))
            --last_token;
          end = ts[last_token - 1].end;
          if (multi)
            {
              /* Canonicalize multiword builtins independently of trivia. */
              const char *last = p->source + ts[last_token - 1].start;
              size_t ln = ts[last_token - 1].end - ts[last_token - 1].start;
              int uns = t->end - t->start == 8
                        && !memcmp (p->source + start, "unsigned", 8);
              int unr = t->end - t->start == 12
                        && !memcmp (p->source + start, "unrestricted", 12);
              const char *canonical;
              if (unr)
                canonical = ln == 5 && !memcmp (last, "float", 5)
                                ? "unrestricted float"
                                : "unrestricted double";
              else if (ln == 5)
                canonical = "unsigned short";
              else
                {
                  size_t longs = 0, j;
                  for (j = first_token; j < p->pos; ++j)
                    if (ts[j].start >= start && ts[j].end - ts[j].start == 4
                        && !memcmp (p->source + ts[j].start, "long", 4))
                      ++longs;
                  canonical = uns ? (longs == 2 ? "unsigned long long"
                                                : "unsigned long")
                                  : (longs == 2 ? "long long" : "long");
                }
              v->value.p = canonical;
              v->value.n = strlen (canonical);
            }
          else
            {
              if (p->source[start] == '_' && start + 1 < end)
                {
                  ++start;
                  named = 1;
                  if (!mulib_webidl_add_ (p, v, "identifier"))
                    goto done;
                }
              v->value.p = p->source + start;
              v->value.n = end - start;
            }
        }
    }
  if (mulib_webidl_take_ (p, "?"))
    {
      if (!named
          && ((v->value.n == 3 && !memcmp (v->value.p, "any", 3))
              || (v->value.n == 7 && !memcmp (v->value.p, "Promise", 7))
              || (v->value.n == 4 && !memcmp (v->value.p, "void", 4))))
        {
          mulib_webidl_fail_ (p, v->offset, "this type cannot be nullable");
          goto done;
        }
      if (!mulib_webidl_add_ (p, v, "nullable"))
        goto done;
    }
  ok = 1;
done:
  --p->depth;
  return ok;
}
static int
mulib_webidl_default_ (mulib_webidl_parser_ *p, mulib_webidl_node_ *parent)
{
  mulib_webidl_token_ *t = mulib_webidl_peek_ (p);
  size_t start = mulib_webidl_offset_ (p), end;
  if (!t)
    return mulib_webidl_fail_ (p, start, "expected literal");
  if (mulib_webidl_take_ (p, "["))
    {
      if (!mulib_webidl_expect_ (p, "]"))
        return 0;
    }
  else if (mulib_webidl_take_ (p, "{"))
    {
      if (!mulib_webidl_expect_ (p, "}"))
        return 0;
    }
  else if (mulib_webidl_take_ (p, "-"))
    {
      if (!mulib_webidl_expect_ (p, "Infinity"))
        return 0;
    }
  else if (!strcmp (t->kind, "string") || !strcmp (t->kind, "number")
           || mulib_webidl_is_ (p, "true") || mulib_webidl_is_ (p, "false")
           || mulib_webidl_is_ (p, "null") || mulib_webidl_is_ (p, "Infinity")
           || mulib_webidl_is_ (p, "NaN") || mulib_webidl_is_ (p, "undefined")
           || mulib_webidl_is_ (p, "-Infinity"))
    ++p->pos;
  else
    return mulib_webidl_fail_ (p, start, "expected literal");
  end = ((mulib_webidl_token_ *)p->tokens.data)[p->pos - 1].end;
  return mulib_webidl_node_new_ (p, parent, "default", p->source + start,
                                 end - start, start)
         != NULL;
}
static int
mulib_webidl_arguments_ (mulib_webidl_parser_ *p, mulib_webidl_node_ *parent)
{
  if (!mulib_webidl_expect_ (p, "("))
    return 0;
  if (mulib_webidl_take_ (p, ")"))
    return 1;
  do
    {
      int optional, variadic;
      mulib_webidl_node_ *v = mulib_webidl_add_ (p, parent, "argument");
      if (!v || !mulib_webidl_attributes_ (p, v))
        return 0;
      optional = mulib_webidl_take_ (p, "optional");
      if (optional && !mulib_webidl_add_ (p, v, "optional"))
        return 0;
      if (!mulib_webidl_type_ (p, v, 0))
        return 0;
      variadic = mulib_webidl_take_ (p, "...");
      if (variadic && !mulib_webidl_add_ (p, v, "variadic"))
        return 0;
      if (optional && variadic)
        return mulib_webidl_fail_ (p, v->offset,
                                   "invalid optional/variadic argument order");
      if (!mulib_webidl_name_ (p, v, "name"))
        return 0;
      if (mulib_webidl_take_ (p, "="))
        {
          if (!optional)
            return mulib_webidl_fail_ (
                p, v->offset, "only optional arguments have defaults");
          if (!mulib_webidl_default_ (p, v))
            return 0;
        }
      if (variadic)
        break;
    }
  while (mulib_webidl_take_ (p, ","));
  return mulib_webidl_expect_ (p, ")");
}
static int
mulib_webidl_member_ (mulib_webidl_parser_ *p, mulib_webidl_node_ *def)
{
  mulib_webidl_node_ *v = mulib_webidl_add_ (p, def, "member");
  int dictionary = !strcmp (def->kind, "dictionary");
  int readonly = 0;
  if (!v || !mulib_webidl_attributes_ (p, v))
    return 0;
  if (dictionary)
    {
      int required = mulib_webidl_take_ (p, "required");
      v->kind = "field";
      if (required && !mulib_webidl_add_ (p, v, "required"))
        return 0;
      if (!mulib_webidl_type_ (p, v, 0) || !mulib_webidl_name_ (p, v, "name"))
        return 0;
      if (mulib_webidl_take_ (p, "="))
        {
          if (required)
            return mulib_webidl_fail_ (p, v->offset,
                                       "required field cannot have a default");
          if (!mulib_webidl_default_ (p, v))
            return 0;
        }
    }
  else if (mulib_webidl_take_ (p, "const"))
    {
      v->kind = "const";
      if (!mulib_webidl_type_ (p, v, 0) || !mulib_webidl_name_ (p, v, "name")
          || !mulib_webidl_expect_ (p, "=") || !mulib_webidl_default_ (p, v))
        return 0;
    }
  else if (mulib_webidl_take_ (p, "constructor"))
    {
      v->kind = "constructor";
      if (!mulib_webidl_arguments_ (p, v))
        return 0;
    }
  else
    {
      static const char *const modifiers[]
          = { "static", "stringifier", "inherit",      "readonly", "getter",
              "setter", "deleter",     "legacycaller", "async" };
      size_t i;
      int special = 0;
      for (i = 0; i < sizeof modifiers / sizeof *modifiers; ++i)
        if (mulib_webidl_take_ (p, modifiers[i]))
          {
            if (!mulib_webidl_add_ (p, v, modifiers[i]))
              return 0;
            if (!strcmp (modifiers[i], "readonly"))
              readonly = 1;
            special = 1;
          }
      if (mulib_webidl_is_ (p, ";") && special)
        {
          mulib_webidl_node_ *flag;
          int stringifier = 0;
          for (flag = v->child; flag; flag = flag->next)
            if (!strcmp (flag->kind, "stringifier"))
              stringifier = 1;
            else if (strcmp (flag->kind, "extended-attributes"))
              return mulib_webidl_fail_ (p, v->offset,
                                         "invalid bare member modifier");
          if (!stringifier)
            return mulib_webidl_fail_ (p, v->offset, "expected member");
          v->kind = "stringifier";
        }
      else if (mulib_webidl_take_ (p, "attribute"))
        {
          v->kind = "attribute";
          if (!mulib_webidl_type_ (p, v, 0)
              || !mulib_webidl_name_ (p, v, "name"))
            return 0;
        }
      else if (mulib_webidl_is_ (p, "iterable")
               || mulib_webidl_is_ (p, "async_iterable")
               || mulib_webidl_is_ (p, "maplike")
               || mulib_webidl_is_ (p, "setlike"))
        {
          mulib_webidl_token_ *t = mulib_webidl_peek_ (p);
          int async = mulib_webidl_is_ (p, "async_iterable");
          v->kind = (mulib_webidl_is_ (p, "iterable") || async) ? "iterable"
                    : mulib_webidl_is_ (p, "maplike")           ? "maplike"
                                                                : "setlike";
          ++p->pos;
          if (async && !mulib_webidl_add_ (p, v, "async"))
            return 0;
          if (!mulib_webidl_expect_ (p, "<") || !mulib_webidl_type_ (p, v, 0))
            return 0;
          if (mulib_webidl_take_ (p, ","))
            {
              if (!strcmp (v->kind, "setlike"))
                return mulib_webidl_fail_ (p, t->start,
                                           "setlike has one type");
              if (!mulib_webidl_type_ (p, v, 0))
                return 0;
            }
          else if (!strcmp (v->kind, "maplike"))
            return mulib_webidl_fail_ (p, t->start,
                                       "maplike requires two types");
          if (!mulib_webidl_expect_ (p, ">"))
            return 0;
          if (mulib_webidl_is_ (p, "("))
            {
              mulib_webidl_node_ *flag;
              int is_async = 0;
              for (flag = v->child; flag; flag = flag->next)
                if (!strcmp (flag->kind, "async"))
                  is_async = 1;
              if (!is_async)
                return mulib_webidl_fail_ (
                    p, v->offset, "only async iterables have arguments");
              if (!mulib_webidl_arguments_ (p, v))
                return 0;
            }
        }
      else
        {
          v->kind = "operation";
          if (readonly)
            return mulib_webidl_fail_ (p, v->offset, "readonly operation");
          if (!mulib_webidl_type_ (p, v, 1))
            return 0;
          if (!mulib_webidl_is_ (p, "("))
            {
              if (!mulib_webidl_name_ (p, v, "name"))
                return 0;
            }
          else if (!special)
            return mulib_webidl_fail_ (p, v->offset,
                                       "ordinary operation requires a name");
          if (!mulib_webidl_arguments_ (p, v))
            return 0;
        }
    }
  return mulib_webidl_expect_ (p, ";");
}
static int
mulib_webidl_parse_ (mulib_webidl_parser_ *p)
{
  p->root = mulib_webidl_add_ (p, NULL, "webidl");
  if (!p->root)
    return 0;
  while (mulib_webidl_peek_ (p))
    {
      mulib_webidl_node_ *v = mulib_webidl_add_ (p, p->root, "definition");
      int partial, callback;
      if (!v || !mulib_webidl_attributes_ (p, v))
        return 0;
      partial = mulib_webidl_take_ (p, "partial");
      if (partial && !mulib_webidl_add_ (p, v, "partial"))
        return 0;
      callback = mulib_webidl_take_ (p, "callback");
      if (mulib_webidl_take_ (p, "interface"))
        {
          v->kind = callback ? "callback-interface" : "interface";
          if (mulib_webidl_take_ (p, "mixin"))
            {
              if (callback)
                return mulib_webidl_fail_ (p, v->offset,
                                           "callback mixin is invalid");
              v->kind = "mixin";
            }
          if (callback && partial)
            return mulib_webidl_fail_ (
                p, v->offset, "partial callback interface is invalid");
        }
      else if (callback)
        {
          v->kind = "callback";
          if (partial || !mulib_webidl_name_ (p, v, "name")
              || !mulib_webidl_expect_ (p, "=")
              || !mulib_webidl_type_ (p, v, 1)
              || !mulib_webidl_arguments_ (p, v)
              || !mulib_webidl_expect_ (p, ";"))
            return mulib_webidl_fail_ (p, v->offset, "invalid callback");
          continue;
        }
      else if (mulib_webidl_take_ (p, "dictionary"))
        v->kind = "dictionary";
      else if (mulib_webidl_take_ (p, "namespace"))
        v->kind = "namespace";
      else if (mulib_webidl_take_ (p, "enum"))
        {
          v->kind = "enum";
          if (partial || !mulib_webidl_name_ (p, v, "name")
              || !mulib_webidl_expect_ (p, "{"))
            return mulib_webidl_fail_ (p, v->offset, "invalid enum");
          do
            {
              mulib_webidl_token_ *t = mulib_webidl_peek_ (p);
              if (!t || strcmp (t->kind, "string"))
                return mulib_webidl_fail_ (p, mulib_webidl_offset_ (p),
                                           "expected enum string");
              ++p->pos;
              if (!mulib_webidl_node_new_ (p, v, "value",
                                           p->source + t->start + 1,
                                           t->end - t->start - 2, t->start))
                return 0;
            }
          while (mulib_webidl_take_ (p, ",") && !mulib_webidl_is_ (p, "}"));
          if (!mulib_webidl_expect_ (p, "}") || !mulib_webidl_expect_ (p, ";"))
            return 0;
          continue;
        }
      else if (mulib_webidl_take_ (p, "typedef"))
        {
          v->kind = "typedef";
          if (partial || !mulib_webidl_type_ (p, v, 0)
              || !mulib_webidl_name_ (p, v, "name")
              || !mulib_webidl_expect_ (p, ";"))
            return mulib_webidl_fail_ (p, v->offset, "invalid typedef");
          continue;
        }
      else
        {
          v->kind = "includes";
          if (partial || !mulib_webidl_name_ (p, v, "name")
              || !mulib_webidl_expect_ (p, "includes")
              || !mulib_webidl_name_ (p, v, "mixin")
              || !mulib_webidl_expect_ (p, ";"))
            return mulib_webidl_fail_ (p, v->offset,
                                       "expected WebIDL definition");
          continue;
        }
      if (!mulib_webidl_name_ (p, v, "name"))
        return 0;
      if (mulib_webidl_take_ (p, ":"))
        {
          if (partial || callback || !strcmp (v->kind, "namespace")
              || !strcmp (v->kind, "mixin"))
            return mulib_webidl_fail_ (p, v->offset,
                                       "inheritance is not allowed here");
          if (!mulib_webidl_name_ (p, v, "inheritance"))
            return 0;
        }
      if (!mulib_webidl_expect_ (p, "{"))
        return 0;
      while (!mulib_webidl_is_ (p, "}"))
        {
          if (!mulib_webidl_peek_ (p))
            return mulib_webidl_fail_ (p, p->length,
                                       "unterminated definition");
          if (!mulib_webidl_member_ (p, v))
            return 0;
        }
      if (!mulib_webidl_expect_ (p, "}") || !mulib_webidl_expect_ (p, ";"))
        return 0;
    }
  return !p->message;
}

static mulib_webidl_node_ *
mulib_webidl_child_ (const mulib_webidl_node_ *v, const char *kind)
{
  mulib_webidl_node_ *c;
  for (c = v->child; c; c = c->next)
    if (!strcmp (c->kind, kind))
      return c;
  return NULL;
}
static int
mulib_webidl_equal_ (mulib_webidl_slice_ a, mulib_webidl_slice_ b)
{
  return a.n == b.n && (!a.n || !memcmp (a.p, b.p, a.n));
}
static int
mulib_webidl_value_is_ (const mulib_webidl_node_ *v, const char *s)
{
  /* Names passed to the C backend and escaped type references denote named
   * declarations, even when their unescaped spelling matches a builtin. */
  return v && strcmp (v->kind, "name")
         && !(!strcmp (v->kind, "type")
              && mulib_webidl_child_ (v, "identifier"))
         && v->value.n == strlen (s) && !memcmp (v->value.p, s, v->value.n);
}
/* Grammar-level restrictions shared by every output format. Type linking is
 * deliberately deferred so a single fragment can refer to another file. */
static int
mulib_webidl_member_valid_ (mulib_webidl_parser_ *p,
                            const mulib_webidl_node_ *owner,
                            const mulib_webidl_node_ *v)
{
  const mulib_webidl_node_ *flag;
  int attr = !strcmp (v->kind, "attribute");
  int op = !strcmp (v->kind, "operation");
  int collection
      = !strcmp (v->kind, "maplike") || !strcmp (v->kind, "setlike");
  int iterable = !strcmp (v->kind, "iterable");
  int bare = !strcmp (v->kind, "stringifier");
  int ctor = !strcmp (v->kind, "constructor");
  int constant = !strcmp (v->kind, "const");
  int modifiers = 0;
  if (!(attr || op || collection || iterable || bare || ctor || constant))
    return 1;
  if ((ctor
       && (strcmp (owner->kind, "interface")
           || mulib_webidl_child_ (owner, "partial")))
      || (!strcmp (owner->kind, "callback-interface") && !(op || constant))
      || (!strcmp (owner->kind, "mixin") && !(op || attr || constant || bare))
      || (!strcmp (owner->kind, "namespace")
          && !(op || constant
               || (attr && mulib_webidl_child_ (v, "readonly")))))
    return mulib_webidl_fail_ (p, v->offset,
                               "member not allowed in this definition");
  for (flag = v->child; flag; flag = flag->next)
    {
      const char *k = flag->kind;
      int valid = 1, modifier = 1;
      if (!strcmp (k, "static"))
        valid = (attr || op) && !strcmp (owner->kind, "interface");
      else if (!strcmp (k, "readonly"))
        {
          valid = attr || collection;
          modifier = 0;
        }
      else if (!strcmp (k, "inherit"))
        valid = attr && !mulib_webidl_child_ (v, "readonly")
                && !strcmp (owner->kind, "interface");
      else if (!strcmp (k, "stringifier"))
        valid = (attr || bare) && strcmp (owner->kind, "namespace")
                && strcmp (owner->kind, "callback-interface");
      else if (!strcmp (k, "getter") || !strcmp (k, "setter")
               || !strcmp (k, "deleter") || !strcmp (k, "legacycaller"))
        valid = op && !strcmp (owner->kind, "interface");
      else if (!strcmp (k, "async"))
        valid = iterable;
      else
        modifier = 0;
      modifiers += modifier;
      if (!valid || modifiers > 1)
        return mulib_webidl_fail_ (p, flag->offset, "invalid member modifier");
    }
  if ((iterable && mulib_webidl_child_ (v, "readonly"))
      || ((collection || iterable) && mulib_webidl_child_ (v, "argument")
          && !mulib_webidl_child_ (v, "async")))
    return mulib_webidl_fail_ (p, v->offset, "invalid collection declaration");
  if (constant)
    {
      const mulib_webidl_node_ *t = mulib_webidl_child_ (v, "type");
      const mulib_webidl_node_ *d = mulib_webidl_child_ (v, "default");
      if (mulib_webidl_child_ (t, "type")
          || mulib_webidl_child_ (t, "nullable") || !d
          || !(
              mulib_webidl_value_is_ (d, "true")
              || mulib_webidl_value_is_ (d, "false")
              || mulib_webidl_value_is_ (d, "Infinity")
              || mulib_webidl_value_is_ (d, "NaN")
              || (d->value.n
                  && (d->value.p[0] == '-' || d->value.p[0] == '.'
                      || mulib_webidl_digit_ ((unsigned char)d->value.p[0])))))
        return mulib_webidl_fail_ (p, v->offset,
                                   "invalid constant type or literal");
    }
  return 1;
}
static int
mulib_webidl_union_has_ (const mulib_webidl_node_ *t, const char *name)
{
  const mulib_webidl_node_ *c;
  if (!t)
    return 0;
  if (mulib_webidl_value_is_ (t, name))
    return 1;
  if (mulib_webidl_value_is_ (t, "union"))
    for (c = t->child; c; c = c->next)
      if (!strcmp (c->kind, "type") && mulib_webidl_union_has_ (c, name))
        return 1;
  return 0;
}
static size_t
mulib_webidl_union_nulls_ (const mulib_webidl_node_ *t)
{
  const mulib_webidl_node_ *c;
  size_t count = mulib_webidl_child_ (t, "nullable") != NULL;
  if (mulib_webidl_value_is_ (t, "union"))
    for (c = t->child; c; c = c->next)
      if (!strcmp (c->kind, "type"))
        count += mulib_webidl_union_nulls_ (c);
  return count;
}
/* Local consistency checks; cross-file name resolution and WebIDL conversion
 * semantics are separate from parsing. The C backend requires closed types. */
static int
mulib_webidl_validate_nodes_ (mulib_webidl_parser_ *p,
                              const mulib_webidl_node_ *parent)
{
  const mulib_webidl_node_ *v, *other;
  for (v = parent->child; v; v = v->next)
    {
      mulib_webidl_node_ *name = mulib_webidl_child_ (v, "name");
      if (!mulib_webidl_member_valid_ (p, parent, v))
        return 0;
      if (!strcmp (v->kind, "type"))
        {
          if (mulib_webidl_value_is_ (v, "union")
              && (mulib_webidl_union_has_ (v, "any")
                  || mulib_webidl_union_has_ (v, "Promise")
                  || mulib_webidl_union_nulls_ (v) > 1))
            return mulib_webidl_fail_ (p, v->offset,
                                       "invalid union member type");
          if (mulib_webidl_value_is_ (v, "record")
              && mulib_webidl_child_ (mulib_webidl_child_ (v, "type"),
                                      "nullable"))
            return mulib_webidl_fail_ (p, v->offset,
                                       "record key cannot be nullable");
          if ((!strcmp (parent->kind, "argument")
               || !strcmp (parent->kind, "field"))
              && mulib_webidl_union_has_ (v, "undefined"))
            return mulib_webidl_fail_ (
                p, v->offset, "undefined argument or dictionary member");
        }
      if (!strcmp (v->kind, "constructor")
          && strcmp (parent->kind, "interface"))
        return mulib_webidl_fail_ (p, v->offset,
                                   "constructor outside interface");
      for (other = parent->child; other != v; other = other->next)
        {
          if (++p->checks > 4194304)
            return mulib_webidl_fail_ (p, v->offset,
                                       "validation work limit exceeded");
          mulib_webidl_node_ *oname = mulib_webidl_child_ (other, "name");
          if (!strcmp (v->kind, "value") && !strcmp (other->kind, "value")
              && mulib_webidl_equal_ (v->value, other->value))
            return mulib_webidl_fail_ (p, v->offset, "duplicate enum value");
          if (!name || !oname
              || !mulib_webidl_equal_ (name->value, oname->value))
            continue;
          if (!strcmp (parent->kind, "webidl"))
            {
              if (!strcmp (v->kind, "includes")
                  || !strcmp (other->kind, "includes"))
                continue;
              if (!strcmp (v->kind, other->kind)
                  && (mulib_webidl_child_ (v, "partial")
                      || mulib_webidl_child_ (other, "partial")))
                continue;
            }
          else if (!strcmp (v->kind, "operation")
                   && !strcmp (other->kind, "operation"))
            continue;
          return mulib_webidl_fail_ (p, name->offset, "duplicate name");
        }
      if (!mulib_webidl_validate_nodes_ (p, v))
        return 0;
    }
  return 1;
}
/* Quoted strings are JSON-compatible YAML double quoted scalars and valid
 * S-expression strings. C strings use octal rather than Unicode escapes. */
static int
mulib_webidl_quote_ (mulib_buffer_t *b, mulib_webidl_slice_ s, int c_string)
{
  size_t i;
  if (!mulib_webidl_text_ (b, "\""))
    return 0;
  for (i = 0; i < s.n; ++i)
    {
      unsigned char c = (unsigned char)s.p[i];
      char escape[8];
      /* YAML normalizes these Unicode line breaks even in quoted scalars. */
      if (!c_string && i + 1 < s.n && c == 0xc2
          && (unsigned char)s.p[i + 1] == 0x85)
        {
          if (!mulib_webidl_text_ (b, "\\u0085"))
            return 0;
          ++i;
          continue;
        }
      if (!c_string && i + 2 < s.n && c == 0xe2
          && (unsigned char)s.p[i + 1] == 0x80
          && ((unsigned char)s.p[i + 2] == 0xa8
              || (unsigned char)s.p[i + 2] == 0xa9))
        {
          if (!mulib_webidl_text_ (b, (unsigned char)s.p[i + 2] == 0xa8
                                          ? "\\u2028"
                                          : "\\u2029"))
            return 0;
          i += 2;
          continue;
        }
      if (c == '"' || c == '\\' || (c_string && c == '?'))
        {
          escape[0] = '\\';
          escape[1] = (char)c;
          if (!mulib_webidl_bytes_ (b, escape, 2))
            return 0;
        }
      else if (c < 32 || c == 127 || (c_string && c >= 128))
        {
          if (c_string)
            (void)snprintf (escape, sizeof escape, "\\%03o", c);
          else
            (void)snprintf (escape, sizeof escape, "\\u%04x", c);
          if (!mulib_webidl_text_ (b, escape))
            return 0;
        }
      else if (!mulib_webidl_bytes_ (b, &c, 1))
        return 0;
    }
  return mulib_webidl_text_ (b, "\"");
}
static int
mulib_webidl_sexpr_ (mulib_buffer_t *b, const mulib_webidl_node_ *v)
{
  const mulib_webidl_node_ *c;
  if (!mulib_webidl_text_ (b, "(") || !mulib_webidl_text_ (b, v->kind))
    return 0;
  if (v->value.p
      && (!mulib_webidl_text_ (b, " ")
          || !mulib_webidl_quote_ (b, v->value, 0)))
    return 0;
  for (c = v->child; c; c = c->next)
    if (!mulib_webidl_text_ (b, " ") || !mulib_webidl_sexpr_ (b, c))
      return 0;
  return mulib_webidl_text_ (b, ")");
}
static int
mulib_webidl_indent_ (mulib_buffer_t *b, size_t n)
{
  while (n--)
    if (!mulib_webidl_text_ (b, " "))
      return 0;
  return 1;
}
static int
mulib_webidl_yaml_ (mulib_buffer_t *b, const mulib_webidl_node_ *v,
                    size_t indent)
{
  const mulib_webidl_node_ *c;
  if (!mulib_webidl_indent_ (b, indent) || !mulib_webidl_text_ (b, "kind: ")
      || !mulib_webidl_text_ (b, v->kind) || !mulib_webidl_text_ (b, "\n"))
    return 0;
  if (v->value.p
      && (!mulib_webidl_indent_ (b, indent)
          || !mulib_webidl_text_ (b, "value: ")
          || !mulib_webidl_quote_ (b, v->value, 0)
          || !mulib_webidl_text_ (b, "\n")))
    return 0;
  if (!mulib_webidl_indent_ (b, indent)
      || !mulib_webidl_text_ (b, v->child ? "children:\n" : "children: []\n"))
    return 0;
  for (c = v->child; c; c = c->next)
    if (!mulib_webidl_indent_ (b, indent) || !mulib_webidl_text_ (b, "  -\n")
        || !mulib_webidl_yaml_ (b, c, indent + 4))
      return 0;
  return 1;
}
static int
mulib_webidl_cst_ (mulib_webidl_parser_ *p, mulib_buffer_t *b)
{
  size_t i;
  mulib_webidl_token_ *ts = (mulib_webidl_token_ *)p->tokens.data;
  if (!mulib_webidl_text_ (b, "(webidl-cst"))
    return 0;
  for (i = 0; i < p->count; ++i)
    {
      char number[64];
      mulib_webidl_slice_ s;
      s.p = p->source + ts[i].start;
      s.n = ts[i].end - ts[i].start;
      (void)snprintf (number, sizeof number, " %zu ", ts[i].start);
      if (!mulib_webidl_text_ (b, "\n  (")
          || !mulib_webidl_text_ (b, ts[i].kind)
          || !mulib_webidl_text_ (b, number) || !mulib_webidl_quote_ (b, s, 0)
          || !mulib_webidl_text_ (b, ")"))
        return 0;
    }
  return mulib_webidl_text_ (b, ")\n");
}

typedef struct mulib_webidl_ctype_cache_
{
  const mulib_webidl_node_ *node;
  int nonnullable, active;
  mulib_buffer_t spelling;
  struct mulib_webidl_ctype_cache_ *next;
} mulib_webidl_ctype_cache_;
typedef struct mulib_webidl_c_
{
  mulib_webidl_parser_ *p;
  mulib_buffer_t helpers;
  size_t serial, steps, cached_bytes;
  mulib_webidl_ctype_cache_ *cache;
} mulib_webidl_c_;
static int
mulib_webidl_cname_ (mulib_buffer_t *b, mulib_webidl_slice_ name)
{
  size_t i;
  if (!mulib_webidl_text_ (b, "webidl_"))
    return 0;
  for (i = 0; i < name.n; ++i)
    {
      if (name.p[i] == '_')
        {
          if (!mulib_webidl_text_ (b, "_u"))
            return 0;
        }
      else if (name.p[i] == '-')
        {
          if (!mulib_webidl_text_ (b, "_h"))
            return 0;
        }
      else if (!mulib_webidl_bytes_ (b, name.p + i, 1))
        return 0;
    }
  return 1;
}
static mulib_webidl_node_ *
mulib_webidl_find_ (mulib_webidl_parser_ *p, mulib_webidl_slice_ name)
{
  mulib_webidl_node_ *v;
  for (v = p->root->child; v; v = v->next)
    {
      if (++p->checks > 4194304)
        {
          mulib_webidl_fail_ (p, v->offset,
                              "name resolution work limit exceeded");
          return NULL;
        }
      mulib_webidl_node_ *n = mulib_webidl_child_ (v, "name");
      if (n && strcmp (v->kind, "includes")
          && !mulib_webidl_child_ (v, "partial")
          && mulib_webidl_equal_ (n->value, name))
        return v;
    }
  return NULL;
}
static int mulib_webidl_ctype_ (mulib_webidl_c_ *, const mulib_webidl_node_ *,
                                mulib_buffer_t *, size_t, int);
static int
mulib_webidl_creturn_ (mulib_webidl_c_ *c, const mulib_webidl_node_ *t,
                       mulib_buffer_t *b, size_t depth)
{
  const mulib_webidl_node_ *base = t;
  size_t n = 0;
  while (base && !mulib_webidl_child_ (base, "nullable") && n++ < 64)
    {
      mulib_webidl_node_ *alias;
      if (mulib_webidl_value_is_ (base, "undefined"))
        return mulib_webidl_text_ (b, "void");
      alias = mulib_webidl_find_ (c->p, base->value);
      if (!alias || strcmp (alias->kind, "typedef"))
        break;
      base = mulib_webidl_child_ (alias, "type");
    }
  return mulib_webidl_ctype_ (c, t, b, depth, 0);
}
static int
mulib_webidl_cargs_ (mulib_webidl_c_ *c, const mulib_webidl_node_ *v,
                     mulib_buffer_t *b, int have_self, size_t depth)
{
  const mulib_webidl_node_ *a;
  size_t ordinal = 0;
  int comma = have_self;
  for (a = v->child; a; a = a->next)
    if (!strcmp (a->kind, "argument"))
      {
        char suffix[80];
        mulib_webidl_node_ *t = mulib_webidl_child_ (a, "type");
        if (comma && !mulib_webidl_text_ (b, ", "))
          return 0;
        if (mulib_webidl_child_ (a, "optional"))
          {
            (void)snprintf (suffix, sizeof suffix, "int webidl__has_%zu, ",
                            ordinal);
            if (!mulib_webidl_text_ (b, suffix))
              return 0;
          }
        if (!mulib_webidl_ctype_ (c, t, b, depth + 1, 0))
          return 0;
        if (mulib_webidl_child_ (a, "variadic"))
          {
            (void)snprintf (suffix, sizeof suffix,
                            " *webidl__arg_%zu, size_t webidl__count_%zu",
                            ordinal, ordinal);
          }
        else
          (void)snprintf (suffix, sizeof suffix, " webidl__arg_%zu", ordinal);
        if (!mulib_webidl_text_ (b, suffix))
          return 0;
        comma = 1;
        ++ordinal;
      }
  return comma || mulib_webidl_text_ (b, "void");
}
static int
mulib_webidl_ctype_build_ (mulib_webidl_c_ *c, const mulib_webidl_node_ *t,
                           mulib_buffer_t *b, size_t depth, int nonnullable)
{
  static const struct
  {
    const char *idl, *c;
  } scalars[] = { { "boolean", "bool" },
                  { "byte", "int8_t" },
                  { "octet", "uint8_t" },
                  { "short", "int16_t" },
                  { "unsigned short", "uint16_t" },
                  { "long", "int32_t" },
                  { "unsigned long", "uint32_t" },
                  { "long long", "int64_t" },
                  { "unsigned long long", "uint64_t" },
                  { "float", "float" },
                  { "unrestricted float", "float" },
                  { "double", "double" },
                  { "unrestricted double", "double" },
                  { "void", "void" },
                  { "undefined", "webidl__undefined" },
                  { "DOMString", "webidl__string" },
                  { "USVString", "webidl__string" },
                  { "ByteString", "webidl__string" },
                  { "bigint", "webidl__string" },
                  { "any", "webidl__any" },
                  { "object", "void *" },
                  { "symbol", "void *" },
                  { "ArrayBuffer", "webidl__bytes" },
                  { "SharedArrayBuffer", "webidl__bytes" },
                  { "ArrayBufferView", "webidl__bytes" },
                  { "BufferSource", "webidl__bytes" },
                  { "DataView", "webidl__bytes" },
                  { "Int8Array", "webidl__bytes" },
                  { "Uint8Array", "webidl__bytes" },
                  { "Uint8ClampedArray", "webidl__bytes" },
                  { "Int16Array", "webidl__bytes" },
                  { "Uint16Array", "webidl__bytes" },
                  { "Int32Array", "webidl__bytes" },
                  { "Uint32Array", "webidl__bytes" },
                  { "Float16Array", "webidl__bytes" },
                  { "Float32Array", "webidl__bytes" },
                  { "Float64Array", "webidl__bytes" },
                  { "BigInt64Array", "webidl__bytes" },
                  { "BigUint64Array", "webidl__bytes" } };
  mulib_webidl_node_ *def, *a;
  mulib_buffer_t fields = { 0 };
  char helper[80];
  size_t i, index = 0;
  int ok = 0;
  if (!t)
    return mulib_webidl_fail_ (c->p, 0, "missing C type");
  if (depth > 64 || ++c->steps > 262144)
    return mulib_webidl_fail_ (c->p, t->offset,
                               "C type expansion limit or cyclic typedef");
  if (!nonnullable && mulib_webidl_child_ (t, "nullable"))
    {
      if (!mulib_webidl_text_ (&fields, "int is_null; ")
          || !mulib_webidl_ctype_ (c, t, &fields, depth + 1, 1)
          || !mulib_webidl_text_ (&fields, " value;"))
        goto done;
    }
  else
    {
      for (i = 0; i < sizeof scalars / sizeof *scalars; ++i)
        if (mulib_webidl_value_is_ (t, scalars[i].idl))
          return mulib_webidl_text_ (b, scalars[i].c);
      if (mulib_webidl_value_is_ (t, "Promise")
          || mulib_webidl_value_is_ (t, "async_sequence"))
        {
          ok = mulib_webidl_ctype_ (c, mulib_webidl_child_ (t, "type"),
                                    &fields, depth + 1, 0)
               && mulib_webidl_text_ (b, mulib_webidl_value_is_ (t, "Promise")
                                             ? "webidl__promise *"
                                             : "webidl__async_sequence *");
          goto done;
        }
      if (mulib_webidl_value_is_ (t, "sequence")
          || mulib_webidl_value_is_ (t, "FrozenArray")
          || mulib_webidl_value_is_ (t, "ObservableArray"))
        {
          if (!mulib_webidl_ctype_ (c, mulib_webidl_child_ (t, "type"),
                                    &fields, depth + 1, 0)
              || !mulib_webidl_text_ (&fields, " *data; size_t length;"))
            goto done;
        }
      else if (mulib_webidl_value_is_ (t, "record"))
        {
          a = mulib_webidl_child_ (t, "type");
          a = a ? a->next : NULL;
          if (!a || !mulib_webidl_text_ (&fields, "webidl__string *keys; ")
              || !mulib_webidl_ctype_ (c, a, &fields, depth + 1, 0)
              || !mulib_webidl_text_ (&fields, " *values; size_t length;"))
            goto done;
        }
      else if (mulib_webidl_value_is_ (t, "union"))
        {
          if (!mulib_webidl_text_ (&fields, "size_t tag; union { "))
            goto done;
          for (a = t->child; a; a = a->next)
            if (!strcmp (a->kind, "type"))
              {
                (void)snprintf (helper, sizeof helper, " value_%zu; ",
                                index++);
                if (!mulib_webidl_ctype_ (c, a, &fields, depth + 1, 0)
                    || !mulib_webidl_text_ (&fields, helper))
                  goto done;
              }
          if (!mulib_webidl_text_ (&fields, "} value;"))
            goto done;
        }
      else
        {
          def = !strcmp (t->kind, "callback")
                    ? (mulib_webidl_node_ *)t
                    : mulib_webidl_find_ (c->p, t->value);
          if (!def)
            return mulib_webidl_fail_ (c->p, t->offset, "unresolved C type");
          if (!strcmp (def->kind, "typedef"))
            return mulib_webidl_ctype_ (c, mulib_webidl_child_ (def, "type"),
                                        b, depth + 1, 0);
          if (!strcmp (def->kind, "callback"))
            {
              mulib_buffer_t signature = { 0 };
              if (t != def)
                return mulib_webidl_ctype_ (c, def, b, depth + 1, 0);
              (void)snprintf (helper, sizeof helper, "webidl__type_%zu",
                              c->serial++);
              ok = mulib_webidl_text_ (&signature, "typedef ")
                   && mulib_webidl_creturn_ (c,
                                             mulib_webidl_child_ (def, "type"),
                                             &signature, depth + 1)
                   && mulib_webidl_text_ (&signature, " (*")
                   && mulib_webidl_text_ (&signature, helper)
                   && mulib_webidl_text_ (&signature, ")(")
                   && mulib_webidl_cargs_ (c, def, &signature, 0, depth + 1)
                   && mulib_webidl_text_ (&signature, ");\n")
                   && mulib_webidl_bytes_ (&c->helpers, signature.data,
                                           signature.length)
                   && mulib_webidl_text_ (b, helper);
              mulib_buffer_free (&signature);
              return ok;
            }
          if (!strcmp (def->kind, "enum"))
            return mulib_webidl_text_ (b, "const char *");
          if (strcmp (def->kind, "interface")
              && strcmp (def->kind, "callback-interface")
              && strcmp (def->kind, "dictionary"))
            return mulib_webidl_fail_ (c->p, t->offset,
                                       "definition is not a C type");
          return mulib_webidl_cname_ (b, t->value)
                 && mulib_webidl_text_ (b, " *");
        }
    }
  (void)snprintf (helper, sizeof helper, "webidl__type_%zu", c->serial++);
  ok = mulib_webidl_text_ (&c->helpers, "typedef struct { ")
       && mulib_webidl_bytes_ (&c->helpers, fields.data, fields.length)
       && mulib_webidl_text_ (&c->helpers, " } ")
       && mulib_webidl_text_ (&c->helpers, helper)
       && mulib_webidl_text_ (&c->helpers, ";\n")
       && mulib_webidl_text_ (b, helper);
done:
  mulib_buffer_free (&fields);
  return ok;
}
/* Memoizing type spellings gives typedefs and getters/setters the same C
 * aggregate identity, and detects cycles without exponentially expanding DAGs.
 */
static int
mulib_webidl_ctype_ (mulib_webidl_c_ *c, const mulib_webidl_node_ *t,
                     mulib_buffer_t *b, size_t depth, int nonnullable)
{
  mulib_webidl_ctype_cache_ *entry;
  if (!t)
    return mulib_webidl_fail_ (c->p, 0, "missing C type");
  for (entry = c->cache; entry; entry = entry->next)
    {
      if (++c->p->checks > 4194304)
        return mulib_webidl_fail_ (c->p, t->offset,
                                   "C type work limit exceeded");
      if (entry->node == t && entry->nonnullable == nonnullable)
        {
          if (entry->active)
            return mulib_webidl_fail_ (c->p, t->offset, "cyclic C type");
          return mulib_webidl_bytes_ (b, entry->spelling.data,
                                      entry->spelling.length);
        }
    }
  entry = (mulib_webidl_ctype_cache_ *)calloc (1, sizeof *entry);
  if (!entry)
    return mulib_webidl_fail_ (c->p, t->offset, "allocation failed");
  entry->node = t;
  entry->nonnullable = nonnullable;
  entry->active = 1;
  entry->next = c->cache;
  c->cache = entry;
  if (!mulib_webidl_ctype_build_ (c, t, &entry->spelling, depth, nonnullable))
    return 0;
  if (entry->spelling.length > 67108864 - c->cached_bytes)
    return mulib_webidl_fail_ (c->p, t->offset,
                               "C type cache byte limit exceeded");
  c->cached_bytes += entry->spelling.length;
  entry->active = 0;
  return mulib_webidl_bytes_ (b, entry->spelling.data, entry->spelling.length);
}
static int
mulib_webidl_csymbol_ (mulib_buffer_t *b, const mulib_webidl_node_ *def,
                       const mulib_webidl_node_ *member, const char *suffix)
{
  mulib_webidl_node_ *name = mulib_webidl_child_ (def, "name");
  mulib_webidl_node_ *mn = mulib_webidl_child_ (member, "name");
  if (!mulib_webidl_cname_ (b, name->value) || !mulib_webidl_text_ (b, "__"))
    return 0;
  if (mn
      && (!mulib_webidl_cname_ (b, mn->value)
          || !mulib_webidl_text_ (b, "__")))
    return 0;
  return mulib_webidl_text_ (b, suffix);
}
static int
mulib_webidl_cself_ (mulib_buffer_t *b, const mulib_webidl_node_ *def)
{
  return mulib_webidl_cname_ (b, mulib_webidl_child_ (def, "name")->value)
         && mulib_webidl_text_ (b, " *webidl__self");
}
static int
mulib_webidl_cconstant_ (mulib_webidl_c_ *c, mulib_buffer_t *b,
                         const mulib_webidl_node_ *def,
                         const mulib_webidl_node_ *v)
{
  mulib_webidl_node_ *literal = mulib_webidl_child_ (v, "default");
  mulib_webidl_node_ *type = mulib_webidl_child_ (v, "type");
  mulib_webidl_node_ *base = type;
  size_t depth = 0;
  int numeric = 0;
  const char *mapped = NULL;
  char integer[96];
  if (!literal || !type)
    return 0;
  while (base && depth++ < 64)
    {
      static const char *const primitives[] = { "boolean",
                                                "byte",
                                                "octet",
                                                "short",
                                                "unsigned short",
                                                "long",
                                                "unsigned long",
                                                "long long",
                                                "unsigned long long",
                                                "float",
                                                "unrestricted float",
                                                "double",
                                                "unrestricted double" };
      size_t j;
      mulib_webidl_node_ *alias;
      if (mulib_webidl_child_ (base, "nullable"))
        break;
      for (j = 0; j < sizeof primitives / sizeof *primitives; ++j)
        if (mulib_webidl_value_is_ (base, primitives[j]))
          numeric = 1;
      if (numeric)
        break;
      alias = mulib_webidl_find_ (c->p, base->value);
      if (!alias || strcmp (alias->kind, "typedef"))
        break;
      base = mulib_webidl_child_ (alias, "type");
    }
  if (!numeric)
    return mulib_webidl_fail_ (
        c->p, type->offset,
        "C constant requires a nonnullable numeric/boolean type");
  if (mulib_webidl_value_is_ (literal, "true"))
    mapped = "1";
  else if (mulib_webidl_value_is_ (literal, "false"))
    mapped = "0";
  else if (mulib_webidl_value_is_ (literal, "Infinity"))
    mapped = "INFINITY";
  else if (literal->value.n > 1 && literal->value.p[0] == '-'
           && !mulib_webidl_digit_ ((unsigned char)literal->value.p[1])
           && literal->value.p[1] != '.')
    mapped = "(-INFINITY)";
  else if (mulib_webidl_value_is_ (literal, "NaN"))
    mapped = "NAN";
  else if (!literal->value.n
           || (literal->value.p[0] != '-'
               && !mulib_webidl_digit_ ((unsigned char)literal->value.p[0])
               && literal->value.p[0] != '.'))
    return mulib_webidl_fail_ (c->p, literal->offset,
                               "C constants must be numeric or boolean");
  if (!mapped)
    {
      size_t j = literal->value.p[0] == '-' ? 1 : 0, start = j;
      unsigned radix = 10;
      uint64_t value = 0;
      int integral = 1;
      if (j + 1 < literal->value.n && literal->value.p[j] == '0'
          && (literal->value.p[j + 1] == 'x'
              || literal->value.p[j + 1] == 'X'))
        {
          radix = 16;
          j += 2;
        }
      else
        {
          size_t k;
          for (k = j; k < literal->value.n; ++k)
            if (literal->value.p[k] == '.' || literal->value.p[k] == 'e'
                || literal->value.p[k] == 'E')
              integral = 0;
          if (j + 1 < literal->value.n && literal->value.p[j] == '0')
            radix = 8;
        }
      if (integral)
        {
          for (; j < literal->value.n; ++j)
            {
              unsigned char ch = (unsigned char)literal->value.p[j];
              unsigned digit = ch <= '9'   ? (unsigned)(ch - '0')
                               : ch <= 'F' ? (unsigned)(ch - 'A' + 10)
                                           : (unsigned)(ch - 'a' + 10);
              if (value > (UINT64_MAX - digit) / radix)
                return mulib_webidl_fail_ (
                    c->p, literal->offset,
                    "C integer literal exceeds 64 bits");
              value = value * radix + digit;
            }
          if (start)
            {
              if (value > (uint64_t)INT64_MAX + 1)
                return mulib_webidl_fail_ (
                    c->p, literal->offset,
                    "C negative integer literal exceeds 64 bits");
              if (value == (uint64_t)INT64_MAX + 1)
                (void)snprintf (integer, sizeof integer,
                                "(-9223372036854775807LL - 1LL)");
              else
                (void)snprintf (integer, sizeof integer, "(-%" PRIu64 "LL)",
                                value);
            }
          else
            (void)snprintf (integer, sizeof integer, "%" PRIu64 "ULL", value);
          mapped = integer;
        }
    }
  return mulib_webidl_text_ (b, "#define ")
         && mulib_webidl_csymbol_ (b, def, v, "constant")
         && mulib_webidl_text_ (b, " ((")
         && mulib_webidl_ctype_ (c, type, b, 0, 0)
         && mulib_webidl_text_ (b, ")(")
         && (mapped
                 ? mulib_webidl_text_ (b, mapped)
                 : mulib_webidl_bytes_ (b, literal->value.p, literal->value.n))
         && mulib_webidl_text_ (b, "))\n");
}
static int
mulib_webidl_cmember_ (mulib_webidl_c_ *c, mulib_buffer_t *b,
                       const mulib_webidl_node_ *def,
                       const mulib_webidl_node_ *v, size_t ordinal)
{
  int self
      = strcmp (def->kind, "namespace") && !mulib_webidl_child_ (v, "static");
  mulib_webidl_node_ *type = mulib_webidl_child_ (v, "type");
  if (!strcmp (v->kind, "const"))
    return mulib_webidl_cconstant_ (c, b, def, v);
  if (!strcmp (v->kind, "attribute"))
    {
      if (!mulib_webidl_ctype_ (c, type, b, 0, 0)
          || !mulib_webidl_text_ (b, " ")
          || !mulib_webidl_csymbol_ (b, def, v, "get")
          || !mulib_webidl_text_ (b, "("))
        return 0;
      if (!(self ? mulib_webidl_cself_ (b, def)
                 : mulib_webidl_text_ (b, "void"))
          || !mulib_webidl_text_ (b, ");\n"))
        return 0;
      if (!mulib_webidl_child_ (v, "readonly"))
        {
          if (!mulib_webidl_text_ (b, "void ")
              || !mulib_webidl_csymbol_ (b, def, v, "set")
              || !mulib_webidl_text_ (b, "("))
            return 0;
          if (self
              && (!mulib_webidl_cself_ (b, def)
                  || !mulib_webidl_text_ (b, ", ")))
            return 0;
          if (!mulib_webidl_ctype_ (c, type, b, 0, 0)
              || !mulib_webidl_text_ (b, " webidl__value);\n"))
            return 0;
        }
      return 1;
    }
  if (!strcmp (v->kind, "operation") || !strcmp (v->kind, "constructor"))
    {
      char suffix[64];
      int ctor = !strcmp (v->kind, "constructor");
      if (ctor)
        {
          self = 0;
          if (!mulib_webidl_cname_ (b,
                                    mulib_webidl_child_ (def, "name")->value)
              || !mulib_webidl_text_ (b, " *"))
            return 0;
        }
      else if (!mulib_webidl_creturn_ (c, type, b, 0))
        return 0;
      (void)snprintf (suffix, sizeof suffix, "%s_%zu", ctor ? "new" : "call",
                      ordinal);
      if (!mulib_webidl_text_ (b, " ")
          || !mulib_webidl_csymbol_ (b, def, v, suffix)
          || !mulib_webidl_text_ (b, "(")
          || (self && !mulib_webidl_cself_ (b, def))
          || !mulib_webidl_cargs_ (c, v, b, self, 0)
          || !mulib_webidl_text_ (b, ");\n"))
        return 0;
      return 1;
    }
  return mulib_webidl_fail_ (
      c->p, v->offset,
      "C99 does not lower iterable/maplike/setlike/stringifier members");
}
static int
mulib_webidl_is_member_ (const mulib_webidl_node_ *v)
{
  return strcmp (v->kind, "name") && strcmp (v->kind, "inheritance")
         && strcmp (v->kind, "partial")
         && strcmp (v->kind, "extended-attributes");
}
static int
mulib_webidl_copy_node_ (mulib_webidl_parser_ *p, mulib_webidl_node_ *parent,
                         const mulib_webidl_node_ *source)
{
  const mulib_webidl_node_ *child;
  mulib_webidl_node_ *v
      = mulib_webidl_node_new_ (p, parent, source->kind, source->value.p,
                                source->value.n, source->offset);
  if (!v)
    return 0;
  for (child = source->child; child; child = child->next)
    if (!mulib_webidl_copy_node_ (p, v, child))
      return 0;
  return 1;
}
/* C lowering links only its private tree; semantic output preserves fragments.
 * First merge partials (including partial mixins), then copy included members.
 */
static int
mulib_webidl_merge_ (mulib_webidl_parser_ *p)
{
  mulib_webidl_node_ *d, *v, *base, **link;
  for (d = p->root->child; d; d = d->next)
    if (mulib_webidl_child_ (d, "partial"))
      {
        base = mulib_webidl_find_ (p, mulib_webidl_child_ (d, "name")->value);
        if (!base || strcmp (base->kind, d->kind))
          return mulib_webidl_fail_ (
              p, d->offset, "unresolved or mismatched partial definition");
        for (v = d->child; v; v = v->next)
          if (mulib_webidl_is_member_ (v)
              && !mulib_webidl_copy_node_ (p, base, v))
            return 0;
      }
  for (d = p->root->child; d; d = d->next)
    if (!strcmp (d->kind, "includes"))
      {
        mulib_webidl_node_ *mixin
            = mulib_webidl_find_ (p, mulib_webidl_child_ (d, "mixin")->value);
        base = mulib_webidl_find_ (p, mulib_webidl_child_ (d, "name")->value);
        if (!base || strcmp (base->kind, "interface") || !mixin
            || strcmp (mixin->kind, "mixin"))
          return mulib_webidl_fail_ (
              p, d->offset, "includes requires an interface and a mixin");
        for (v = mixin->child; v; v = v->next)
          if (mulib_webidl_is_member_ (v)
              && !mulib_webidl_copy_node_ (p, base, v))
            return 0;
      }
  link = &p->root->child;
  p->root->tail = NULL;
  while ((d = *link) != NULL)
    if (mulib_webidl_child_ (d, "partial") || !strcmp (d->kind, "mixin")
        || !strcmp (d->kind, "includes"))
      {
        *link = d->next;
        d->next = NULL;
        mulib_webidl_nodes_free_ (d);
      }
    else
      {
        p->root->tail = d;
        link = &d->next;
      }
  return mulib_webidl_validate_nodes_ (p, p->root);
}
static int
mulib_webidl_c99_ (mulib_webidl_parser_ *p, mulib_buffer_t *b)
{
  mulib_webidl_c_ c;
  mulib_webidl_node_ *d, *v, *name;
  mulib_buffer_t declarations = { 0 };
  int ok = 0;
  memset (&c, 0, sizeof c);
  c.p = p;
  if (!mulib_webidl_merge_ (p))
    goto done;
  if (!mulib_webidl_text_ (
          b, "/* Generated WebIDL C99 declarations; no runtime bindings. */\n"
             "#include <stddef.h>\n#include <stdint.h>\n#include "
             "<stdbool.h>\n#include <math.h>\n"
             "#ifndef MULIB_WEBIDL_C99_TYPES\n#define MULIB_WEBIDL_C99_TYPES\n"
             "typedef struct { const void *data; size_t length; } "
             "webidl__string;\n"
             "typedef struct { void *data; size_t length; } webidl__bytes;\n"
             "typedef struct { void *data; size_t tag; } webidl__any;\n"
             "typedef struct { unsigned char unused; } webidl__undefined;\n"
             "typedef struct webidl__async_sequence webidl__async_sequence;\n"
             "typedef struct webidl__promise webidl__promise;\n#endif\n"))
    goto done;
  for (d = p->root->child; d; d = d->next)
    {
      mulib_webidl_node_ *ancestor = d;
      size_t depth = 0;
      name = mulib_webidl_child_ (d, "name");
      while (mulib_webidl_child_ (ancestor, "inheritance"))
        {
          mulib_webidl_node_ *base
              = mulib_webidl_child_ (ancestor, "inheritance");
          ancestor = mulib_webidl_find_ (p, base->value);
          if (!ancestor || strcmp (ancestor->kind, d->kind) || ++depth > 64
              || ancestor == d)
            {
              mulib_webidl_fail_ (
                  p, base->offset,
                  "unresolved, mismatched or cyclic inheritance");
              goto done;
            }
        }
      if (!strcmp (d->kind, "dictionary") || !strcmp (d->kind, "interface")
          || !strcmp (d->kind, "callback-interface"))
        {
          if (!mulib_webidl_text_ (b, "typedef struct ")
              || !mulib_webidl_cname_ (b, name->value)
              || !mulib_webidl_text_ (b, " ")
              || !mulib_webidl_cname_ (b, name->value)
              || !mulib_webidl_text_ (b, ";\n"))
            goto done;
        }
    }
  for (d = p->root->child; d; d = d->next)
    {
      size_t ordinal = 0;
      name = mulib_webidl_child_ (d, "name");
      if (!strcmp (d->kind, "typedef") || !strcmp (d->kind, "callback"))
        {
          if (!mulib_webidl_text_ (&declarations, "typedef ")
              || !mulib_webidl_ctype_ (&c, name, &declarations, 0, 0)
              || !mulib_webidl_text_ (&declarations, " ")
              || !mulib_webidl_cname_ (&declarations, name->value)
              || !mulib_webidl_text_ (&declarations, ";\n"))
            goto done;
        }
      else if (!strcmp (d->kind, "enum"))
        {
          if (!mulib_webidl_text_ (&declarations, "typedef const char *")
              || !mulib_webidl_cname_ (&declarations, name->value)
              || !mulib_webidl_text_ (&declarations, ";\n"))
            goto done;
          for (v = d->child; v; v = v->next)
            if (!strcmp (v->kind, "value"))
              {
                char suffix[64];
                (void)snprintf (suffix, sizeof suffix, "__value_%zu ",
                                ordinal++);
                if (!mulib_webidl_text_ (&declarations, "#define ")
                    || !mulib_webidl_cname_ (&declarations, name->value)
                    || !mulib_webidl_text_ (&declarations, suffix)
                    || !mulib_webidl_quote_ (&declarations, v->value, 1)
                    || !mulib_webidl_text_ (&declarations, "\n"))
                  goto done;
              }
        }
      else if (!strcmp (d->kind, "dictionary"))
        {
          mulib_webidl_node_ *base = mulib_webidl_child_ (d, "inheritance");
          if (!mulib_webidl_text_ (&declarations, "struct ")
              || !mulib_webidl_cname_ (&declarations, name->value)
              || !mulib_webidl_text_ (&declarations, " {\n"))
            goto done;
          if (base)
            {
              mulib_webidl_node_ *parent = mulib_webidl_find_ (p, base->value);
              if (!parent || strcmp (parent->kind, "dictionary"))
                {
                  mulib_webidl_fail_ (p, base->offset,
                                      "invalid dictionary base");
                  goto done;
                }
              if (!mulib_webidl_text_ (&declarations, "  ")
                  || !mulib_webidl_cname_ (&declarations, base->value)
                  || !mulib_webidl_text_ (&declarations, " *webidl__base;\n"))
                goto done;
              ++ordinal;
            }
          for (v = d->child; v; v = v->next)
            if (!strcmp (v->kind, "field"))
              {
                mulib_webidl_node_ *field = mulib_webidl_child_ (v, "name");
                if (!mulib_webidl_child_ (v, "required"))
                  {
                    if (!mulib_webidl_text_ (&declarations, "  bool has__")
                        || !mulib_webidl_cname_ (&declarations, field->value)
                        || !mulib_webidl_text_ (&declarations, ";\n"))
                      goto done;
                  }
                if (!mulib_webidl_text_ (&declarations, "  ")
                    || !mulib_webidl_ctype_ (&c,
                                             mulib_webidl_child_ (v, "type"),
                                             &declarations, 0, 0)
                    || !mulib_webidl_text_ (&declarations, " ")
                    || !mulib_webidl_cname_ (&declarations, field->value)
                    || !mulib_webidl_text_ (&declarations, ";\n"))
                  goto done;
                ++ordinal;
              }
          if ((!ordinal
               && !mulib_webidl_text_ (&declarations,
                                       "  unsigned char webidl__empty;\n"))
              || !mulib_webidl_text_ (&declarations, "};\n"))
            goto done;
        }
      else
        {
          for (v = d->child; v; v = v->next)
            if (!strcmp (v->kind, "operation")
                || !strcmp (v->kind, "constructor")
                || !strcmp (v->kind, "attribute") || !strcmp (v->kind, "const")
                || !strcmp (v->kind, "iterable")
                || !strcmp (v->kind, "maplike") || !strcmp (v->kind, "setlike")
                || !strcmp (v->kind, "stringifier"))
              if (!mulib_webidl_cmember_ (&c, &declarations, d, v, ordinal++))
                goto done;
        }
    }
  ok = mulib_webidl_bytes_ (b, c.helpers.data, c.helpers.length)
       && mulib_webidl_bytes_ (b, declarations.data, declarations.length);
done:
  while (c.cache)
    {
      mulib_webidl_ctype_cache_ *next = c.cache->next;
      mulib_buffer_free (&c.cache->spelling);
      free (c.cache);
      c.cache = next;
    }
  mulib_buffer_free (&c.helpers);
  mulib_buffer_free (&declarations);
  return ok;
}

int
mulib_webidl_compile (const char *source, size_t length,
                      const mulib_webidl_options_t *options,
                      mulib_buffer_t *out, mulib_webidl_error_t *error)
{
  mulib_webidl_parser_ p;
  mulib_buffer_t result = { 0 };
  mulib_webidl_options_t opts = { MULIB_WEBIDL_SEXPR, MULIB_WEBIDL_AST };
  size_t i;
  int ok = 0;
  memset (&p, 0, sizeof p);
  p.source = source ? source : "";
  p.length = length;
  if (error)
    memset (error, 0, sizeof *error);
  if (options)
    opts = *options;
  if ((!source && length) || !out || length > 16777216
      || opts.format < MULIB_WEBIDL_SEXPR || opts.format > MULIB_WEBIDL_C99
      || opts.tree < MULIB_WEBIDL_AST || opts.tree > MULIB_WEBIDL_CST
      || (opts.tree == MULIB_WEBIDL_CST && opts.format != MULIB_WEBIDL_SEXPR))
    {
      mulib_webidl_fail_ (&p, 0, "invalid arguments, options or input length");
      goto done;
    }
  for (i = 0; i < length;)
    {
      uint32_t codepoint;
      size_t used;
      if (!mulib_utf8_decode ((const unsigned char *)source + i, length - i,
                              &codepoint, &used))
        {
          mulib_webidl_fail_ (&p, i, "invalid UTF-8 source");
          goto done;
        }
      i += used;
    }
  if (!mulib_webidl_lex_ (&p) || !mulib_webidl_parse_ (&p)
      || !mulib_webidl_validate_nodes_ (&p, p.root))
    goto done;
  if (opts.format == MULIB_WEBIDL_SEXPR)
    ok = opts.tree == MULIB_WEBIDL_CST
             ? mulib_webidl_cst_ (&p, &result)
             : (mulib_webidl_sexpr_ (&result, p.root)
                && mulib_webidl_text_ (&result, "\n"));
  else if (opts.format == MULIB_WEBIDL_CODEYAML)
    ok = mulib_webidl_yaml_ (&result, p.root, 0);
  else
    ok = mulib_webidl_c99_ (&p, &result);
  if (ok)
    ok = mulib_buffer_append (out, result.data, result.length);
  if (!ok && !p.message)
    mulib_webidl_fail_ (
        &p, mulib_webidl_offset_ (&p),
        "allocation failed, output limit exceeded or invalid output buffer");
done:
  if (!ok && error)
    {
      error->message = p.message;
      error->offset = p.error;
      error->line = error->column = 1;
      /* Invalid input pointers/lengths must not be inspected on argument
       * failure.
       */
      if (source && p.error <= length)
        for (i = 0; i < p.error; ++i)
          {
            if (source[i] == '\r')
              {
                ++error->line;
                error->column = 1;
                if (i + 1 < p.error && source[i + 1] == '\n')
                  ++i;
              }
            else if (source[i] == '\n')
              {
                ++error->line;
                error->column = 1;
              }
            else
              ++error->column;
          }
    }
  mulib_webidl_nodes_free_ (p.root);
  mulib_buffer_free (&p.tokens);
  mulib_buffer_free (&result);
  return ok;
}
#endif
