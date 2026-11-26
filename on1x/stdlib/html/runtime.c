#include "runtime.h"
#include "on1x_runtime.h"
#include "mulib-html.h"
#include "mulib-html-treebuilder.h"
#include "mulib-xpath.h"
#include <stdlib.h>
#include <string.h>

enum {
  input_limit = 64 * 1024, markup_limit = 128 * 1024, depth_limit = 48,
  node_limit = 1024, token_tree_limit = 2048, query_limit = 1024,
  query_separators = 16
};
enum { invalid = 1, limit = 2, kernel_failure = 3, absent = 4, protocol = 5 };
enum { tolerant, strict, fragment };
enum {
  kind, name, value, text, serialize, parent, first, next, subtree_end,
  attr, attr_count, attr_name, attr_value, xpath
};

/* All replies are '+' followed by the length-aware payload, or '-[1-5]'.
 * The temporary staging area is stack memory: none of the native owners
 * survive the call to the On1x allocator, which may exit on exhaustion. */
typedef struct {
  size_t size;
  char bytes[markup_limit + 2];
} reply_t;

static const char *finish(const reply_t *r, int error)
{
  static const char *const errors[] = {"-5", "-1", "-2", "-3", "-4", "-5"};
  if (error) return errors[error];
  return r->size == 1 ? "+" : on1x_text_slice(r->bytes, 0, (int64_t)r->size);
}

static int append(reply_t *r, const char *s, size_t n)
{
  if (n > sizeof r->bytes - 1 - r->size) return limit;
  memcpy(r->bytes + r->size, s, n);
  r->size += n;
  r->bytes[r->size] = '\0';
  return 0;
}

static int literal(reply_t *r, const char *s)
{
  return s ? append(r, s, strlen(s)) : absent;
}

static int nibble(unsigned char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int uint_reply(reply_t *r, size_t n)
{
  static const char digits[] = "0123456789abcdef";
  char v[4];
  if (n > 0xffffu) return protocol;
  for (size_t i = sizeof v; i; n >>= 4) v[--i] = digits[n & 15u];
  return append(r, v, sizeof v);
}

static int space(char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static int ascii_alpha(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_void(const char *tag)
{
  static const char *const names[] = {"area", "base", "br", "col", "embed", "hr",
    "img", "input", "link", "meta", "param", "source", "track", "wbr"};
  for (size_t i = 0; i < sizeof names / sizeof *names; ++i)
    if (!strcmp(tag, names[i])) return 1;
  return 0;
}

static int known_reference(const char *s, size_t n)
{
  static const char *const refs[] = {"amp;", "lt;", "gt;", "quot;", "apos;", "nbsp;"};
  /* Let the real HTML parser validate numeric references, including ranges. */
  if (n && *s == '#') return 1;
  for (size_t i = 0; i < sizeof refs / sizeof *refs; ++i) {
    size_t len = strlen(refs[i]);
    if (n >= len && !memcmp(s, refs[i], len)) return 1;
  }
  return 0;
}

/* Preserve supported references for mulib_html_parse to decode; protect stray
 * '&', unknown named references (literal text), and literal '<'. For a start
 * token the caller has already emitted the opening '<'. */
static int append_lexeme(reply_t *r, const char *s, size_t n)
{
  size_t begin = 0;
  for (size_t i = 0; i < n; ++i) {
    const char *escape = NULL;
    if (s[i] == '<') escape = "&lt;";
    if (s[i] == '&' && !known_reference(s + i + 1, n - i - 1)) escape = "&amp;";
    if (escape) {
      int rc = append(r, s + begin, i - begin);
      if (!rc) rc = literal(r, escape);
      if (rc) return rc;
      begin = i + 1;
    }
  }
  return append(r, s + begin, n - begin);
}

static int only_space(const mulib_html_tree_node_t *n)
{
  if (!n->is_text) return 0;
  for (size_t i = 0; i < n->text.len; ++i)
    if (!space(n->text.data[i])) return 0;
  return 1;
}

static int framing_ok(const char *tag, int *head, int *body)
{
  if (!strcmp(tag, "head")) {
    if (*head || *body) return 0;
    *head = 1;
  }
  if (!strcmp(tag, "body")) {
    if (*body) return 0;
    *body = 1;
  }
  return 1;
}

/* frame: 2 permits the sole html root, 1 permits its immediate head/body,
 * and 0 denotes ordinary content. No inferred browser insertion modes. */
static int tree_emit(reply_t *r, const mulib_html_tree_node_t *n, int frame)
{
  int rc;
  int has_head = 0, has_body = 0;
  const char *tag = n->tag;
  const char *raw = n->attributes.data;
  size_t taglen, rawlen;
  if (n->is_text) return append_lexeme(r, n->text.data, n->text.len);
  taglen = strlen(tag);
  rawlen = n->attributes.len;
  if (!raw || rawlen < taglen + 2 || !ascii_alpha(tag[0]) ||
      (!space(raw[taglen + 1]) && raw[taglen + 1] != '>' &&
       raw[taglen + 1] != '/')) return invalid;
  /* Treebuilder recognizes slash-closing non-void tags; this profile doesn't
   * reinterpret them (the HTML parser also rejects such syntax). */
  if (raw[rawlen - 2] == '/' && !is_void(tag)) return invalid;
  /* Explicit static analysis profile: ignore complete script/style subtrees. */
  if (!strcmp(tag, "script") || !strcmp(tag, "style")) return 0;
  if ((!strcmp(tag, "html") && frame != 2) ||
      ((!strcmp(tag, "head") || !strcmp(tag, "body")) && frame != 1))
    return invalid;
  rc = append(r, raw, 1);
  if (!rc) rc = append_lexeme(r, raw + 1, rawlen - 1);
  if (rc || is_void(tag)) return rc;
  for (const mulib_html_tree_node_t *c = n->first_child; c; c = c->next_sibling) {
    if (frame == 2 && !framing_ok(c->tag, &has_head, &has_body)) return invalid;
    rc = tree_emit(r, c, frame == 2 ? 1 : 0);
    if (rc) return rc;
  }
  rc = literal(r, "</");
  if (!rc) rc = literal(r, tag);
  if (!rc) rc = literal(r, ">");
  return rc;
}

static int tolerant_document(const char *source, size_t n, reply_t *r)
{
  mulib_html_tree_node_t *root = NULL;
  const mulib_html_tree_node_t *c, *element = NULL;
  int wrap, has_head = 0, has_body = 0, rc = 0;
  if (!mulib_html_tree_parse(source, n, token_tree_limit, depth_limit, &root))
    return kernel_failure;
  for (c = root->first_child; c; c = c->next_sibling) {
    if (!strcmp(c->tag, "html")) {
      if (element) { rc = invalid; goto done; }
      element = c;
    }
    if (!framing_ok(c->tag, &has_head, &has_body)) { rc = invalid; goto done; }
  }
  if (element) {
    for (c = root->first_child; c; c = c->next_sibling) {
      if (c != element && !only_space(c)) { rc = invalid; goto done; }
    }
    rc = tree_emit(r, element, 2);
    goto done;
  }
  /* Without an explicit html: wrap top-level head/body in html, or put
   * other top-level content in html/body. No browser insertion modes. */
  wrap = !has_head && !has_body;
  rc = literal(r, wrap ? "<html><body>" : "<html>");
  for (c = root->first_child; !rc && c; c = c->next_sibling) {
    if (!wrap && strcmp(c->tag, "head") && strcmp(c->tag, "body") && !only_space(c))
      rc = invalid;
    else
      rc = tree_emit(r, c, wrap ? 0 : 1);
  }
  if (!rc) rc = literal(r, wrap ? "</body></html>" : "</html>");
done:
  mulib_html_tree_free(root);
  return rc;
}

typedef struct {
  mulib_xml_node_t *nodes[node_limit]; /* borrowed for one native call */
  size_t end[node_limit];             /* one-past-subtree preorder index */
  size_t count;
} html_index;

static int collect(html_index *tree, mulib_xml_node_t *node, size_t depth)
{
  size_t i;
  if (tree->count == node_limit || depth > depth_limit) return limit;
  i = tree->count++;
  tree->nodes[i] = node;
  for (mulib_xml_node_t *c = mulib_xml_first(node); c; c = mulib_xml_next(c)) {
    int rc = collect(tree, c, depth + (mulib_xml_type(c) == MULIB_XML_ELEMENT));
    if (rc) return rc;
  }
  tree->end[i] = tree->count;
  return 0;
}

static int ordinal_reply(reply_t *r, const html_index *tree,
                         const mulib_xml_node_t *node)
{
  if (!node) return absent;
  for (size_t i = 0; i < tree->count; ++i)
    if (tree->nodes[i] == node) return uint_reply(r, i);
  return protocol;
}

static int append_serialized(reply_t *r, const mulib_xml_node_t *node)
{
  size_t n;
  char *s = mulib_html_serialize(node, &n);
  int rc = s ? append(r, s, n) : kernel_failure;
  free(s);
  return rc;
}

static int serialize_root(reply_t *r, const mulib_xml_node_t *root, int frag)
{
  /* Fragment root is synthetic div. Snapshot stores only the fragment. */
  if (!frag) return append_serialized(r, root);
  for (mulib_xml_node_t *c = mulib_xml_first(root); c; c = mulib_xml_next(c)) {
    int rc = append_serialized(r, c);
    if (rc) return rc;
  }
  return 0;
}

/* Use the kernel's complete syntax checker after limiting work. Ordered
 * insertion in that kernel is expensive on broad/multi-descendant paths. */
static int xpath_reply(reply_t *r, const html_index *tree, mulib_xml_node_t *node,
                       const char *expr)
{
  mulib_xpath_result_t result = {0};
  mulib_xml_error_t error = {0};
  size_t len = 0, separators = 0, descendants = 0;
  char quote = 0;
  int rc = 0;
  for (; len <= query_limit && expr[len]; ++len) {}
  if (len > query_limit) return limit;
  for (size_t i = 0; i < len; ++i) {
    if (quote) {
      if (expr[i] == quote) quote = 0;
    } else if (expr[i] == '\'' || expr[i] == '"') {
      quote = expr[i];
    } else if (expr[i] == '/') {
      if (++separators > query_separators) return limit;
      if (i + 1 < len && expr[i + 1] == '/') {
        if (++descendants > 1) return limit;
        ++i;
      }
    }
  }
  if (!mulib_xpath_query(node, expr, &result, &error))
    rc = error.message && !strcmp(error.message, "allocation failed")
      ? kernel_failure : invalid;
  for (size_t i = 0; !rc && i < result.count; ++i)
    rc = ordinal_reply(r, tree, result.nodes[i]);
  mulib_xpath_result_free(&result);
  return rc;
}

const char *on1x_html_parse_v1(const char *source, int64_t length,
                               int64_t profile, int64_t hex)
{
  char decoded[input_limit + 1];
  reply_t r;
  mulib_xml_node_t *root;
  html_index tree;
  size_t n;
  int rc;
  if (!source || length < 0 || profile < tolerant || profile > fragment ||
      (hex != 0 && hex != 1)) return finish(NULL, invalid);
  if (length > (int64_t)input_limit * (hex ? 2 : 1)) return finish(NULL, limit);
  n = (size_t)length;
  if (hex) {
    if (n % 2) return finish(NULL, invalid);
    for (size_t i = 0; i < n; i += 2) {
      int hi = nibble((unsigned char)source[i]), lo = nibble((unsigned char)source[i + 1]);
      if (hi < 0 || lo < 0) return finish(NULL, invalid);
      decoded[i / 2] = (char)(hi * 16 + lo);
    }
    n /= 2;
    decoded[n] = '\0';
    source = decoded;
  }
  if (!mulib_xml_valid_text(source, n)) return finish(NULL, invalid);
  if (profile == tolerant) {
    r.size = 0;
    r.bytes[0] = '\0';
    rc = tolerant_document(source, n, &r);
    if (rc) return finish(&r, rc);
    source = r.bytes;
    n = r.size;
  }
  /* The MuLIB parser merges syntax, unsupported constructs and allocation
   * failures in some diagnostics; do not claim a finer error distinction. */
  root = mulib_html_parse(source, n, profile == fragment, NULL);
  if (!root) return finish(NULL, kernel_failure);
  r.size = 1;
  memcpy(r.bytes, "+", 2);
  tree.count = 0;
  rc = collect(&tree, root, profile == fragment);
  if (!rc) rc = serialize_root(&r, root, profile == fragment);
  mulib_xml_destroy(root);
  return finish(&r, rc);
}

const char *on1x_html_node_v1(const char *markup, int64_t frag, int64_t ordinal,
                               int64_t op, const char *arg, int64_t index)
{
  reply_t r = {1, "+"};
  mulib_xml_node_t *root, *node;
  html_index tree;
  int rc;
  size_t n;
  if (!markup || !arg || (frag != 0 && frag != 1) || ordinal < 0 || index < 0 ||
      op < kind || op > xpath) return finish(&r, invalid);
  for (n = 0; n <= markup_limit && markup[n]; ++n) {}
  if (n > markup_limit) return finish(&r, limit);
  root = mulib_html_parse(markup, n, (int)frag, NULL);
  if (!root) return finish(&r, kernel_failure);
  tree.count = 0;
  rc = collect(&tree, root, (size_t)frag);
  if (rc) goto done;
  if ((uint64_t)ordinal >= tree.count) { rc = invalid; goto done; }
  node = tree.nodes[ordinal];
  switch (op) {
    case kind: rc = uint_reply(&r, (size_t)mulib_xml_type(node)); break;
    case name: rc = literal(&r, mulib_xml_name(node)); break;
    case value: rc = literal(&r, mulib_xml_value(node)); break;
    case text: {
      size_t len;
      char *s = mulib_html_text(node, &len);
      rc = s ? append(&r, s, len) : kernel_failure;
      free(s);
      break;
    }
    case serialize: rc = ordinal ? append_serialized(&r, node) :
                                    serialize_root(&r, root, (int)frag); break;
    case parent: rc = ordinal_reply(&r, &tree, mulib_xml_parent(node)); break;
    case first: rc = ordinal_reply(&r, &tree, mulib_xml_first(node)); break;
    case next: rc = ordinal_reply(&r, &tree, mulib_xml_next(node)); break;
    case subtree_end: rc = uint_reply(&r, tree.end[ordinal]); break;
    case attr:
      rc = !mulib_xml_valid_name(arg) ? invalid : literal(&r, mulib_xml_attr(node, arg));
      break;
    case attr_count: rc = uint_reply(&r, mulib_xml_attr_count(node)); break;
    case attr_name:
    case attr_value:
      if ((uint64_t)index >= mulib_xml_attr_count(node)) rc = absent;
      else rc = literal(&r, op == attr_name ? mulib_xml_attr_name(node, (size_t)index)
                                           : mulib_xml_attr_value(node, (size_t)index));
      break;
    case xpath: rc = xpath_reply(&r, &tree, node, arg); break;
    default: rc = protocol; break;
  }
done:
  mulib_xml_destroy(root);
  return finish(&r, rc);
}
