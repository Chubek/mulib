/** @file mulib-archml.h
 * @brief Validated architecture descriptions, P-code and bit-scattered encodings.
 * Define MULIB_IMPL_ARCHML and MULIB_IMPL_PARSEC once. ISO C11, no implicit
 * dependency implementations. See manual/mulib-archml.md for the versioned DSL.
 */
#ifndef MULIB_ARCHML_H
#define MULIB_ARCHML_H
#include "mulib-parsec.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_ARCHML_VERSION 1
#define MULIB_ARCHML_MAX_OPERANDS 16
#define MULIB_ARCHML_MAX_BYTES 32
typedef enum mulib_archml_result {
  MULIB_ARCHML_OK = 1, MULIB_ARCHML_INVALID = -1, MULIB_ARCHML_SYNTAX = -2,
  MULIB_ARCHML_SCHEMA = -3, MULIB_ARCHML_LIMIT = -4, MULIB_ARCHML_NOMEM = -5,
  MULIB_ARCHML_IO = -6, MULIB_ARCHML_RANGE = -7, MULIB_ARCHML_MISMATCH = -8
} mulib_archml_result;
typedef enum mulib_archml_kind {
  MULIB_ARCHML_ATOM = 1, MULIB_ARCHML_STRING, MULIB_ARCHML_LIST
} mulib_archml_kind;
/** Immutable borrowed view, valid until document destruction. text is decoded,
 * NUL-terminated for atoms/strings, NULL for lists. Offsets refer to source bytes.
 * Never mutate, copy ownership, or free a node. All traversal is nonrecursive. */
typedef struct mulib_archml_node {
  mulib_archml_kind kind;
  const char *text;
  size_t offset, length;
  const struct mulib_archml_node *child, *next;
  size_t subtree_first; /* Internal postorder interval start; read-only. */
} mulib_archml_node_t;
typedef struct mulib_archml mulib_archml_t;
typedef struct mulib_archml_error {
  mulib_archml_result code;
  size_t offset, line, column;
  const char *message; /* Static; survives destruction. */
} mulib_archml_error_t;
/** Zero selects 16 MiB source, 1000000 nodes, depth 128, 100000000 work units.
 * Work bounds parsing and semantic validation. Depth is S-expression nesting.
 * File loading obeys max_source before allocating the next input chunk. */
typedef struct mulib_archml_options {
  size_t max_source, max_nodes, max_depth, max_work;
} mulib_archml_options_t;
/** Parse the entire length-delimited source. Owns all returned text; source may
 * be released immediately. On failure *out is unchanged; free before reuse.
 * Rejects duplicate records/properties, unresolved refs, invalid encodings,
 * undeclared P-code operations, and invalid register views. Error is optional. */
mulib_archml_result mulib_archml_parse (const char *, size_t,
    const mulib_archml_options_t *, mulib_archml_t **out, mulib_archml_error_t *);
mulib_archml_result mulib_archml_load (const char *path,
    const mulib_archml_options_t *, mulib_archml_t **out, mulib_archml_error_t *);
void mulib_archml_destroy (mulib_archml_t *);
const char *mulib_archml_name (const mulib_archml_t *);
const mulib_archml_node_t *mulib_archml_root (const mulib_archml_t *);
/** Record lookup is O(log records); at() preserves source order. */
size_t mulib_archml_count (const mulib_archml_t *, const char *kind);
const mulib_archml_node_t *mulib_archml_at (const mulib_archml_t *, const char *kind, size_t);
const mulib_archml_node_t *mulib_archml_find (const mulib_archml_t *, const char *kind, const char *name);
const mulib_archml_node_t *mulib_archml_child (const mulib_archml_node_t *, size_t);
const mulib_archml_node_t *mulib_archml_property (const mulib_archml_node_t *, const char *);
/** Strict decimal/0x unsigned integer conversion; no signs, whitespace or wrap.
 * Failure leaves output unchanged. */
int mulib_archml_u64 (const mulib_archml_node_t *, uint64_t *);
/** Signed decimal/0x conversion; optional leading '-' only, including INT64_MIN. */
int mulib_archml_i64 (const mulib_archml_node_t *, int64_t *);
typedef struct mulib_archml_register {
  const char *name, *storage, *write_policy;
  uint64_t bits, offset, encoding, dwarf;
} mulib_archml_register_t;
int mulib_archml_register_info (const mulib_archml_t *, const char *, mulib_archml_register_t *);
/** Return 1 for overlapping storage slices, 0 disjoint, -1 unknown register. */
int mulib_archml_register_overlap (const mulib_archml_t *, const char *, const char *);
/** Feature availability includes transitive (requires (ref feature ...)).
 * A feature must be explicitly listed AND all its dependencies satisfied.
 * Instructions without requires are available. Returns -1 on invalid names or
 * cycles. No host probing, implied OS state, or implicit extension enablement. */
int mulib_archml_enabled (const mulib_archml_t *, const mulib_archml_node_t *,
    const char *const *features, size_t count);
/** Encode/decode a named, concrete instruction. Operand order is the DSL order.
 * Values are uint64 bit patterns; signed operands use two's complement, and
 * scaled immediates are supplied in bytes (must be divisible by scale).
 * Only encoding constraints are enforced here: callers check modes/features,
 * register allocation restrictions, privilege, and memory legality separately.
 * Outputs including consumed/length remain unchanged on failure. Decode reads
 * a prefix, preserving trailing instructions. No allocation. NULL operands are
 * valid only with count zero; output buffers must not overlap operand arrays.
 */
mulib_archml_result mulib_archml_encode (const mulib_archml_t *, const char *,
    const uint64_t *operands, size_t count, void *bytes, size_t capacity, size_t *length);
mulib_archml_result mulib_archml_decode (const mulib_archml_t *, const char *,
    const void *bytes, size_t length, uint64_t *operands, size_t count, size_t *consumed);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ARCHML) && !defined(MULIB_ARCHML_IMPLEMENTED)
#define MULIB_ARCHML_IMPLEMENTED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_archml {
  mulib_archml_node_t *nodes;
  char *text;
  const mulib_archml_node_t *root, **records;
  size_t node_count, record_count;
};
static mulib_archml_result
mulib_archml_error_ (mulib_archml_error_t *e, mulib_archml_result code,
                     size_t at, const char *message)
{
  if (e) { e->code = code; e->offset = at; e->line = e->column = 1; e->message = message; }
  return code;
}
const mulib_archml_node_t *mulib_archml_child (const mulib_archml_node_t *n, size_t i)
{
  if (!n || n->kind != MULIB_ARCHML_LIST) return NULL;
  n = n->child;
  while (n && i) { n = n->next; --i; }
  return n;
}
static const char *mulib_archml_text_ (const mulib_archml_node_t *n)
{ return n && n->kind != MULIB_ARCHML_LIST ? n->text : NULL; }
static const char *mulib_archml_word_ (const mulib_archml_node_t *n, size_t i)
{ return mulib_archml_text_ (mulib_archml_child (n, i)); }
static int mulib_archml_is_ (const mulib_archml_node_t *n, const char *s)
{ const char *t = mulib_archml_text_ (n); return t && !strcmp (t, s); }
const mulib_archml_node_t *mulib_archml_property (const mulib_archml_node_t *n, const char *name)
{
  if (!name) return NULL;
  for (n = mulib_archml_child (n, 0); n; n = n->next)
    if (mulib_archml_is_ (mulib_archml_child (n, 0), name)) return n;
  return NULL;
}
int mulib_archml_u64 (const mulib_archml_node_t *n, uint64_t *out)
{
  const char *s = mulib_archml_text_ (n);
  uint64_t v = 0;
  unsigned base = 10;
  if (!s || !*s || !out) return 0;
  if (s[0] == '0' && s[1] == 'x') { base = 16; s += 2; if (!*s) return 0; }
  for (; *s; ++s) {
    unsigned d = *s >= '0' && *s <= '9' ? (unsigned)(*s - '0')
      : *s >= 'a' && *s <= 'f' ? (unsigned)(*s - 'a' + 10)
      : *s >= 'A' && *s <= 'F' ? (unsigned)(*s - 'A' + 10) : 16;
    if (d >= base || v > (UINT64_MAX - d) / base) return 0;
    v = v * base + d;
  }
  *out = v; return 1;
}
static int mulib_archml_number_ (const mulib_archml_node_t *n, const char *key, uint64_t *v)
{ return mulib_archml_u64 (mulib_archml_child (mulib_archml_property (n, key), 1), v); }
int mulib_archml_i64 (const mulib_archml_node_t *n, int64_t *out)
{
  mulib_archml_node_t copy;
  uint64_t v;
  int negative;
  if (!out || !mulib_archml_text_ (n)) return 0;
  copy = *n; negative = copy.text[0] == '-';
  if (negative) ++copy.text;
  if (!mulib_archml_u64 (&copy, &v) || v > (uint64_t)INT64_MAX + (unsigned)negative) return 0;
  *out = negative ? v == (UINT64_C (1) << 63) ? INT64_MIN : -(int64_t)v : (int64_t)v;
  return 1;
}
const mulib_archml_node_t *mulib_archml_root (const mulib_archml_t *a)
{ return a ? a->root : NULL; }
const char *mulib_archml_name (const mulib_archml_t *a)
{ return a ? mulib_archml_word_ (a->root, 2) : NULL; }
size_t mulib_archml_count (const mulib_archml_t *a, const char *kind)
{
  size_t count = 0;
  if (a && kind) for (size_t i = 0; i < a->record_count; ++i)
    if (!strcmp (mulib_archml_word_ (a->records[i], 0), kind)) ++count;
  return count;
}
const mulib_archml_node_t *mulib_archml_at (const mulib_archml_t *a, const char *kind, size_t index)
{
  if (a && kind) for (const mulib_archml_node_t *n = mulib_archml_child (a->root, 3); n; n = n->next)
    if (!strcmp (mulib_archml_word_ (n, 0), kind) && index-- == 0) return n;
  return NULL;
}
const mulib_archml_node_t *mulib_archml_find (const mulib_archml_t *a, const char *kind, const char *name)
{
  size_t lo = 0, hi;
  if (!a || !kind || !name) return NULL;
  hi = a->record_count;
  while (lo < hi) {
    size_t m = lo + (hi - lo) / 2;
    const mulib_archml_node_t *n = a->records[m];
    int c = strcmp (kind, mulib_archml_word_ (n, 0));
    if (!c) c = strcmp (name, mulib_archml_word_ (n, 1));
    if (!c) return n;
    if (c < 0) hi = m; else lo = m + 1;
  }
  return NULL;
}
static int mulib_archml_compare_ (const void *x, const void *y)
{
  const mulib_archml_node_t *a = *(const mulib_archml_node_t *const *)x;
  const mulib_archml_node_t *b = *(const mulib_archml_node_t *const *)y;
  int c = strcmp (mulib_archml_word_ (a, 0), mulib_archml_word_ (b, 0));
  return c ? c : strcmp (mulib_archml_word_ (a, 1), mulib_archml_word_ (b, 1));
}
void mulib_archml_destroy (mulib_archml_t *a)
{ if (a) { free (a->nodes); free (a->text); free (a->records); free (a); } }
static int mulib_archml_atom_ (unsigned char c, void *p)
{ (void)p; return c > 32 && c < 127 && c != '(' && c != ')' && c != '"' && c != ';' && c != '\\'; }
static int mulib_archml_space_ (unsigned char c, void *p)
{ (void)p; return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static int mulib_archml_string_ (unsigned char c, void *p)
{ (void)p; return c >= 32 && c != 127 && c != '"' && c != '\\'; }
static int mulib_archml_comment_ (unsigned char c, void *p)
{ (void)p; return c && c != '\n' && c != '\r'; }
static mulib_archml_options_t mulib_archml_options_ (const mulib_archml_options_t *o)
{
  mulib_archml_options_t r = {16u * 1024u * 1024u, 1000000, 128, 100000000};
  if (o) { if (o->max_source) r.max_source = o->max_source;
    if (o->max_nodes) r.max_nodes = o->max_nodes;
    if (o->max_depth) r.max_depth = o->max_depth;
    if (o->max_work) r.max_work = o->max_work; }
  return r;
}
static int mulib_archml_grammar_ (const char *s, size_t n,
    const mulib_archml_options_t *o, mulib_parsec_result_t *r)
{
  mulib_parsec_parser_t space, comment_byte, comment_parts[2], comment;
  mulib_parsec_parser_t skips[2], skip, ws, atom_byte, atom, quoted_byte;
  mulib_parsec_parser_t escapes[5], escape, escape_parts[2], chars[2], chars_choice;
  mulib_parsec_parser_t str_parts[3], string, list_parts[4], list;
  mulib_parsec_parser_t element, alternatives[3], member[2], members, root_parts[4], root;
  const mulib_parsec_parser_t *reference = &element;
  mulib_parsec_options_t limits;
  limits.max_depth = o->max_depth > (SIZE_MAX - 32) / 8 ? SIZE_MAX : o->max_depth * 8 + 32;
  limits.max_steps = o->max_work; limits.max_captures = o->max_nodes;
  space = mulib_parsec_satisfy (mulib_archml_space_, NULL, "whitespace");
  comment_byte = mulib_parsec_satisfy (mulib_archml_comment_, NULL, "comment byte");
  comment_parts[0] = mulib_parsec_byte (';');
  comment_parts[1] = mulib_parsec_repeat (&comment_byte, 0, SIZE_MAX);
  comment = mulib_parsec_sequence (comment_parts, 2);
  skips[0] = space; skips[1] = comment;
  skip = mulib_parsec_choice (skips, 2); ws = mulib_parsec_repeat (&skip, 0, SIZE_MAX);
  atom_byte = mulib_parsec_satisfy (mulib_archml_atom_, NULL, "atom");
  atom = mulib_parsec_repeat (&atom_byte, 1, SIZE_MAX);
  alternatives[0] = mulib_parsec_capture (&atom, MULIB_ARCHML_ATOM);
  quoted_byte = mulib_parsec_satisfy (mulib_archml_string_, NULL, "string byte");
  escapes[0] = mulib_parsec_byte ('"'); escapes[1] = mulib_parsec_byte ('\\');
  escapes[2] = mulib_parsec_byte ('n'); escapes[3] = mulib_parsec_byte ('r'); escapes[4] = mulib_parsec_byte ('t');
  escape = mulib_parsec_choice (escapes, 5);
  escape_parts[0] = mulib_parsec_byte ('\\'); escape_parts[1] = escape;
  chars[0] = mulib_parsec_sequence (escape_parts, 2); chars[1] = quoted_byte;
  chars_choice = mulib_parsec_choice (chars, 2);
  str_parts[0] = mulib_parsec_byte ('"'); str_parts[1] = mulib_parsec_repeat (&chars_choice, 0, SIZE_MAX);
  str_parts[2] = mulib_parsec_byte ('"'); string = mulib_parsec_sequence (str_parts, 3);
  alternatives[1] = mulib_parsec_capture (&string, MULIB_ARCHML_STRING);
  member[0] = mulib_parsec_ref (&reference); member[1] = ws;
  members = mulib_parsec_sequence (member, 2);
  list_parts[0] = mulib_parsec_byte ('('); list_parts[1] = ws;
  list_parts[2] = mulib_parsec_repeat (&members, 0, SIZE_MAX); list_parts[3] = mulib_parsec_byte (')');
  list = mulib_parsec_sequence (list_parts, 4);
  alternatives[2] = mulib_parsec_capture (&list, MULIB_ARCHML_LIST);
  element = mulib_parsec_choice (alternatives, 3);
  root_parts[0] = ws; root_parts[1] = element; root_parts[2] = ws; root_parts[3] = mulib_parsec_end ();
  root = mulib_parsec_sequence (root_parts, 4);
  return mulib_parsec_run_bounded (&root, s, n, &limits, o->max_work, r);
}
static const mulib_archml_node_t *mulib_archml_ref_ (const mulib_archml_t *a, const mulib_archml_node_t *n)
{
  return mulib_archml_is_ (mulib_archml_child (n, 0), "ref")
      && !mulib_archml_child (n, 3)
      ? mulib_archml_find (a, mulib_archml_word_ (n, 1), mulib_archml_word_ (n, 2)) : NULL;
}
int mulib_archml_register_info (const mulib_archml_t *a, const char *name, mulib_archml_register_t *out)
{
  const mulib_archml_node_t *n = mulib_archml_find (a, "register", name);
  mulib_archml_register_t r;
  memset (&r, 0, sizeof (r));
  if (!n || !out || !mulib_archml_number_ (n, "bits", &r.bits)
      || !mulib_archml_number_ (n, "offset", &r.offset)
      || !mulib_archml_number_ (n, "encoding", &r.encoding)) return 0;
  r.name = mulib_archml_word_ (n, 1);
  r.storage = mulib_archml_word_ (mulib_archml_property (n, "storage"), 1);
  r.write_policy = mulib_archml_word_ (mulib_archml_property (n, "write"), 1);
  r.dwarf = UINT64_MAX;
  (void)mulib_archml_number_ (n, "dwarf", &r.dwarf);
  if (!r.storage || !r.write_policy) return 0;
  *out = r; return 1;
}
int mulib_archml_register_overlap (const mulib_archml_t *a, const char *x, const char *y)
{
  mulib_archml_register_t r, s;
  if (!mulib_archml_register_info (a, x, &r) || !mulib_archml_register_info (a, y, &s)) return -1;
  return !strcmp (r.storage, s.storage) && r.offset < s.offset + s.bits && s.offset < r.offset + r.bits;
}
typedef struct mulib_archml_encoding_ {
  unsigned char base[MULIB_ARCHML_MAX_BYTES], mask[MULIB_ARCHML_MAX_BYTES];
  const mulib_archml_node_t *operands[MULIB_ARCHML_MAX_OPERANDS], *encoding;
  uint64_t bits[MULIB_ARCHML_MAX_OPERANDS], scales[MULIB_ARCHML_MAX_OPERANDS];
  unsigned sign[MULIB_ARCHML_MAX_OPERANDS];
  size_t length, count;
} mulib_archml_encoding_;
static uint64_t mulib_archml_mask_ (unsigned n)
{ return n == 64 ? UINT64_MAX : (UINT64_C (1) << n) - 1; }
static int mulib_archml_hex_ (unsigned char c)
{ return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static size_t mulib_archml_operand_ (const mulib_archml_encoding_ *e, const char *name)
{
  if (name) for (size_t i = 0; i < e->count; ++i)
    if (!strcmp (mulib_archml_word_ (e->operands[i], 0), name)) return i;
  return e->count;
}
static int mulib_archml_allowed_ (const mulib_archml_t *a, const mulib_archml_node_t *op, uint64_t v)
{
  const mulib_archml_node_t *p = mulib_archml_property (op, "exclude");
  for (p = mulib_archml_child (p, 1); p; p = p->next) {
    uint64_t x;
    if (mulib_archml_u64 (p, &x) && v == x) return 0;
  }
  if (mulib_archml_is_ (mulib_archml_child (mulib_archml_property (op, "kind"), 1), "register")) {
    const mulib_archml_node_t *cls = mulib_archml_ref_ (a, mulib_archml_child (mulib_archml_property (op, "class"), 1));
    for (p = mulib_archml_child (mulib_archml_property (cls, "members"), 1); p; p = p->next) {
      uint64_t encoding;
      const mulib_archml_node_t *reg = mulib_archml_ref_ (a, p);
      if (mulib_archml_number_ (reg, "encoding", &encoding) && encoding == v) return 1;
    }
    return 0;
  }
  return 1;
}
static int mulib_archml_encoding_read_ (const mulib_archml_node_t *n, mulib_archml_encoding_ *e)
{
  const char *base;
  uint64_t covered[MULIB_ARCHML_MAX_OPERANDS] = {0};
  memset (e, 0, sizeof (*e));
  e->encoding = mulib_archml_property (n, "encoding");
  base = mulib_archml_word_ (mulib_archml_property (e->encoding, "base"), 1);
  if (!base || !*base || strlen (base) % 2 || strlen (base) / 2 > MULIB_ARCHML_MAX_BYTES
      || !mulib_archml_property (n, "operands")) return 0;
  e->length = strlen (base) / 2;
  for (size_t i = 0; i < e->length; ++i) {
    int hi = mulib_archml_hex_ ((unsigned char)base[2*i]), lo = mulib_archml_hex_ ((unsigned char)base[2*i+1]);
    if (hi < 0 || lo < 0) return 0;
    e->base[i] = (unsigned char)(hi * 16 + lo); e->mask[i] = 255;
  }
  for (const mulib_archml_node_t *op = mulib_archml_child (mulib_archml_property (n, "operands"), 1); op; op = op->next) {
    size_t i = e->count;
    const char *sign = mulib_archml_word_ (mulib_archml_property (op, "signed"), 1);
    const char *name = mulib_archml_word_ (op, 0);
    if (i == MULIB_ARCHML_MAX_OPERANDS || !name || mulib_archml_operand_ (e, name) != i
        || !mulib_archml_number_ (op, "bits", &e->bits[i]) || !e->bits[i] || e->bits[i] > 64
        || !mulib_archml_number_ (op, "scale", &e->scales[i]) || !e->scales[i]
        || (e->scales[i] & (e->scales[i] - 1)) || !sign
        || (strcmp (sign, "true") && strcmp (sign, "false"))) return 0;
    e->sign[i] = !strcmp (sign, "true"); e->operands[i] = op; ++e->count;
    /* All represented byte offsets must fit a 64-bit operand bit pattern. */
    if (e->scales[i] - 1 > (UINT64_MAX >> (unsigned)(e->bits[i] - 1)) / 2) return 0;
  }
  for (const mulib_archml_node_t *f = mulib_archml_child (e->encoding, 1); f; f = f->next) {
    uint64_t dst, bits, src;
    size_t operand;
    if (mulib_archml_is_ (mulib_archml_child (f, 0), "base")) {
      if (f != mulib_archml_property (e->encoding, "base") || mulib_archml_child (f, 2)) return 0;
      continue;
    }
    operand = mulib_archml_operand_ (e, mulib_archml_word_ (f, 1));
    if (!mulib_archml_is_ (mulib_archml_child (f, 0), "field") || operand == e->count
        || !mulib_archml_u64 (mulib_archml_child (f, 2), &dst)
        || !mulib_archml_u64 (mulib_archml_child (f, 3), &bits)
        || !mulib_archml_u64 (mulib_archml_child (f, 4), &src) || mulib_archml_child (f, 5)
        || !bits || bits > 64 || dst >= e->length * 8 || bits > e->length * 8 - dst
        || src >= e->bits[operand] || bits > e->bits[operand] - src) return 0;
    for (unsigned j = 0; j < (unsigned)bits; ++j) {
      size_t b = (size_t)dst + j;
      unsigned char mask = (unsigned char)(1u << (b % 8));
      uint64_t bit = UINT64_C (1) << ((unsigned)src + j);
      if (!(e->mask[b/8] & mask) || (e->base[b/8] & mask) || (covered[operand] & bit)) return 0;
      e->mask[b/8] = (unsigned char)(e->mask[b/8] & ~mask); covered[operand] |= bit;
    }
  }
  for (size_t i = 0; i < e->count; ++i)
    if (covered[i] != mulib_archml_mask_ ((unsigned)e->bits[i])) return 0;
  return 1;
}
mulib_archml_result mulib_archml_encode (const mulib_archml_t *a, const char *name,
    const uint64_t *values, size_t count, void *output, size_t capacity, size_t *length)
{
  mulib_archml_encoding_ e;
  uint64_t v[MULIB_ARCHML_MAX_OPERANDS];
  unsigned char bytes[MULIB_ARCHML_MAX_BYTES];
  const mulib_archml_node_t *n = mulib_archml_find (a, "instruction", name);
  if (!n || !output || !length || (!values && count) || !mulib_archml_encoding_read_ (n, &e) || count != e.count)
    return MULIB_ARCHML_INVALID;
  if (capacity < e.length) return MULIB_ARCHML_LIMIT;
  for (size_t i = 0; i < count; ++i) {
    uint64_t mask = mulib_archml_mask_ ((unsigned)e.bits[i]);
    unsigned shift = 0;
    if ((values[i] & (e.scales[i] - 1)) || !mulib_archml_allowed_ (a, e.operands[i], values[i])) return MULIB_ARCHML_RANGE;
    while ((UINT64_C (1) << shift) < e.scales[i]) ++shift;
    v[i] = values[i] >> shift;
    if (e.sign[i] && (values[i] >> 63) && shift) v[i] |= UINT64_MAX << (64 - shift);
    if (e.sign[i]) {
      uint64_t extended = v[i] & mask;
      if (extended & (UINT64_C (1) << (e.bits[i] - 1))) extended |= ~mask;
      if (extended != v[i]) return MULIB_ARCHML_RANGE;
    } else if (v[i] > mask) return MULIB_ARCHML_RANGE;
  }
  memcpy (bytes, e.base, e.length);
  for (const mulib_archml_node_t *f = mulib_archml_child (e.encoding, 1); f; f = f->next) {
    uint64_t dst = 0, bits = 0, src = 0;
    size_t i;
    if (!mulib_archml_is_ (mulib_archml_child (f, 0), "field")) continue;
    i = mulib_archml_operand_ (&e, mulib_archml_word_ (f, 1));
    (void)mulib_archml_u64 (mulib_archml_child (f, 2), &dst);
    (void)mulib_archml_u64 (mulib_archml_child (f, 3), &bits);
    (void)mulib_archml_u64 (mulib_archml_child (f, 4), &src);
    for (unsigned j = 0; j < bits; ++j)
      if ((v[i] >> (src + j)) & 1) bytes[(dst+j)/8] |= (unsigned char)(1u << ((dst+j)%8));
  }
  memcpy (output, bytes, e.length); *length = e.length; return MULIB_ARCHML_OK;
}
mulib_archml_result mulib_archml_decode (const mulib_archml_t *a, const char *name,
    const void *input, size_t length, uint64_t *values, size_t count, size_t *consumed)
{
  mulib_archml_encoding_ e;
  uint64_t v[MULIB_ARCHML_MAX_OPERANDS] = {0};
  const unsigned char *bytes = (const unsigned char *)input;
  const mulib_archml_node_t *n = mulib_archml_find (a, "instruction", name);
  if (!n || (!input && length) || !consumed || (!values && count)
      || !mulib_archml_encoding_read_ (n, &e) || count != e.count) return MULIB_ARCHML_INVALID;
  if (length < e.length) return MULIB_ARCHML_LIMIT;
  for (size_t i = 0; i < e.length; ++i)
    if ((bytes[i] & e.mask[i]) != e.base[i]) return MULIB_ARCHML_MISMATCH;
  for (const mulib_archml_node_t *f = mulib_archml_child (e.encoding, 1); f; f = f->next) {
    uint64_t dst = 0, bits = 0, src = 0;
    size_t i;
    if (!mulib_archml_is_ (mulib_archml_child (f, 0), "field")) continue;
    i = mulib_archml_operand_ (&e, mulib_archml_word_ (f, 1));
    (void)mulib_archml_u64 (mulib_archml_child (f, 2), &dst);
    (void)mulib_archml_u64 (mulib_archml_child (f, 3), &bits);
    (void)mulib_archml_u64 (mulib_archml_child (f, 4), &src);
    for (unsigned j = 0; j < bits; ++j)
      if ((bytes[(dst+j)/8] >> ((dst+j)%8)) & 1u) v[i] |= UINT64_C (1) << (src+j);
  }
  for (size_t i = 0; i < count; ++i) {
    if (e.sign[i] && (v[i] & (UINT64_C (1) << (e.bits[i]-1)))) v[i] |= ~mulib_archml_mask_ ((unsigned)e.bits[i]);
    v[i] *= e.scales[i];
    if (!mulib_archml_allowed_ (a, e.operands[i], v[i])) return MULIB_ARCHML_MISMATCH;
  }
  if (count) memcpy (values, v, count * sizeof (*values));
  *consumed = e.length; return MULIB_ARCHML_OK;
}
static int mulib_archml_kind_ (const char *s)
{
  static const char *const kinds[] = {"target","storage","register","regclass","instruction",
    "feature","mode","memory","addressing","atomic","exception","vector","predication",
    "legalize","selection","schedule","resource","rewrite","layout","syntax","extension",
    "smt","abi","relocation","pcode-op","microarch","type","coverage"};
  for (size_t i = 0; i < sizeof (kinds)/sizeof (*kinds); ++i) if (!strcmp (s, kinds[i])) return 1;
  return !strncmp (s, "x-", 2) && s[2];
}
/* Kahn-style elimination over the feature DAG, bounded even for hostile cycles.
 * The record limit bounds storage; the validation work budget bounds rescans. */
static mulib_archml_result mulib_archml_features_ (const mulib_archml_t *a, size_t *work, mulib_archml_error_t *e)
{
  unsigned char *ready;
  size_t left = mulib_archml_count (a, "feature");
  if (!left) return MULIB_ARCHML_OK;
  ready = (unsigned char *)calloc (a->node_count, 1);
  if (!ready) return mulib_archml_error_ (e, MULIB_ARCHML_NOMEM, 0, "feature validation allocation failed");
  while (left) {
    size_t before = left;
    for (size_t i = 0; i < a->record_count; ++i) {
      const mulib_archml_node_t *n = a->records[i], *dep;
      int satisfied = 1;
      if (!(*work)--) { free (ready); return mulib_archml_error_ (e, MULIB_ARCHML_LIMIT, n->offset, "feature work limit"); }
      if (ready[n-a->nodes] || !mulib_archml_is_ (mulib_archml_child (n, 0), "feature")) continue;
      for (dep = mulib_archml_child (mulib_archml_property (n, "requires"), 1); dep; dep = dep->next) {
        const mulib_archml_node_t *f = mulib_archml_ref_ (a, dep);
        if (!f || !mulib_archml_is_ (mulib_archml_child (f, 0), "feature")) {
          free (ready); return mulib_archml_error_ (e, MULIB_ARCHML_SCHEMA, dep->offset, "requires must reference a feature");
        }
        if (!ready[f-a->nodes]) satisfied = 0;
      }
      if (satisfied) { ready[n-a->nodes] = 1; --left; }
    }
    if (before == left) { free (ready); return mulib_archml_error_ (e, MULIB_ARCHML_SCHEMA, 0, "cyclic feature requirements"); }
  }
  free (ready); return MULIB_ARCHML_OK;
}
static mulib_archml_result mulib_archml_validate_ (mulib_archml_t *a, size_t work, mulib_archml_error_t *e)
{
  const mulib_archml_node_t *n, *p;
  uint64_t version;
  size_t index = 0;
#define MULIB_ARCHML_BAD_(node, msg) return mulib_archml_error_ (e, MULIB_ARCHML_SCHEMA, (node)->offset, msg)
#define MULIB_ARCHML_WORK_(node) do { if (!work--) return mulib_archml_error_ (e, MULIB_ARCHML_LIMIT, (node)->offset, "validation work limit"); } while (0)
  if (!mulib_archml_is_ (mulib_archml_child (a->root, 0), "archml")
      || !mulib_archml_u64 (mulib_archml_child (a->root, 1), &version) || version != 1
      || !mulib_archml_word_ (a->root, 2) || !*mulib_archml_word_ (a->root, 2))
    MULIB_ARCHML_BAD_ (a->root, "expected (archml 1 name records...)");
  for (n = mulib_archml_child (a->root, 3); n; n = n->next) ++a->record_count;
  if (!a->record_count) MULIB_ARCHML_BAD_ (a->root, "missing target");
  if (a->record_count > SIZE_MAX / sizeof (*a->records)) return MULIB_ARCHML_LIMIT;
  a->records = (const mulib_archml_node_t **)malloc (a->record_count * sizeof (*a->records));
  if (!a->records) return mulib_archml_error_ (e, MULIB_ARCHML_NOMEM, 0, "record allocation failed");
  for (n = mulib_archml_child (a->root, 3); n; n = n->next) {
    const char *kind = mulib_archml_word_ (n, 0), *name = mulib_archml_word_ (n, 1);
    if (!kind || !mulib_archml_kind_ (kind) || !name || !*name) MULIB_ARCHML_BAD_ (n, "invalid record kind/name");
    a->records[index++] = n;
    for (p = mulib_archml_child (n, 2); p; p = p->next) {
      if (!mulib_archml_word_ (p, 0)) MULIB_ARCHML_BAD_ (p, "expected named property list");
      for (const mulib_archml_node_t *q = mulib_archml_child (n, 2); q != p; q = q->next) {
        MULIB_ARCHML_WORK_ (q);
        if (!strcmp (mulib_archml_word_ (p, 0), mulib_archml_word_ (q, 0))) MULIB_ARCHML_BAD_ (p, "duplicate property");
      }
    }
  }
  qsort (a->records, a->record_count, sizeof (*a->records), mulib_archml_compare_);
  for (size_t i = 1; i < a->record_count; ++i)
    if (!mulib_archml_compare_ (&a->records[i-1], &a->records[i])) MULIB_ARCHML_BAD_ (a->records[i], "duplicate record");
  if (mulib_archml_count (a, "target") != 1) MULIB_ARCHML_BAD_ (a->root, "exactly one target required");
  for (size_t i = 0; i < a->node_count; ++i) {
    n = &a->nodes[i]; MULIB_ARCHML_WORK_ (n);
    if (mulib_archml_is_ (mulib_archml_child (n, 0), "ref") && !mulib_archml_ref_ (a, n))
      MULIB_ARCHML_BAD_ (n, "unresolved or malformed reference");
  }
  for (size_t i = 0; i < a->record_count; ++i) {
    const char *kind;
    uint64_t bits;
    n = a->records[i]; kind = mulib_archml_word_ (n, 0); MULIB_ARCHML_WORK_ (n);
    for (p = mulib_archml_child (mulib_archml_property (n, "requires"), 1); p; p = p->next) {
      const mulib_archml_node_t *f = mulib_archml_ref_ (a, p);
      MULIB_ARCHML_WORK_ (p);
      if (!f || !mulib_archml_is_ (mulib_archml_child (f, 0), "feature")) MULIB_ARCHML_BAD_ (p, "requires must reference a feature");
    }
    if (!strcmp (kind, "target")) {
      const char *endian = mulib_archml_word_ (mulib_archml_property (n, "endian"), 1);
      if (!mulib_archml_number_ (n, "bits", &bits) || !bits || bits > 64
          || !endian || (strcmp (endian, "little") && strcmp (endian, "big"))) MULIB_ARCHML_BAD_ (n, "invalid target bits/endian");
    } else if (!strcmp (kind, "storage")) {
      if (!mulib_archml_number_ (n, "bits", &bits) || !bits) MULIB_ARCHML_BAD_ (n, "invalid storage width");
    } else if (!strcmp (kind, "register")) {
      mulib_archml_register_t r;
      const mulib_archml_node_t *storage;
      if (!mulib_archml_register_info (a, mulib_archml_word_ (n, 1), &r)) MULIB_ARCHML_BAD_ (n, "invalid register properties");
      storage = mulib_archml_find (a, "storage", r.storage);
      if (!storage || !mulib_archml_number_ (storage, "bits", &bits) || !r.bits || r.offset > bits || r.bits > bits-r.offset)
        MULIB_ARCHML_BAD_ (n, "register view exceeds storage");
      if (strcmp (r.write_policy, "preserve") && strcmp (r.write_policy, "zero-extend")
          && strcmp (r.write_policy, "discard") && strcmp (r.write_policy, "replace")) MULIB_ARCHML_BAD_ (n, "unknown register write policy");
    } else if (!strcmp (kind, "regclass")) {
      if (!mulib_archml_number_ (n, "bits", &bits) || !bits
          || !mulib_archml_child (mulib_archml_property (n, "members"), 1)) MULIB_ARCHML_BAD_ (n, "empty or invalid register class");
      for (p = mulib_archml_child (mulib_archml_property (n, "members"), 1); p; p = p->next) {
        const mulib_archml_node_t *reg = mulib_archml_ref_ (a, p);
        uint64_t width;
        if (!reg || !mulib_archml_is_ (mulib_archml_child (reg, 0), "register")
            || !mulib_archml_number_ (reg, "bits", &width) || width != bits) MULIB_ARCHML_BAD_ (p, "register class member width/kind mismatch");
      }
    } else if (!strcmp (kind, "instruction")) {
      mulib_archml_encoding_ enc;
      p = mulib_archml_property (n, "pcode");
      if (!mulib_archml_encoding_read_ (n, &enc)) MULIB_ARCHML_BAD_ (n, "invalid, overlapping or incomplete encoding");
      for (size_t k = 0; k < enc.count; ++k) {
        const mulib_archml_node_t *op = enc.operands[k], *cls;
        const char *type = mulib_archml_word_ (mulib_archml_property (op, "kind"), 1);
        const char *access = mulib_archml_word_ (mulib_archml_property (op, "access"), 1);
        if (!type || (strcmp (type, "register") && strcmp (type, "immediate")) || !access
            || (strcmp (access, "read") && strcmp (access, "write") && strcmp (access, "read-write"))) MULIB_ARCHML_BAD_ (op, "invalid operand kind/access");
        for (const mulib_archml_node_t *x = mulib_archml_child (op, 1); x; x = x->next) {
          if (!mulib_archml_word_ (x, 0)) MULIB_ARCHML_BAD_ (x, "invalid operand property");
          for (const mulib_archml_node_t *y = mulib_archml_child (op, 1); y != x; y = y->next) {
            MULIB_ARCHML_WORK_ (y);
            if (!strcmp (mulib_archml_word_ (x, 0), mulib_archml_word_ (y, 0))) MULIB_ARCHML_BAD_ (x, "duplicate operand property");
          }
        }
        if (!strcmp (type, "register")) {
          cls = mulib_archml_ref_ (a, mulib_archml_child (mulib_archml_property (op, "class"), 1));
          if (!cls || !mulib_archml_is_ (mulib_archml_child (cls, 0), "regclass")
              || enc.scales[k] != 1 || enc.sign[k]) MULIB_ARCHML_BAD_ (op, "invalid register operand class/scale/sign");
        }
        for (const mulib_archml_node_t *x = mulib_archml_child (mulib_archml_property (op, "exclude"), 1); x; x = x->next)
          if (!mulib_archml_u64 (x, &bits)) MULIB_ARCHML_BAD_ (x, "invalid excluded operand value");
      }
      if (!p || !mulib_archml_child (p, 1) || mulib_archml_child (p, 1)->kind != MULIB_ARCHML_LIST
          || mulib_archml_child (p, 2)) MULIB_ARCHML_BAD_ (n, "instruction needs one P-code expression");
      /* Captures are postorder: the expression occupies one contiguous interval.
       * Inspect lists without a native recursive traversal. */
      for (size_t j = p->subtree_first; j < (size_t)(p - a->nodes); ++j) {
        const mulib_archml_node_t *expr = &a->nodes[j], *op;
        uint64_t min, max;
        size_t argc = 0;
        MULIB_ARCHML_WORK_ (expr);
        if (expr->kind != MULIB_ARCHML_LIST) continue;
        op = mulib_archml_find (a, "pcode-op", mulib_archml_word_ (expr, 0));
        if (!op) MULIB_ARCHML_BAD_ (expr, "undeclared P-code operator");
        for (const mulib_archml_node_t *arg = mulib_archml_child (expr, 1); arg; arg = arg->next) ++argc;
        if (!mulib_archml_u64 (mulib_archml_child (mulib_archml_property (op, "arity"), 1), &min)
            || !mulib_archml_u64 (mulib_archml_child (mulib_archml_property (op, "arity"), 2), &max)
            || argc < min || argc > max) MULIB_ARCHML_BAD_ (expr, "P-code operator arity mismatch");
        if (mulib_archml_is_ (mulib_archml_child (expr, 0), "read") || mulib_archml_is_ (mulib_archml_child (expr, 0), "write")) {
          size_t operand = mulib_archml_operand_ (&enc, mulib_archml_word_ (expr, 1));
          const char *access;
          int writing = mulib_archml_is_ (mulib_archml_child (expr, 0), "write");
          if (operand == enc.count) MULIB_ARCHML_BAD_ (expr, "unknown P-code operand");
          access = mulib_archml_word_ (mulib_archml_property (enc.operands[operand], "access"), 1);
          if (!strcmp (access, writing ? "read" : "write")) MULIB_ARCHML_BAD_ (expr, "P-code violates operand access");
          if (writing && !mulib_archml_property (enc.operands[operand], "class")) MULIB_ARCHML_BAD_ (expr, "cannot write an immediate operand");
        }
        if (mulib_archml_is_ (mulib_archml_child (expr, 0), "reg") || mulib_archml_is_ (mulib_archml_child (expr, 0), "set-reg"))
          if (!mulib_archml_find (a, "register", mulib_archml_word_ (expr, 1))) MULIB_ARCHML_BAD_ (expr, "unknown implicit P-code register");
      }
    } else if (!strcmp (kind, "pcode-op")) {
      uint64_t max;
      p = mulib_archml_property (n, "arity");
      if (!mulib_archml_u64 (mulib_archml_child (p, 1), &bits)
          || !mulib_archml_u64 (mulib_archml_child (p, 2), &max) || bits > max || max > 1024
          || mulib_archml_child (p, 3) || !mulib_archml_word_ (mulib_archml_property (n, "semantics"), 1))
        MULIB_ARCHML_BAD_ (n, "invalid P-code operator contract");
    }
  }
#undef MULIB_ARCHML_BAD_
#undef MULIB_ARCHML_WORK_
  return mulib_archml_features_ (a, &work, e);
}
mulib_archml_result mulib_archml_parse (const char *s, size_t n,
    const mulib_archml_options_t *options, mulib_archml_t **out, mulib_archml_error_t *error)
{
  mulib_archml_options_t o = mulib_archml_options_ (options);
  mulib_parsec_result_t parsed;
  mulib_archml_t *a = NULL;
  mulib_archml_error_t e;
  mulib_archml_result rc;
  size_t at = 0, *depth = NULL;
  int status;
  memset (&e, 0, sizeof (e)); e.code = MULIB_ARCHML_OK;
  if (!out || (!s && n) || n == SIZE_MAX) { rc = mulib_archml_error_ (&e, MULIB_ARCHML_INVALID, 0, "invalid arguments"); goto done; }
  if (n > o.max_source) { rc = mulib_archml_error_ (&e, MULIB_ARCHML_LIMIT, 0, "source limit"); goto done; }
  status = mulib_archml_grammar_ (s, n, &o, &parsed);
  if (status != 1) {
    rc = mulib_archml_error_ (&e, parsed.error.code == MULIB_PARSEC_MEMORY ? MULIB_ARCHML_NOMEM
      : parsed.error.code == MULIB_PARSEC_LIMIT ? MULIB_ARCHML_LIMIT : MULIB_ARCHML_SYNTAX,
      parsed.error.offset, parsed.error.expected);
    mulib_parsec_result_free (&parsed); goto done;
  }
  rc = MULIB_ARCHML_NOMEM;
  a = (mulib_archml_t *)calloc (1, sizeof (*a));
  if (!a) goto memory;
  if (parsed.count > SIZE_MAX / sizeof (*a->nodes) || parsed.count > SIZE_MAX / sizeof (*depth)) { rc = MULIB_ARCHML_LIMIT; goto memory; }
  a->nodes = (mulib_archml_node_t *)calloc (parsed.count, sizeof (*a->nodes));
  a->text = (char *)malloc (n + 1);
  depth = (size_t *)calloc (parsed.count, sizeof (*depth));
  if (!a->nodes || !a->text || !depth) goto memory;
  a->node_count = parsed.count;
  for (size_t i = 0; i < parsed.count; ++i) {
    const mulib_parsec_capture_t *c = &parsed.captures[i];
    mulib_archml_node_t *node = &a->nodes[i];
    node->kind = (mulib_archml_kind)c->tag; node->offset = c->offset; node->length = c->length;
    node->subtree_first = c->first;
    if (node->kind == MULIB_ARCHML_LIST) {
      size_t j = i;
      depth[i] = 1;
      while (j > c->first) {
        mulib_archml_node_t *child = &a->nodes[j-1];
        child->next = node->child; node->child = child;
        if (depth[j-1] + 1 > depth[i]) depth[i] = depth[j-1] + 1;
        j = parsed.captures[j-1].first;
      }
      if (depth[i] > o.max_depth) { rc = MULIB_ARCHML_LIMIT; goto memory; }
    } else {
      size_t start = c->offset, end = start + c->length;
      node->text = a->text + at;
      if (node->kind == MULIB_ARCHML_STRING) { ++start; --end; }
      while (start < end) {
        char ch = s[start++];
        if (ch == '\\') { ch = s[start++]; if (ch == 'n') ch = '\n'; else if (ch == 'r') ch = '\r'; else if (ch == 't') ch = '\t'; }
        a->text[at++] = ch;
      }
      a->text[at++] = 0;
    }
  }
  a->root = &a->nodes[a->node_count-1];
  free (depth); depth = NULL;
  mulib_parsec_result_free (&parsed);
  rc = mulib_archml_validate_ (a, o.max_work, &e);
  if (rc == MULIB_ARCHML_OK) { *out = a; a = NULL; }
  goto done;
memory:
  mulib_archml_error_ (&e, rc, 0, rc == MULIB_ARCHML_NOMEM ? "out of memory" : "tree resource limit");
  free (depth); mulib_parsec_result_free (&parsed);
done:
  mulib_archml_destroy (a);
  if (rc != MULIB_ARCHML_OK && s) for (size_t i = 0; i < e.offset && i < n; ++i) {
    if (s[i] == '\n') { ++e.line; e.column = 1; } else ++e.column;
  }
  if (error) *error = e;
  return rc;
}
mulib_archml_result mulib_archml_load (const char *path, const mulib_archml_options_t *options,
    mulib_archml_t **out, mulib_archml_error_t *e)
{
  mulib_archml_options_t o = mulib_archml_options_ (options);
  FILE *f;
  char *data = NULL;
  size_t size = 0, capacity = 0;
  mulib_archml_result rc = MULIB_ARCHML_OK;
  if (!path || !out) return mulib_archml_error_ (e, MULIB_ARCHML_INVALID, 0, "invalid load arguments");
  f = fopen (path, "rb");
  if (!f) return mulib_archml_error_ (e, MULIB_ARCHML_IO, 0, "cannot open architecture");
  for (;;) {
    int c = fgetc (f);
    if (c == EOF) break;
    if (size >= o.max_source || size == SIZE_MAX - 1) { rc = MULIB_ARCHML_LIMIT; break; }
    if (size == capacity) {
      size_t cap = capacity > SIZE_MAX / 2 ? SIZE_MAX : capacity ? capacity * 2 : 4096;
      char *next;
      if (cap > o.max_source) cap = o.max_source;
      next = (char *)realloc (data, cap);
      if (!next) { rc = MULIB_ARCHML_NOMEM; break; }
      data = next; capacity = cap;
    }
    data[size++] = (char)c;
  }
  if (ferror (f)) rc = MULIB_ARCHML_IO;
  if (fclose (f)) rc = MULIB_ARCHML_IO;
  if (rc == MULIB_ARCHML_OK) rc = mulib_archml_parse (data, size, options, out, e);
  else mulib_archml_error_ (e, rc, size, "architecture read failed or exceeded limit");
  free (data); return rc;
}
int mulib_archml_enabled (const mulib_archml_t *a, const mulib_archml_node_t *n,
    const char *const *features, size_t count)
{
  const mulib_archml_node_t *stack[128];
  size_t depth = 0, work = 0;
  if (!a || !n || (!features && count)) return -1;
  for (size_t i = 0; i < count; ++i) if (!mulib_archml_find (a, "feature", features[i])) return -1;
  stack[depth++] = n;
  while (depth) {
    const mulib_archml_node_t *item = stack[--depth];
    if (++work > 100000) return -1;
    if (mulib_archml_is_ (mulib_archml_child (item, 0), "feature")) {
      size_t i;
      for (i = 0; i < count; ++i) if (!strcmp (features[i], mulib_archml_word_ (item, 1))) break;
      if (i == count) return 0;
    }
    for (const mulib_archml_node_t *r = mulib_archml_child (mulib_archml_property (item, "requires"), 1); r; r = r->next) {
      const mulib_archml_node_t *f = mulib_archml_ref_ (a, r);
      if (!f || !mulib_archml_is_ (mulib_archml_child (f, 0), "feature") || depth == 128) return -1;
      stack[depth++] = f;
    }
  }
  return 1;
}
#ifdef __cplusplus
}
#endif
#endif
