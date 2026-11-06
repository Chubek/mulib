/**
 * @file mulib-termsyn.h
 * @brief Stateful incremental editor-buffer lexing, CSS themes and terminal output.
 * Define TERMSYN and EDBUF/TERMSTYLE with all TERMSTYLE dependencies in one TU.
 * Built-in C/C++, Python, JSON and shell lexers are lexical colorizers, not
 * language parsers. Custom line lexers can implement arbitrary state machines.
 */
#ifndef MULIB_TERMSYN_H
#define MULIB_TERMSYN_H
#include "mulib-edbuf.h"
#include "mulib-termstyle.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_termsyn mulib_termsyn_t;
typedef enum mulib_termsyn_language {
  MULIB_TERMSYN_PLAIN, MULIB_TERMSYN_C, MULIB_TERMSYN_CPP,
  MULIB_TERMSYN_PYTHON, MULIB_TERMSYN_JSON, MULIB_TERMSYN_SHELL
} mulib_termsyn_language_t;
typedef enum mulib_termsyn_token {
  MULIB_TERMSYN_TEXT, MULIB_TERMSYN_KEYWORD, MULIB_TERMSYN_TYPE,
  MULIB_TERMSYN_STRING, MULIB_TERMSYN_NUMBER, MULIB_TERMSYN_COMMENT,
  MULIB_TERMSYN_OPERATOR, MULIB_TERMSYN_PUNCTUATION, MULIB_TERMSYN_PREPROCESSOR,
  MULIB_TERMSYN_CONSTANT, MULIB_TERMSYN_IDENTIFIER, MULIB_TERMSYN_ERROR,
  MULIB_TERMSYN_TOKEN_COUNT
} mulib_termsyn_token_t;
typedef struct mulib_termsyn_span {
  size_t offset, length;
  mulib_termsyn_token_t token;
} mulib_termsyn_span_t;
enum mulib_termsyn_result {
  MULIB_TERMSYN_OK = 0, MULIB_TERMSYN_EINVAL = -1, MULIB_TERMSYN_ENOMEM = -2,
  MULIB_TERMSYN_ERANGE = -3, MULIB_TERMSYN_ENOTFOUND = -4, MULIB_TERMSYN_ELEXER = -5
};
/** @brief Emit ordered, nonoverlapping, nonempty line-relative spans; 1/0.
 * Gaps are automatically TEXT. A failed emit must cause the lexer to fail. */
typedef int (*mulib_termsyn_emit_fn)(void *, size_t offset, size_t length, mulib_termsyn_token_t);
/** @brief Lex one line including its LF (last line may be empty). Return 1/0;
 * state starts at zero and next_state feeds the next line. Must be deterministic
 * for (bytes,state,user); user data is borrowed and must remain unchanged. */
typedef int (*mulib_termsyn_lexer_fn)(const char *, size_t length, uint64_t state,
                                     mulib_termsyn_emit_fn, void *emit_context,
                                     uint64_t *next_state, void *user);
/** @brief Create a built-in lexer cache; NULL on invalid language/OOM. */
mulib_termsyn_t *mulib_termsyn_create(mulib_termsyn_language_t);
/** @brief Create a custom lexer cache; callback/user must outlive the cache. */
mulib_termsyn_t *mulib_termsyn_create_custom(mulib_termsyn_lexer_fn, void *user);
/** @brief Release cached text/tokens. NULL harmless. */
void mulib_termsyn_destroy(mulib_termsyn_t *);
/** @brief Atomically snapshot and update a buffer. Reuse unchanged prefix and
 * suffix lines when lexical entry states match; propagate edits until states
 * converge. Snapshot/line indexing is O(bytes); only affected lines are lexed.
 * No concurrent buffer edits. Buffer must outlive its association with cache. */
int mulib_termsyn_update(mulib_termsyn_t *, const mulib_edbuf_t *);
/** @brief Number of lines actually lexed by the last successful update. */
size_t mulib_termsyn_lexed_lines(const mulib_termsyn_t *);
/** @brief Revision represented by the cache, zero before update. */
uint64_t mulib_termsyn_revision(const mulib_termsyn_t *);
/** @brief Total spans, with complete byte coverage (empty lines have none). */
size_t mulib_termsyn_span_count(const mulib_termsyn_t *);
/** @brief Copy indexed absolute span; failure leaves output unchanged. */
int mulib_termsyn_span(const mulib_termsyn_t *, size_t index, mulib_termsyn_span_t *);
/** @brief Find the span covering a byte; EOF returns ENOTFOUND. */
int mulib_termsyn_at(const mulib_termsyn_t *, size_t offset, mulib_termsyn_span_t *);
/** @brief Stable CSS class for a token, or NULL for an invalid token. */
const char *mulib_termsyn_token_name(mulib_termsyn_token_t);
/** @brief Fill a default RGB theme, directly usable by vterm/TUI/curses. */
void mulib_termsyn_default_theme(mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT]);
/** @brief Resolve CSS on <code class="syntax LANGUAGE"><span class="TOKEN"/>.
 * Supported CSS is termstyle's profile. NULL sheet selects the default theme.
 * Output array unchanged on failure. */
int mulib_termsyn_theme(const mulib_css_sheet_t *, mulib_termsyn_language_t,
                        mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT], mulib_xml_error_t *);
/** @brief Render a cached byte range with termstyle sanitization, atomically.
 * NULL styles selects default theme. Range and token splits must be UTF-8
 * boundaries. Returns termsyn errors, or a termstyle error for output failure. */
int mulib_termsyn_render(const mulib_termsyn_t *, size_t offset, size_t length,
                         const mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT],
                         const mulib_terminfo_t *, unsigned flags, mulib_strbuf_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSYN) && !defined(MULIB_TERMSYN_IMPLEMENTED)
#define MULIB_TERMSYN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_termsyn_tokens_ {
  size_t refs, count, capacity, end, length;
  int failed;
  mulib_termsyn_span_t *spans;
} mulib_termsyn_tokens_;
typedef struct mulib_termsyn_line_ {
  size_t start, length;
  uint64_t in, out;
  mulib_termsyn_tokens_ *tokens;
} mulib_termsyn_line_;
struct mulib_termsyn {
  mulib_termsyn_lexer_fn lexer;
  void *user;
  mulib_termsyn_language_t language;
  const mulib_edbuf_t *buffer;
  uint64_t revision;
  char *text;
  size_t size, count, span_count, lexed;
  mulib_termsyn_line_ *lines;
};
static const char *const mulib_termsyn_names_[] = {
  "text","keyword","type","string","number","comment","operator","punctuation",
  "preprocessor","constant","identifier","error"
};
const char *mulib_termsyn_token_name(mulib_termsyn_token_t token) {
  return token >= 0 && token < MULIB_TERMSYN_TOKEN_COUNT ? mulib_termsyn_names_[token] : NULL;
}
static void mulib_termsyn_tokens_drop_(mulib_termsyn_tokens_ *t) {
  if (t && !--t->refs) { free(t->spans); free(t); }
}
static void mulib_termsyn_lines_free_(mulib_termsyn_line_ *lines, size_t count) {
  size_t i;
  for (i = 0; i < count; ++i) mulib_termsyn_tokens_drop_(lines[i].tokens);
  free(lines);
}
static int mulib_termsyn_add_(mulib_termsyn_tokens_ *t, size_t at, size_t n, mulib_termsyn_token_t kind) {
  mulib_termsyn_span_t *s;
  if (!n) return 1;
  if (t->count && t->spans[t->count-1].token == kind && t->end == at) {
    t->spans[t->count-1].length += n; t->end = at + n; return 1;
  }
  if (t->count == t->capacity) {
    size_t cap = t->capacity ? t->capacity * 2 : 16;
    if (cap < t->capacity || cap > SIZE_MAX / sizeof *s) { t->failed = 2; return 0; }
    s = (mulib_termsyn_span_t *)realloc(t->spans, cap * sizeof *s);
    if (!s) { t->failed = 2; return 0; }
    t->spans = s; t->capacity = cap;
  }
  s = &t->spans[t->count++]; s->offset = at; s->length = n; s->token = kind; t->end = at + n; return 1;
}
static int mulib_termsyn_emit_(void *context, size_t at, size_t n, mulib_termsyn_token_t kind) {
  mulib_termsyn_tokens_ *t = (mulib_termsyn_tokens_ *)context;
  if (t->failed) return 0;
  if (!n || at < t->end || at > t->length || n > t->length - at
      || kind < 0 || kind >= MULIB_TERMSYN_TOKEN_COUNT) { t->failed = 1; return 0; }
  if (at > t->end && !mulib_termsyn_add_(t, t->end, at - t->end, MULIB_TERMSYN_TEXT)) return 0;
  return mulib_termsyn_add_(t, at, n, kind);
}
static int mulib_termsyn_ident_(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128;
}
static int mulib_termsyn_digit_(unsigned char c) { return c >= '0' && c <= '9'; }
static int mulib_termsyn_word_(const char *words, const char *s, size_t n) {
  while (*words) {
    const char *start;
    while (*words == ' ') ++words;
    start = words;
    while (*words && *words != ' ') ++words;
    if ((size_t)(words - start) == n && !memcmp(start, s, n)) return 1;
  }
  return 0;
}
static mulib_termsyn_token_t mulib_termsyn_word_kind_(mulib_termsyn_language_t lang, const char *s, size_t n) {
  const char *kw = "", *types = "", *constants = "";
  if (lang == MULIB_TERMSYN_C || lang == MULIB_TERMSYN_CPP) {
    kw = "auto break case const continue default do else enum extern for goto if inline register restrict return sizeof static struct switch typedef union volatile while _Alignas _Alignof _Atomic _Generic _Noreturn _Static_assert _Thread_local alignas alignof asm catch class concept constexpr consteval constinit co_await co_return co_yield decltype delete dynamic_cast explicit export friend mutable namespace new noexcept operator private protected public requires reinterpret_cast static_assert static_cast template this thread_local throw try typeid typename using virtual";
    types = "void char short int long float double signed unsigned _Bool _Complex bool wchar_t char8_t char16_t char32_t size_t ptrdiff_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t";
    constants = "NULL true false nullptr";
  } else if (lang == MULIB_TERMSYN_PYTHON) {
    kw = "and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield match case";
    types = "bool bytes bytearray complex dict float frozenset int list object set str tuple type";
    constants = "True False None NotImplemented Ellipsis";
  } else if (lang == MULIB_TERMSYN_JSON) constants = "true false null";
  else if (lang == MULIB_TERMSYN_SHELL) {
    kw = "if then else elif fi case esac for select while until do done in function time coproc";
    types = "export readonly local declare typeset";
  }
  if (mulib_termsyn_word_(kw, s, n)) return MULIB_TERMSYN_KEYWORD;
  if (mulib_termsyn_word_(types, s, n)) return MULIB_TERMSYN_TYPE;
  if (mulib_termsyn_word_(constants, s, n)) return MULIB_TERMSYN_CONSTANT;
  return MULIB_TERMSYN_IDENTIFIER;
}
static int mulib_termsyn_builtin_(const char *s, size_t n, uint64_t state,
                                   mulib_termsyn_emit_fn emit, void *context, uint64_t *out, void *user) {
  mulib_termsyn_language_t lang = *(mulib_termsyn_language_t *)user;
  size_t i = 0;
  int c_lang = lang == MULIB_TERMSYN_C || lang == MULIB_TERMSYN_CPP, leading = 1;
  if (lang == MULIB_TERMSYN_PLAIN) {
    *out = 0; return !n || emit(context, 0, n, MULIB_TERMSYN_TEXT);
  }
  while (i < n) {
    size_t start = i;
    mulib_termsyn_token_t token = MULIB_TERMSYN_TEXT;
    if (!state && c_lang && i + 1 < n && s[i] == '/' && s[i+1] == '*') { state = 1; i += 2; }
    if (!state && (s[i] == '"' || (s[i] == '\'' && lang != MULIB_TERMSYN_JSON))) {
      char quote = s[i];
      state = quote == '"' ? 2 : 3; ++i;
      if (lang == MULIB_TERMSYN_PYTHON && i + 1 < n && s[i] == quote && s[i+1] == quote) {
        state = quote == '"' ? 5 : 4; i += 2;
      }
    }
    if (state == 1) {
      token = MULIB_TERMSYN_COMMENT;
      while (i < n) { if (i + 1 < n && s[i] == '*' && s[i+1] == '/') { i += 2; state = 0; break; } ++i; }
    } else if (state >= 2 && state <= 5) {
      char quote = state == 2 || state == 5 ? '"' : '\'';
      int triple = state >= 4, continued = 0;
      token = MULIB_TERMSYN_STRING;
      while (i < n) {
        if (s[i] == '\\' && !(lang == MULIB_TERMSYN_SHELL && quote == '\'')) {
          if (i + 1 < n) { continued = s[i+1] == '\n'; i += 2; continue; }
          ++i; break;
        }
        if (s[i] == quote && (!triple || (i + 2 < n && s[i+1] == quote && s[i+2] == quote))) {
          i += triple ? 3 : 1; state = 0; break;
        }
        if (s[i] == '\n' && !triple && lang != MULIB_TERMSYN_SHELL) break;
        ++i;
      }
      if (state && !triple && lang != MULIB_TERMSYN_SHELL && !continued) {
        state = 0; token = MULIB_TERMSYN_ERROR;
        if (i == start) ++i;
      }
    } else if (state == 6 || (c_lang && leading && s[i] == '#')) {
      token = MULIB_TERMSYN_PREPROCESSOR; i = n; state = 0;
      if (n >= 2 && s[n-1] == '\n' && s[n-2] == '\\') state = 6;
    } else if ((c_lang && i + 1 < n && s[i] == '/' && s[i+1] == '/')
               || ((lang == MULIB_TERMSYN_PYTHON || lang == MULIB_TERMSYN_SHELL) && s[i] == '#')) {
      token = MULIB_TERMSYN_COMMENT;
      while (i < n && s[i] != '\n') ++i;
    } else if (mulib_termsyn_ident_((unsigned char)s[i])) {
      ++i;
      while (i < n && (mulib_termsyn_ident_((unsigned char)s[i]) || mulib_termsyn_digit_((unsigned char)s[i]))) ++i;
      token = mulib_termsyn_word_kind_(lang, s + start, i - start);
    } else if (mulib_termsyn_digit_((unsigned char)s[i]) || (s[i] == '.' && i + 1 < n && mulib_termsyn_digit_((unsigned char)s[i+1]))) {
      token = MULIB_TERMSYN_NUMBER; ++i;
      while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (mulib_termsyn_ident_(c) || mulib_termsyn_digit_(c) || c == '.') ++i;
        else if ((c == '+' || c == '-') && (s[i-1] == 'e' || s[i-1] == 'E' || s[i-1] == 'p' || s[i-1] == 'P')) ++i;
        else break;
      }
    } else if (lang == MULIB_TERMSYN_SHELL && s[i] == '$') {
      token = MULIB_TERMSYN_CONSTANT; ++i;
      if (i < n && s[i] == '{') { ++i; while (i < n && s[i] != '}' && s[i] != '\n') ++i; if (i < n && s[i] == '}') ++i; }
      else while (i < n && (mulib_termsyn_ident_((unsigned char)s[i]) || mulib_termsyn_digit_((unsigned char)s[i]))) ++i;
    } else if (s[i] && strchr("+-*/%=!<>|&^~?:.", s[i])) {
      token = MULIB_TERMSYN_OPERATOR; ++i;
      while (i < n && s[i] && strchr("+-*/%=!<>|&^~?:.", s[i])) ++i;
    } else if (s[i] && strchr("(){}[],;", s[i])) { token = MULIB_TERMSYN_PUNCTUATION; ++i; }
    else {
      ++i;
      while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    }
    if (i == start) ++i;
    if (!emit(context, start, i - start, token)) return 0;
    if (token != MULIB_TERMSYN_TEXT) leading = 0;
  }
  *out = state; return 1;
}
mulib_termsyn_t *mulib_termsyn_create_custom(mulib_termsyn_lexer_fn lexer, void *user) {
  mulib_termsyn_t *h;
  if (!lexer) return NULL;
  h = (mulib_termsyn_t *)calloc(1, sizeof *h);
  if (h) { h->lexer = lexer; h->user = user; }
  return h;
}
mulib_termsyn_t *mulib_termsyn_create(mulib_termsyn_language_t language) {
  mulib_termsyn_t *h;
  if (language < MULIB_TERMSYN_PLAIN || language > MULIB_TERMSYN_SHELL) return NULL;
  h = mulib_termsyn_create_custom(mulib_termsyn_builtin_, NULL);
  if (h) { h->language = language; h->user = &h->language; }
  return h;
}
void mulib_termsyn_destroy(mulib_termsyn_t *h) {
  if (!h) return;
  mulib_termsyn_lines_free_(h->lines, h->count); free(h->text); free(h);
}
static int mulib_termsyn_same_(const mulib_termsyn_t *h, size_t old,
                               const char *text, const mulib_termsyn_line_ *line) {
  return h->lines[old].length == line->length
      && !memcmp(h->text + h->lines[old].start, text + line->start, line->length);
}
int mulib_termsyn_update(mulib_termsyn_t *h, const mulib_edbuf_t *b) {
  char *text;
  mulib_termsyn_line_ *lines;
  size_t count, size, i, at = 0, prefix = 0, suffix = 0, lexed = 0, spans = 0;
  uint64_t state = 0;
  int failure = MULIB_TERMSYN_ENOMEM;
  if (!h || !b) return MULIB_TERMSYN_EINVAL;
  if (h->buffer == b && h->text && h->revision == mulib_edbuf_revision(b)) { h->lexed = 0; return 0; }
  count = mulib_edbuf_line_count(b); size = mulib_edbuf_size(b);
  if (count > SIZE_MAX / sizeof *lines) return MULIB_TERMSYN_ERANGE;
  text = mulib_edbuf_copy(b);
  if (!text) return MULIB_TERMSYN_ENOMEM;
  lines = (mulib_termsyn_line_ *)calloc(count, sizeof *lines);
  if (!lines) { free(text); return MULIB_TERMSYN_ENOMEM; }
  for (i = 0; i < count; ++i) {
    lines[i].start = at;
    while (at < size && text[at] != '\n') ++at;
    if (at < size) ++at;
    lines[i].length = at - lines[i].start;
  }
  if (h->buffer == b) {
    while (prefix < count && prefix < h->count && mulib_termsyn_same_(h, prefix, text, &lines[prefix])) ++prefix;
    while (suffix < count - prefix && suffix < h->count - prefix
           && mulib_termsyn_same_(h, h->count - 1 - suffix, text, &lines[count - 1 - suffix])) ++suffix;
  }
  for (i = 0; i < count; ++i) {
    size_t old = i < prefix ? i : i >= count - suffix ? h->count - (count - i) : SIZE_MAX;
    lines[i].in = state;
    if (old != SIZE_MAX && h->lines[old].in == state) {
      lines[i].tokens = h->lines[old].tokens; ++lines[i].tokens->refs;
      lines[i].out = h->lines[old].out;
    } else {
      mulib_termsyn_tokens_ *t = (mulib_termsyn_tokens_ *)calloc(1, sizeof *t);
      int ok;
      if (!t) goto fail;
      t->refs = 1; t->length = lines[i].length; lines[i].tokens = t;
      ok = h->lexer(text + lines[i].start, lines[i].length, state,
                    mulib_termsyn_emit_, t, &lines[i].out, h->user);
      if (!ok || t->failed) { failure = t->failed == 2 ? MULIB_TERMSYN_ENOMEM : MULIB_TERMSYN_ELEXER; goto fail; }
      if (t->end < t->length && !mulib_termsyn_add_(t, t->end, t->length - t->end, MULIB_TERMSYN_TEXT)) goto fail;
      ++lexed;
    }
    state = lines[i].out;
    if (lines[i].tokens->count > SIZE_MAX - spans) { failure = MULIB_TERMSYN_ERANGE; goto fail; }
    spans += lines[i].tokens->count;
  }
  mulib_termsyn_lines_free_(h->lines, h->count); free(h->text);
  h->lines = lines; h->count = count; h->text = text; h->size = size; h->buffer = b;
  h->revision = mulib_edbuf_revision(b); h->span_count = spans; h->lexed = lexed; return 0;
fail:
  mulib_termsyn_lines_free_(lines, count); free(text); return failure;
}
size_t mulib_termsyn_lexed_lines(const mulib_termsyn_t *h) { return h ? h->lexed : 0; }
uint64_t mulib_termsyn_revision(const mulib_termsyn_t *h) { return h ? h->revision : 0; }
size_t mulib_termsyn_span_count(const mulib_termsyn_t *h) { return h ? h->span_count : 0; }
int mulib_termsyn_span(const mulib_termsyn_t *h, size_t index, mulib_termsyn_span_t *out) {
  size_t i;
  if (!h || !out) return MULIB_TERMSYN_EINVAL;
  for (i = 0; i < h->count; ++i) {
    if (index < h->lines[i].tokens->count) {
      *out = h->lines[i].tokens->spans[index]; out->offset += h->lines[i].start; return 0;
    }
    index -= h->lines[i].tokens->count;
  }
  return MULIB_TERMSYN_ERANGE;
}
int mulib_termsyn_at(const mulib_termsyn_t *h, size_t at, mulib_termsyn_span_t *out) {
  size_t low = 0, high, line, i;
  if (!h || !out) return MULIB_TERMSYN_EINVAL;
  if (at >= h->size) return MULIB_TERMSYN_ENOTFOUND;
  high = h->count;
  while (low < high) { size_t mid = low + (high - low) / 2; if (h->lines[mid].start <= at) low = mid + 1; else high = mid; }
  line = low - 1;
  for (i = 0; i < h->lines[line].tokens->count; ++i) {
    mulib_termsyn_span_t span = h->lines[line].tokens->spans[i];
    if (at - h->lines[line].start - span.offset < span.length) { span.offset += h->lines[line].start; *out = span; return 0; }
  }
  return MULIB_TERMSYN_ENOTFOUND;
}
void mulib_termsyn_default_theme(mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT]) {
  size_t i;
  if (!styles) return;
  for (i = 0; i < MULIB_TERMSYN_TOKEN_COUNT; ++i) styles[i] = mulib_term_style_default();
  styles[MULIB_TERMSYN_KEYWORD].foreground = MULIB_TERM_RGB(198,120,221);
  styles[MULIB_TERMSYN_TYPE].foreground = MULIB_TERM_RGB(229,192,123);
  styles[MULIB_TERMSYN_STRING].foreground = MULIB_TERM_RGB(152,195,121);
  styles[MULIB_TERMSYN_NUMBER].foreground = MULIB_TERM_RGB(209,154,102);
  styles[MULIB_TERMSYN_CONSTANT].foreground = MULIB_TERM_RGB(209,154,102);
  styles[MULIB_TERMSYN_COMMENT].foreground = MULIB_TERM_RGB(92,99,112);
  styles[MULIB_TERMSYN_COMMENT].attributes = MULIB_TERM_ITALIC;
  styles[MULIB_TERMSYN_PREPROCESSOR].foreground = MULIB_TERM_RGB(86,182,194);
  styles[MULIB_TERMSYN_ERROR].foreground = MULIB_TERM_RGB(224,108,117);
  styles[MULIB_TERMSYN_ERROR].attributes = MULIB_TERM_UNDERLINE;
}
int mulib_termsyn_theme(const mulib_css_sheet_t *sheet, mulib_termsyn_language_t language,
                        mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT], mulib_xml_error_t *error) {
  static const char *const classes[] = {"syntax plain","syntax c","syntax cpp","syntax python","syntax json","syntax shell"};
  mulib_xml_node_t *root, *child;
  mulib_term_style_t base, tmp[MULIB_TERMSYN_TOKEN_COUNT];
  size_t i;
  int result = MULIB_TERMSYN_ENOMEM;
  if (!styles || language < MULIB_TERMSYN_PLAIN || language > MULIB_TERMSYN_SHELL) return MULIB_TERMSYN_EINVAL;
  if (error) { error->offset = 0; error->message = NULL; }
  if (!sheet) { mulib_termsyn_default_theme(styles); return 0; }
  root = mulib_xml_new(MULIB_XML_ELEMENT, "code");
  if (!root) return result;
  if (!mulib_xml_set_attr(root, "class", classes[language])) goto done;
  result = mulib_termstyle_resolve(sheet, root, NULL, &base, error);
  if (result) goto done;
  for (i = 0; i < MULIB_TERMSYN_TOKEN_COUNT; ++i) {
    child = mulib_xml_new(MULIB_XML_ELEMENT, "span");
    if (!child) { result = MULIB_TERMSYN_ENOMEM; goto done; }
    if (!mulib_xml_set_attr(child, "class", mulib_termsyn_names_[i]) || !mulib_xml_append(root, child)) {
      mulib_xml_destroy(child); result = MULIB_TERMSYN_ENOMEM; goto done;
    }
    result = mulib_termstyle_resolve(sheet, child, &base, &tmp[i], error);
    if (result) goto done;
  }
  memcpy(styles, tmp, sizeof tmp);
done:
  mulib_xml_destroy(root); return result;
}
int mulib_termsyn_render(const mulib_termsyn_t *h, size_t at, size_t n,
                         const mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT],
                         const mulib_terminfo_t *info, unsigned flags, mulib_strbuf_t *out) {
  mulib_term_style_t defaults[MULIB_TERMSYN_TOKEN_COUNT];
  mulib_strbuf_t tmp;
  size_t i, j, end;
  int result = 0;
  if (!h || !out || !h->text || (flags & ~3u)) return MULIB_TERMSYN_EINVAL;
  if (at > h->size || n > h->size - at) return MULIB_TERMSYN_ERANGE;
  if (!styles) { mulib_termsyn_default_theme(defaults); styles = defaults; }
  end = at + n; mulib_strbuf_init(&tmp);
  for (i = 0; i < h->count && !result; ++i) {
    if (h->lines[i].start >= end) break;
    if (h->lines[i].start + h->lines[i].length <= at) continue;
    for (j = 0; j < h->lines[i].tokens->count; ++j) {
      mulib_termsyn_span_t s = h->lines[i].tokens->spans[j];
      size_t begin, stop;
      s.offset += h->lines[i].start; begin = s.offset > at ? s.offset : at;
      stop = s.offset + s.length < end ? s.offset + s.length : end;
      if (begin < stop) {
        result = mulib_termstyle_write(styles[s.token], info, flags, h->text + begin, stop - begin, &tmp);
        if (result) break;
      }
    }
  }
  if (!result && !mulib_strbuf_append_n(out, tmp.data, tmp.length)) result = MULIB_TERMSYN_ENOMEM;
  mulib_strbuf_free(&tmp); return result;
}
#ifdef __cplusplus
}
#endif
#endif
