/* Private wire format shared by the TermScript compiler and C11 runtime.
 * Version 1: eight magic/version octets, then u64 node count, pool length,
 * and root. Each 49-octet node is a kind byte, u64 offset, length, first,
 * next, parent, and a signed 64-bit two's-complement value. All integers are
 * little-endian; UINT64_MAX is the absent index. The packed byte pool follows
 * the last node. Parent indexes let the decoder verify preorder without a
 * recursive walk or attacker-sized scratch storage. */
#ifndef MULIB_TERMSCRIPT_CAPSULE_PRIVATE_
#define MULIB_TERMSCRIPT_CAPSULE_PRIVATE_
#include <limits.h>

enum {
  MULIB_TSCAP_HEADER_ = 32, MULIB_TSCAP_NODE_ = 49,
  MULIB_TSCAP_KIND_ = 0, MULIB_TSCAP_OFFSET_ = 1, MULIB_TSCAP_LENGTH_ = 9,
  MULIB_TSCAP_FIRST_ = 17, MULIB_TSCAP_NEXT_ = 25, MULIB_TSCAP_PARENT_ = 33,
  MULIB_TSCAP_INTEGER_ = 41
};
#define MULIB_TSCAP_MAX_NODES_ UINT64_C(65536)
#define MULIB_TSCAP_MAX_POOL_ UINT64_C(2097152)

static const unsigned char mulib_tscap_magic_[8] = {
  0x54, 0x53, 0x41, 0x53, 0x54, 1, 0, 0
};

/* Wire IDs do not depend on the C enum's layout or ordering. */
static const mulib_termscript_kind_t mulib_tscap_kinds_[] = {
  MULIB_TERMSCRIPT_WORD, MULIB_TERMSCRIPT_INTEGER, MULIB_TERMSCRIPT_STRING,
  MULIB_TERMSCRIPT_COMMAND, MULIB_TERMSCRIPT_CHARACTER, MULIB_TERMSCRIPT_SYMBOL,
  MULIB_TERMSCRIPT_ARRAY_SYMBOL, MULIB_TERMSCRIPT_RECORD_SYMBOL, MULIB_TERMSCRIPT_PARAMETER,
  MULIB_TERMSCRIPT_KEY, MULIB_TERMSCRIPT_BLOCK, MULIB_TERMSCRIPT_TUPLE,
  MULIB_TERMSCRIPT_ARRAY, MULIB_TERMSCRIPT_RECORD, MULIB_TERMSCRIPT_FIELD,
  MULIB_TERMSCRIPT_DIRECTIVE, MULIB_TERMSCRIPT_ESCAPE
};

static inline int
mulib_tscap_wire_kind_(mulib_termscript_kind_t kind)
{
  for (size_t i = 0; i < sizeof mulib_tscap_kinds_ / sizeof *mulib_tscap_kinds_; ++i)
    if (mulib_tscap_kinds_[i] == kind) return (int)i;
  return -1;
}

static inline int
mulib_tscap_children_(mulib_termscript_kind_t kind)
{
  return kind == MULIB_TERMSCRIPT_BLOCK || kind == MULIB_TERMSCRIPT_TUPLE
    || kind == MULIB_TERMSCRIPT_ARRAY || kind == MULIB_TERMSCRIPT_RECORD
    || kind == MULIB_TERMSCRIPT_FIELD;
}

static inline int
mulib_tscap_text_(mulib_termscript_kind_t kind)
{
  return (!mulib_tscap_children_(kind) || kind == MULIB_TERMSCRIPT_FIELD)
    && kind != MULIB_TERMSCRIPT_INTEGER && kind != MULIB_TERMSCRIPT_CHARACTER;
}

static inline uint64_t
mulib_tscap_read_(const unsigned char *bytes)
{
  uint64_t value = 0;
  for (unsigned i = 8; i; --i)
    value = (value << 8) | bytes[i - 1];
  return value;
}

static inline void
mulib_tscap_write_(unsigned char *bytes, uint64_t value)
{
  for (unsigned i = 0; i < 8; ++i) {
    bytes[i] = (unsigned char)(value & 255u);
    value >>= 8;
  }
}

static inline int64_t
mulib_tscap_int_(uint64_t value)
{
  return value <= (uint64_t)INT64_MAX ? (int64_t)value
    : -1 - (int64_t)(UINT64_MAX - value);
}

static inline uint64_t
mulib_tscap_link_(size_t index)
{
  return index == SIZE_MAX ? UINT64_MAX : (uint64_t)index;
}

static inline size_t
mulib_tscap_index_(uint64_t value)
{
  /* Only use after verifying the value is an absent or a valid index. */
  return value == UINT64_MAX ? SIZE_MAX : (size_t)value;
}

static inline const unsigned char *
mulib_tscap_row_(const unsigned char *bytes, size_t index)
{
  return bytes + MULIB_TSCAP_HEADER_ + index * MULIB_TSCAP_NODE_;
}

static inline int
mulib_tscap_size_(uint64_t count, uint64_t pool, size_t *size)
{
  if (count > MULIB_TSCAP_MAX_NODES_ || pool > MULIB_TSCAP_MAX_POOL_
      || count > (SIZE_MAX - MULIB_TSCAP_HEADER_) / MULIB_TSCAP_NODE_)
    return 0;
  size_t rows = MULIB_TSCAP_HEADER_ + (size_t)count * MULIB_TSCAP_NODE_;
  if (pool > SIZE_MAX - rows) return 0;
  *size = rows + (size_t)pool;
  return 1;
}

static inline int
mulib_tscap_hex_(unsigned char c)
{
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static inline int
mulib_tscap_utf8_(const unsigned char *s, size_t n)
{
  for (size_t i = 0; i < n;) {
    unsigned first = s[i++], lo = 0x80, hi = 0xbf;
    size_t extra;
    if (first < 0x80) continue;
    if (first >= 0xc2 && first <= 0xdf) extra = 1;
    else if (first >= 0xe0 && first <= 0xef) {
      extra = 2;
      if (first == 0xe0) lo = 0xa0; /* No overlong forms. */
      if (first == 0xed) hi = 0x9f; /* No surrogate code points. */
    } else if (first >= 0xf0 && first <= 0xf4) {
      extra = 3;
      if (first == 0xf0) lo = 0x90;
      if (first == 0xf4) hi = 0x8f; /* No values above U+10FFFF. */
    } else return 0;
    if (extra > n - i || s[i] < lo || s[i] > hi) return 0;
    ++i;
    while (--extra) {
      if (s[i] < 0x80 || s[i] > 0xbf) return 0;
      ++i;
    }
  }
  return 1;
}

static inline int
mulib_tscap_term_sep_(unsigned char c)
{
  return mulib_termscript_space_(c) || c == ',' || c == '|';
}

static inline int
mulib_tscap_term_valid_(const char *text, size_t n)
{
  size_t count = 0;
  for (size_t i = 0; i < n;) {
    while (i < n && mulib_tscap_term_sep_((unsigned char)text[i])) ++i;
    if (i == n) break;
    ++count;
    if (text[i] == '"') {
      do { ++i; } while (i < n && text[i] != '"');
      if (i == n || (++i < n && !mulib_tscap_term_sep_((unsigned char)text[i]))) return 0;
    } else {
      do { ++i; } while (i < n && !mulib_tscap_term_sep_((unsigned char)text[i]));
    }
  }
  return count != 0;
}

static inline int
mulib_tscap_span_(mulib_termscript_kind_t kind, const unsigned char *text, size_t n,
                  uint64_t integer)
{
  if (kind == MULIB_TERMSCRIPT_INTEGER) return n == 0;
  if (kind == MULIB_TERMSCRIPT_CHARACTER)
    return n == 0 && integer <= 0x10ffff && !(integer >= 0xd800 && integer <= 0xdfff);
  if (integer) return 0;
  if (!mulib_tscap_text_(kind)) return n == 0;
  if (!mulib_tscap_utf8_(text, n)) return 0;
  if (kind == MULIB_TERMSCRIPT_STRING || kind == MULIB_TERMSCRIPT_COMMAND) {
    unsigned char quote = kind == MULIB_TERMSCRIPT_STRING ? '"' : '`';
    for (size_t i = 0; i < n; ++i) {
      if (!text[i] || text[i] == quote) return 0;
      if (text[i] != '\\') continue;
      if (++i == n || !text[i]) return 0;
      if (text[i] == 'x') {
        if (n - i < 3 || !mulib_tscap_hex_(text[i + 1]) || !mulib_tscap_hex_(text[i + 2]))
          return 0;
        i += 2;
      } else if (!strchr("\\\"`abefnrtv0", text[i])) return 0;
    }
    return 1;
  }
  if (!n || memchr(text, 0, n)) return 0;
  if (kind == MULIB_TERMSCRIPT_ESCAPE) {
    if (n > MULIB_TERMSCRIPT_BRACKET_NAME_LIMIT_) return 0;
    for (size_t i = 0; i < n; ++i)
      if (!mulib_termscript_bracket_name_byte_(text[i])) return 0;
  } else if (kind == MULIB_TERMSCRIPT_DIRECTIVE) {
    if (text[0] != '%' || memchr(text, '\n', n) || memchr(text, '\r', n)) return 0;
  } else {
    for (size_t i = 0; i < n; ++i) {
      unsigned char c = text[i];
      if (c <= ' ' || c == 0x7f || strchr("{}()[]<>,;\"`#@%$!\\", c)
          || (kind == MULIB_TERMSCRIPT_FIELD && c == ':')) return 0;
    }
  }
  return 1;
}

static inline int
mulib_tscap_controls_(const unsigned char *bytes, const unsigned char *pool,
                      uint64_t index, size_t *bad)
{
  unsigned char has_else[128];
  size_t depth = 0;
  int previous_if = 0, after_if_block = 0;
  while (index != UINT64_MAX) {
    const unsigned char *row = mulib_tscap_row_(bytes, (size_t)index);
    mulib_termscript_kind_t kind = mulib_tscap_kinds_[row[MULIB_TSCAP_KIND_]];
    const unsigned char *text = pool + (size_t)mulib_tscap_read_(row + MULIB_TSCAP_OFFSET_);
    size_t n = (size_t)mulib_tscap_read_(row + MULIB_TSCAP_LENGTH_);
    uint64_t next = mulib_tscap_read_(row + MULIB_TSCAP_NEXT_);
    *bad = MULIB_TSCAP_HEADER_ + (size_t)index * MULIB_TSCAP_NODE_;
    if (kind == MULIB_TERMSCRIPT_DIRECTIVE) {
      if (n >= 9 && !memcmp(text, "%if-term:", 9)) {
        if (depth == sizeof has_else || !mulib_tscap_term_valid_((const char *)text + 9, n - 9))
          return 0;
        has_else[depth++] = 0;
      } else if (n >= 6 && !memcmp(text, "%else:", 6)) {
        if (!depth || has_else[depth - 1]) return 0;
        has_else[depth - 1] = 1;
      } else if (n >= 5 && !memcmp(text, "%end:", 5)) {
        if (!depth) return 0;
        --depth;
      } else if (n < 8 || memcmp(text, "%module:", 8)) return 0;
    }
    if (after_if_block && kind == MULIB_TERMSCRIPT_WORD && n == 4 && !memcmp(text, "else", 4)
        && (next == UINT64_MAX ||
            mulib_tscap_kinds_[mulib_tscap_row_(bytes, (size_t)next)[MULIB_TSCAP_KIND_]]
              != MULIB_TERMSCRIPT_BLOCK)) return 0;
    after_if_block = previous_if && kind == MULIB_TERMSCRIPT_BLOCK;
    previous_if = kind == MULIB_TERMSCRIPT_WORD && n == 2 && !memcmp(text, "if", 2);
    index = next;
  }
  return depth == 0;
}

static inline int
mulib_tscap_validate_(const unsigned char *bytes, size_t length, size_t *count_out,
                     size_t *pool_out, size_t *bad)
{
  *bad = 0;
  if (!bytes || length < MULIB_TSCAP_HEADER_ ||
      memcmp(bytes, mulib_tscap_magic_, sizeof mulib_tscap_magic_)) return 0;
#if UCHAR_MAX > 255
  for (size_t i = 0; i < MULIB_TSCAP_HEADER_; ++i)
    if (bytes[i] > 255) return 0;
#endif
  uint64_t count64 = mulib_tscap_read_(bytes + 8), pool64 = mulib_tscap_read_(bytes + 16);
  size_t exact;
  if (!mulib_tscap_size_(count64, pool64, &exact) || exact != length ||
      mulib_tscap_read_(bytes + 24) != (count64 ? 0 : UINT64_MAX)) return 0;
#if UCHAR_MAX > 255
  for (size_t i = MULIB_TSCAP_HEADER_; i < length; ++i)
    if (bytes[i] > 255) { *bad = i; return 0; }
#endif
  size_t count = (size_t)count64, pool_length = (size_t)pool64, used = 0;
  const unsigned char *pool = mulib_tscap_row_(bytes, count);
  uint64_t parent = UINT64_MAX;
  for (size_t i = 0; i < count; ++i) {
    const unsigned char *node = mulib_tscap_row_(bytes, i);
    uint64_t span = mulib_tscap_read_(node + MULIB_TSCAP_LENGTH_);
    uint64_t first = mulib_tscap_read_(node + MULIB_TSCAP_FIRST_);
    uint64_t next = mulib_tscap_read_(node + MULIB_TSCAP_NEXT_);
    unsigned k = node[MULIB_TSCAP_KIND_];
    *bad = MULIB_TSCAP_HEADER_ + i * MULIB_TSCAP_NODE_;
    if (k >= sizeof mulib_tscap_kinds_ / sizeof *mulib_tscap_kinds_
        || mulib_tscap_read_(node + MULIB_TSCAP_OFFSET_) != used || span > pool_length - used
        || mulib_tscap_read_(node + MULIB_TSCAP_PARENT_) != parent
        || (first != UINT64_MAX && (first != i + 1 || first >= count))
        || (next != UINT64_MAX && (next <= i || next >= count))) return 0;
    mulib_termscript_kind_t kind = mulib_tscap_kinds_[k];
    mulib_termscript_kind_t parent_kind = parent == UINT64_MAX ? MULIB_TERMSCRIPT_BLOCK
      : mulib_tscap_kinds_[mulib_tscap_row_(bytes, (size_t)parent)[MULIB_TSCAP_KIND_]];
    if ((first != UINT64_MAX && !mulib_tscap_children_(kind))
        || (kind == MULIB_TERMSCRIPT_FIELD) != (parent_kind == MULIB_TERMSCRIPT_RECORD)
        || (kind == MULIB_TERMSCRIPT_FIELD && first == UINT64_MAX)
        || (parent_kind == MULIB_TERMSCRIPT_FIELD && next != UINT64_MAX)) return 0;
    if (!mulib_tscap_span_(kind, pool + used, (size_t)span,
                          mulib_tscap_read_(node + MULIB_TSCAP_INTEGER_))) {
      *bad = length - pool_length + used;
      return 0;
    }
    used += (size_t)span;
    if (first != UINT64_MAX) {
      parent = i;
      continue;
    }
    while (next == UINT64_MAX && parent != UINT64_MAX) {
      const unsigned char *ancestor = mulib_tscap_row_(bytes, (size_t)parent);
      next = mulib_tscap_read_(ancestor + MULIB_TSCAP_NEXT_);
      parent = mulib_tscap_read_(ancestor + MULIB_TSCAP_PARENT_);
    }
    if (next != (i + 1 == count ? UINT64_MAX : (uint64_t)(i + 1))) return 0;
  }
  if (used != pool_length) return 0;
  /* The previous pass established that each sibling list is a disjoint
   * valid forward chain, so checking each list remains linear. */
  if (count && !mulib_tscap_controls_(bytes, pool, 0, bad)) return 0;
  for (size_t i = 0; i < count; ++i) {
    uint64_t first = mulib_tscap_read_(mulib_tscap_row_(bytes, i) + MULIB_TSCAP_FIRST_);
    if (first != UINT64_MAX && !mulib_tscap_controls_(bytes, pool, first, bad)) return 0;
  }
  *count_out = count;
  *pool_out = pool_length;
  return 1;
}
#endif
