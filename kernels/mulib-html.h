/**
 * @file mulib-html.h
 * @brief Bounded explicit-tag HTML parser, builder and serializer on the XML DOM.
 *
 * Lowercase ASCII tag/attribute names, HTML void elements, and doctype output
 * are supported. Raw-text elements are rejected to avoid misleading escaping.
 * Attributes/URLs are not sanitized. Implement with MULIB_IMPL_HTML plus XML,
 * STRBUF and UTF8 switches. See manual/mulib-html.md.
 */
#ifndef MULIB_HTML_H
#define MULIB_HTML_H
#include "mulib-xml.h"
#include "mulib-docir.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Create an owned html/head/title/body document. NULL title means
   * empty. */
  mulib_xml_node_t *mulib_html_document (const char *title);
  /** @brief Borrow the body's node from a document made by
   * mulib_html_document. */
  mulib_xml_node_t *mulib_html_body (const mulib_xml_node_t *document);
  /** @brief Append an element, optionally with a text child, atomically.
   * Returns a borrowed child or NULL; parent is unchanged on failure. */
  mulib_xml_node_t *mulib_html_add (mulib_xml_node_t *parent, const char *tag,
                                    const char *text);
  /** @brief Shared XML escaping for ordinary HTML text and quoted attributes.
   * No raw-text contexts; 1 success, 0 invalid UTF-8/XML chars or allocation
   * error. */
  int mulib_html_escape (mulib_strbuf_t *out, const char *text, size_t length,
                         int attribute);
  /** @brief Serialize a DOM node as HTML; returns malloc-owned text or NULL.
   * Documents require an html root and receive <!doctype html>. Void elements
   * cannot have children; script/style and other raw-text tags are rejected.
   * Initial LF in pre/textarea/listing is padded to survive HTML parsing. */
  char *mulib_html_serialize (const mulib_xml_node_t *node, size_t *length);
  /** @brief Parse explicit-tag HTML into the shared owned DOM. ASCII names are
   * lowercased; void tags, boolean/unquoted attributes, comments, HTML doctype,
   * numeric entities and amp/lt/gt/quot/apos/nbsp are supported. No HTML5 error
   * recovery, implicit end tags, raw text, foreign content or external I/O.
   * fragment=1 returns a synthetic div container; otherwise requires html root.
   * NULL on failure with byte diagnostic; maximum 16 MiB and depth 128. */
  mulib_xml_node_t *mulib_html_parse(const char *source, size_t length, int fragment,
                                     mulib_xml_error_t *error);
  /** @brief Concatenate descendant text/CDATA, translating br to LF. No CSS
   * whitespace collapse or hidden-element filtering. Owned UTF-8 or NULL. */
  char *mulib_html_text(const mulib_xml_node_t *node, size_t *length);
  /** @brief Atomic HTML/XHTML document export of validated semantic DocIR.
   * Requires MULIB_ENABLE_DOCIR. Language is a bounded ASCII BCP-47-style tag. */
  int mulib_html_docir(const mulib_docir_node_t *root,const char *title,const char *language,mulib_strbuf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_H */

#if defined(MULIB_IMPL_HTML) && !defined(MULIB_HTML_IMPLEMENTED)
#define MULIB_HTML_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_html_name_internal (const char *s)
  {
    if (!s || *s < 'a' || *s > 'z')
      return 0;
    for (s++; *s; s++)
      if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-'
            || *s == '_' || *s == ':'))
        return 0;
    return 1;
  }
  static int
  mulib_html_in_internal (const char *tag, const char *const *names, size_t n)
  {
    size_t i;
    for (i = 0; i < n; i++)
      if (!strcmp (tag, names[i]))
        return 1;
    return 0;
  }
  static int
  mulib_html_void_internal (const char *s)
  {
    static const char *const names[]
        = { "area",  "base", "br",   "col",   "embed",  "hr",    "img",
            "input", "link", "meta", "param", "source", "track", "wbr" };
    return mulib_html_in_internal (s, names,
                                   sizeof (names) / sizeof (names[0]));
  }
  static int
  mulib_html_raw_internal (const char *s)
  {
    static const char *const names[]
        = { "script",  "style",    "xmp",       "iframe",
            "noembed", "noframes", "plaintext", "noscript" };
    return mulib_html_in_internal (s, names,
                                   sizeof (names) / sizeof (names[0]));
  }
  mulib_xml_node_t *
  mulib_html_add (mulib_xml_node_t *parent, const char *tag, const char *text)
  {
    mulib_xml_node_t *n, *t;
    if (!parent || (mulib_xml_name(parent) &&
        (mulib_html_void_internal(mulib_xml_name(parent)) || mulib_html_raw_internal(mulib_xml_name(parent)) ||
         !strcmp(mulib_xml_name(parent), "title") || !strcmp(mulib_xml_name(parent), "textarea")))
        || !mulib_html_name_internal (tag) || mulib_html_raw_internal (tag)
        || (text && mulib_html_void_internal (tag)))
      return NULL;
    n = mulib_xml_new (MULIB_XML_ELEMENT, tag);
    if (!n)
      return NULL;
    if (text)
      {
        t = mulib_xml_new (MULIB_XML_TEXT, text);
        if (!t)
          {
            mulib_xml_destroy (n);
            return NULL;
          }
        if (!mulib_xml_append (n, t))
          {
            mulib_xml_destroy (t);
            mulib_xml_destroy (n);
            return NULL;
          }
      }
    if (!mulib_xml_append (parent, n))
      {
        mulib_xml_destroy (n);
        return NULL;
      }
    return n;
  }
  mulib_xml_node_t *
  mulib_html_document (const char *title)
  {
    mulib_xml_node_t *doc = mulib_xml_new (MULIB_XML_DOCUMENT, NULL), *html,
                     *head;
    if (!doc)
      return NULL;
    html = mulib_html_add (doc, "html", NULL);
    head = html ? mulib_html_add (html, "head", NULL) : NULL;
    if (!head || !mulib_html_add (head, "title", title ? title : "")
        || !mulib_html_add (html, "body", NULL))
      {
        mulib_xml_destroy (doc);
        return NULL;
      }
    return doc;
  }
  mulib_xml_node_t *
  mulib_html_body (const mulib_xml_node_t *doc)
  {
    mulib_xml_node_t *root = mulib_xml_root (doc), *n;
    if (!root || strcmp (mulib_xml_name (root), "html"))
      return NULL;
    for (n = mulib_xml_first (root); n; n = mulib_xml_next (n))
      if (mulib_xml_name (n) && !strcmp (mulib_xml_name (n), "body"))
        return n;
    return NULL;
  }
  int
  mulib_html_escape (mulib_strbuf_t *out, const char *s, size_t n, int attr)
  {
    return mulib_xml_escape (out, s, n, attr);
  }
  static int
  mulib_html_leading_lf_internal (const mulib_xml_node_t *node)
  {
    mulib_xml_node_t *c;
    for (c = mulib_xml_first (node); c; c = mulib_xml_next (c))
      {
        mulib_xml_type_t type = mulib_xml_type (c);
        const char *value;
        /* Empty text emits nothing; a tag/comment does emit a token before LF.
         */
        if (type != MULIB_XML_TEXT && type != MULIB_XML_CDATA)
          return 0;
        value = mulib_xml_value (c);
        if (*value)
          return *value == '\n';
      }
    return 0;
  }
  static int
  mulib_html_write_internal (mulib_strbuf_t *out, const mulib_xml_node_t *node,
                             size_t depth)
  {
    mulib_xml_type_t type = mulib_xml_type (node);
    mulib_xml_node_t *c;
    const char *name = mulib_xml_name (node);
    size_t i;
    if (type == MULIB_XML_DOCUMENT)
      {
        c = mulib_xml_root (node);
        if (!c || strcmp (mulib_xml_name (c), "html")
            || !mulib_strbuf_append (out, "<!doctype html>\n"))
          return 0;
        for (c = mulib_xml_first (node); c; c = mulib_xml_next (c))
          if (!mulib_html_write_internal (out, c, depth))
            return 0;
        return 1;
      }
    if (type == MULIB_XML_ELEMENT)
      {
        if (depth > MULIB_XML_DEPTH_LIMIT || !mulib_html_name_internal (name)
            || mulib_html_raw_internal (name)
            || (mulib_html_void_internal (name) && mulib_xml_first (node)))
          return 0;
        if (!strcmp(name, "title") || !strcmp(name, "textarea"))
          for (c = mulib_xml_first(node); c; c = mulib_xml_next(c))
            if (mulib_xml_type(c) != MULIB_XML_TEXT && mulib_xml_type(c) != MULIB_XML_CDATA) return 0;
        if (!mulib_strbuf_putc (out, '<') || !mulib_strbuf_append (out, name))
          return 0;
        for (i = 0; i < mulib_xml_attr_count (node); i++)
          {
            const char *key = mulib_xml_attr_name (node, i),
                       *value = mulib_xml_attr_value (node, i);
            if (!mulib_html_name_internal (key)
                || !mulib_strbuf_putc (out, ' ')
                || !mulib_strbuf_append (out, key)
                || !mulib_strbuf_append (out, "=\"")
                || !mulib_html_escape (out, value, strlen (value), 1)
                || !mulib_strbuf_putc (out, '"'))
              return 0;
          }
        if (!mulib_strbuf_putc (out, '>'))
          return 0;
        if (mulib_html_void_internal (name))
          return 1;
        /* HTML discards one LF token immediately after these start tags. */
        if ((!strcmp (name, "pre") || !strcmp (name, "textarea")
             || !strcmp (name, "listing"))
            && mulib_html_leading_lf_internal (node)
            && !mulib_strbuf_putc (out, '\n'))
          return 0;
        for (c = mulib_xml_first (node); c; c = mulib_xml_next (c))
          if (!mulib_html_write_internal (out, c, depth + 1))
            return 0;
        return mulib_strbuf_append (out, "</")
               && mulib_strbuf_append (out, name)
               && mulib_strbuf_putc (out, '>');
      }
    if (type == MULIB_XML_COMMENT)
      {
        /* HTML has additional comment-token hazards not rejected by XML. */
        const char *v = mulib_xml_value (node);
        size_t n = strlen (v);
        if (*v == '>' || !strncmp (v, "->", 2) || strstr (v, "<!-")
            || (n && v[n - 1] == '<'))
          return 0;
        return mulib_strbuf_append (out, "<!--")
               && mulib_strbuf_append (out, v)
               && mulib_strbuf_append (out, "-->");
      }
    return mulib_html_escape (out, mulib_xml_value (node),
                              strlen (mulib_xml_value (node)), 0);
  }
  char *
  mulib_html_serialize (const mulib_xml_node_t *node, size_t *length)
  {
    mulib_strbuf_t out;
    mulib_strbuf_init (&out);
    if (length)
      *length = 0;
    if (!node || !mulib_html_write_internal (&out, node, 1)
        || !mulib_strbuf_append_n (&out, "", 0))
      {
        mulib_strbuf_free (&out);
        return NULL;
      }
    if (length)
      *length = out.length;
    return out.data;
  }
  static int mulib_html_space_(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
  }
  static char mulib_html_lower_(char c) { return c >= 'A' && c <= 'Z' ? (char)(c+32) : c; }
  static char *mulib_html_copy_(const char *s, size_t n, int lower) {
    size_t i; char *p = (char *)malloc(n+1);
    if (p) { for (i = 0; i < n; ++i) p[i] = lower ? mulib_html_lower_(s[i]) : s[i]; p[n] = 0; }
    return p;
  }
  static int mulib_html_equal_(const char *s, size_t n, const char *word) {
    size_t i;
    if (strlen(word) != n) return 0;
    for (i = 0; i < n; ++i) if (mulib_html_lower_(s[i]) != word[i]) return 0;
    return 1;
  }
  static char *mulib_html_decode_(const char *s, size_t n) {
    mulib_strbuf_t b; size_t i = 0;
    mulib_strbuf_init(&b);
    while (i < n) {
      if (s[i] == '&') {
        size_t begin = ++i, j; uint32_t cp = 0; unsigned char bytes[4]; size_t used;
        while (i < n && s[i] != ';' && i-begin < 32) ++i;
        if (i == n || s[i] != ';') goto fail;
        if (i > begin && s[begin] == '#') {
          unsigned base = 10; j = begin+1;
          if (j < i && (s[j] == 'x' || s[j] == 'X')) { base = 16; ++j; }
          if (j == i) goto fail;
          for (; j < i; ++j) {
            unsigned d = s[j] >= '0' && s[j] <= '9' ? (unsigned)(s[j]-'0') :
              s[j] >= 'a' && s[j] <= 'f' ? (unsigned)(s[j]-'a'+10) :
              s[j] >= 'A' && s[j] <= 'F' ? (unsigned)(s[j]-'A'+10) : 16;
            if (d >= base || cp > (0x10ffffu-d)/base) goto fail;
            cp = cp*base+d;
          }
        } else {
          static const char *const names[] = {"amp", "lt", "gt", "quot", "apos", "nbsp"};
          static const uint32_t codes[] = {38, 60, 62, 34, 39, 160};
          for (j = 0; j < 6; ++j) if (strlen(names[j]) == i-begin && !memcmp(s+begin, names[j], i-begin)) break;
          if (j == 6) goto fail;
          cp = codes[j];
        }
        used = mulib_utf8_encode(cp, bytes, sizeof bytes);
        if (!used || !mulib_xml_valid_text((const char *)bytes, used) ||
            !mulib_strbuf_append_n(&b, (const char *)bytes, used)) goto fail;
        ++i;
      } else if (s[i] == '\r') {
        ++i; if (i < n && s[i] == '\n') ++i;
        if (!mulib_strbuf_putc(&b, '\n')) goto fail;
      } else if (!mulib_strbuf_putc(&b, s[i++])) goto fail;
    }
    if (!mulib_strbuf_append_n(&b, "", 0)) goto fail;
    return b.data;
  fail:
    mulib_strbuf_free(&b); return NULL;
  }
  mulib_xml_node_t *mulib_html_parse(const char *s, size_t n, int fragment, mulib_xml_error_t *error) {
    mulib_xml_node_t *root = NULL, *parent, *pending = NULL;
    size_t p = 0, depth = 0, nodes = 0;
    int doctype = 0, strip_lf = 0;
    char *name = NULL, *key = NULL, *value = NULL;
    const char *message = "invalid or unsupported HTML";
    if (error) { error->offset = 0; error->message = NULL; }
    if (n > 16u*1024u*1024u || (fragment != 0 && fragment != 1) || !mulib_xml_valid_text(s, n)) goto fail;
    root = mulib_xml_new(fragment ? MULIB_XML_ELEMENT : MULIB_XML_DOCUMENT, fragment ? "div" : NULL);
    if (!root) goto fail;
    parent = root; depth = fragment ? 1 : 0;
    while (p < n) {
      size_t begin;
      if (++nodes > 1000000) { message = "HTML node limit"; goto fail; }
      if (s[p] != '<') {
        begin = p; while (p < n && s[p] != '<') ++p;
        value = mulib_html_decode_(s+begin, p-begin);
        if (!value) { message = "invalid entity or allocation failure"; goto fail; }
        if (strip_lf && value[0] == '\n') memmove(value, value+1, strlen(value));
        strip_lf = 0;
        if (!fragment && parent == root) {
          size_t i;
          for (i = 0; value[i]; ++i) if (!mulib_html_space_(value[i])) goto fail;
          free(value); value = NULL; continue;
        }
        pending = mulib_xml_new(MULIB_XML_TEXT, value); free(value); value = NULL;
        if (!pending || !mulib_xml_append(parent, pending)) goto fail;
        pending = NULL; continue;
      }
      strip_lf = 0;
      if (n-p >= 4 && !memcmp(s+p, "<!--", 4)) {
        if (mulib_xml_name(parent) && (!strcmp(mulib_xml_name(parent), "title") ||
            !strcmp(mulib_xml_name(parent), "textarea"))) goto fail;
        p += 4; begin = p;
        while (n-p >= 3 && memcmp(s+p, "-->", 3)) ++p;
        if (n-p < 3) { message = "unterminated comment"; goto fail; }
        value = mulib_html_copy_(s+begin, p-begin, 0); p += 3;
        if (!value || *value == '>' || !strncmp(value, "->", 2) || strstr(value, "<!-") ||
            (*value && value[strlen(value)-1] == '<')) goto fail;
        pending = mulib_xml_new(MULIB_XML_COMMENT, value); free(value); value = NULL;
        if (!pending || !mulib_xml_append(parent, pending)) goto fail;
        pending = NULL; continue;
      }
      if (n-p >= 9 && mulib_html_equal_(s+p, 9, "<!doctype")) {
        if (fragment || doctype || parent != root || mulib_xml_root(root)) goto fail;
        p += 9;
        if (p == n || !mulib_html_space_(s[p])) goto fail;
        while (p < n && mulib_html_space_(s[p])) ++p;
        begin = p; while (p < n && !mulib_html_space_(s[p]) && s[p] != '>') ++p;
        if (!mulib_html_equal_(s+begin, p-begin, "html")) goto fail;
        while (p < n && mulib_html_space_(s[p])) ++p;
        if (p == n || s[p++] != '>') goto fail;
        doctype = 1; continue;
      }
      ++p;
      {
        int closing = p < n && s[p] == '/', self = 0;
        if (closing) ++p;
        begin = p;
        while (p < n && !mulib_html_space_(s[p]) && s[p] != '>' && s[p] != '/') ++p;
        name = mulib_html_copy_(s+begin, p-begin, 1);
        if (!name || !mulib_html_name_internal(name) || mulib_html_raw_internal(name) ||
            !strcmp(name, "svg") || !strcmp(name, "math")) goto fail;
        if (closing) {
          while (p < n && mulib_html_space_(s[p])) ++p;
          if (p == n || s[p++] != '>' || parent == root || strcmp(name, mulib_xml_name(parent))) goto fail;
          parent = mulib_xml_parent(parent); --depth; free(name); name = NULL; continue;
        }
        if (mulib_xml_name(parent) && (!strcmp(mulib_xml_name(parent), "title") ||
            !strcmp(mulib_xml_name(parent), "textarea"))) goto fail;
        if (depth >= MULIB_XML_DEPTH_LIMIT) { message = "HTML depth limit"; goto fail; }
        pending = mulib_xml_new(MULIB_XML_ELEMENT, name);
        if (!pending) goto fail;
        for (;;) {
          size_t spaces = p, after_key;
          while (p < n && mulib_html_space_(s[p])) ++p;
          if (p == n) goto fail;
          if (s[p] == '>') { ++p; break; }
          if (s[p] == '/' && n-p >= 2 && s[p+1] == '>') { p += 2; self = 1; break; }
          if (p == spaces) goto fail;
          begin = p;
          while (p < n && !mulib_html_space_(s[p]) && s[p] != '=' && s[p] != '>' && s[p] != '/') ++p;
          key = mulib_html_copy_(s+begin, p-begin, 1);
          if (!key || !mulib_html_name_internal(key) || mulib_xml_attr(pending, key)) goto fail;
          after_key = p;
          while (p < n && mulib_html_space_(s[p])) ++p;
          if (p < n && s[p] == '=') {
            char quote = 0;
            ++p; while (p < n && mulib_html_space_(s[p])) ++p;
            if (p < n && (s[p] == '\'' || s[p] == '"')) quote = s[p++];
            begin = p;
            while (p < n && (quote ? s[p] != quote : !mulib_html_space_(s[p]) && s[p] != '>')) {
              if (s[p] == '<' || (!quote && (s[p] == '\'' || s[p] == '"' || s[p] == '`' || s[p] == '='))) goto fail;
              ++p;
            }
            if (p == n || (!quote && p == begin)) goto fail;
            value = mulib_html_decode_(s+begin, p-begin);
            if (quote) ++p;
          } else { p = after_key; value = mulib_html_copy_("", 0, 0); }
          if (!value || !mulib_xml_set_attr(pending, key, value)) goto fail;
          free(key); key = NULL; free(value); value = NULL;
        }
        if (self && !mulib_html_void_internal(name)) { message = "non-void self-closing tag"; goto fail; }
        if (!mulib_xml_append(parent, pending)) goto fail;
        if (!mulib_html_void_internal(name)) {
          parent = pending; ++depth;
          strip_lf = !strcmp(name, "pre") || !strcmp(name, "textarea") || !strcmp(name, "listing");
        }
        pending = NULL; free(name); name = NULL;
      }
    }
    if (parent != root || (!fragment && (!mulib_xml_root(root) || strcmp(mulib_xml_name(mulib_xml_root(root)), "html")))) goto fail;
    return root;
  fail:
    if (error) { error->offset = p; error->message = message; }
    free(name); free(key); free(value); mulib_xml_destroy(pending); mulib_xml_destroy(root); return NULL;
  }
  static int mulib_html_text_(const mulib_xml_node_t *node, mulib_strbuf_t *out, size_t depth) {
    const char *v = mulib_xml_value(node), *name = mulib_xml_name(node);
    mulib_xml_node_t *c;
    if (v && mulib_xml_type(node) != MULIB_XML_COMMENT) return mulib_strbuf_append(out, v);
    if (depth > MULIB_XML_DEPTH_LIMIT) return 0;
    if (name && !strcmp(name, "br")) return mulib_strbuf_putc(out, '\n');
    for (c = mulib_xml_first(node); c; c = mulib_xml_next(c)) if (!mulib_html_text_(c, out, depth+1)) return 0;
    return 1;
  }
  char *mulib_html_text(const mulib_xml_node_t *node, size_t *length) {
    mulib_strbuf_t out; mulib_strbuf_init(&out);
    if (length) *length = 0;
    if (!node || !mulib_html_text_(node, &out, 0) || !mulib_strbuf_append_n(&out, "", 0)) {
      mulib_strbuf_free(&out); return NULL;
    }
    if (length) *length = out.length;
    return out.data;
  }
int mulib_html_docir(const mulib_docir_node_t *root,const char *title,const char *language,mulib_strbuf_t *out) {
#ifdef MULIB_ENABLE_DOCIR
    mulib_docir_snapshot_t snapshot={0};mulib_typecore_buf_t body={0};mulib_strbuf_t result={0};int ok=0;
    if(!language)language="und";
    size_t n=strlen(language);
    if(!out||!n||n>63)return 0;
    for(size_t i=0;i<n;i++)if(!((language[i]>='A'&&language[i]<='Z')||(language[i]>='a'&&language[i]<='z')||
       (i&&((language[i]>='0'&&language[i]<='9')||language[i]=='-'))))return 0;
    if(!mulib_docir_snapshot_make(root,NULL,&snapshot,NULL))return 0;
    if(!mulib_docir_snapshot_validate(&snapshot,mulib_docir_CHECK_DOCUMENT|mulib_docir_CHECK_REFERENCES,NULL)||
       !mulib_docir_render_snapshot(&snapshot,mulib_docir_FORMAT_HTML,&body))goto done;
    if(!mulib_strbuf_append(&result,"<!DOCTYPE html>\n<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"")||
       !mulib_strbuf_append(&result,language)||!mulib_strbuf_append(&result,"\" xml:lang=\"")||
       !mulib_strbuf_append(&result,language)||!mulib_strbuf_append(&result,"\"><head><meta charset=\"utf-8\" /><title>")||
       !mulib_html_escape(&result,title?title:"",title?strlen(title):0,0)||
       !mulib_strbuf_append(&result,"</title></head><body>")||
       !mulib_strbuf_append_n(&result,(const char*)body.data,body.size)||
       !mulib_strbuf_append(&result,"</body></html>\n")||!mulib_strbuf_append_n(out,result.data,result.length))goto done;
    ok=1;
done:mulib_strbuf_free(&result);mulib_typecore_free(&body);mulib_docir_snapshot_destroy(&snapshot);return ok;
#else
    (void)root;(void)title;(void)language;(void)out;return 0;
#endif
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
