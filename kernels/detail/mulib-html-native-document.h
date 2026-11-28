/* Native tree construction for the profile in manual/html-native-dom-profile.md.
 * This implementation owns no intermediate tree and invokes no callbacks. */
#include "mulib-html-native-tokenizer.h"
#include "../mulib-html-entities.h"
#include <stdlib.h>

enum mulib_hn_mode_
{
  HN_BEFORE_HTML_, HN_BEFORE_HEAD_, HN_IN_HEAD_, HN_AFTER_HEAD_,
  HN_IN_BODY_, HN_AFTER_BODY_, HN_AFTER_HTML_
};

typedef struct mulib_hn_builder_
{
  mulib_dom_document_t *document;
  mulib_dom_node_t *html, *head, *body, *active;
  const mulib_html_dombridge_limits_t *limits;
  size_t nodes, depth;
  enum mulib_hn_mode_ mode;
  const char *raw;
  muba_buf tail;
  int seen_doctype;
  mulib_html_dombridge_status_t status;
} mulib_hn_builder_;

static int
mulib_hn_is_ (const char *tag, const char *word)
{
  return !strcmp (tag, word);
}

/* A space-delimited list, matched as complete names. */
static int
mulib_hn_one_of_ (const char *tag, const char *list)
{
  size_t k = strlen (tag);
  while (*list)
    {
      const char *end = strchr (list, ' ');
      size_t len = end ? (size_t)(end - list) : strlen (list);
      if (k == len && !memcmp (tag, list, len))
        return 1;
      if (!end)
        break;
      list = end + 1;
    }
  return 0;
}

static int
mulib_hn_fail_ (mulib_hn_builder_ *b, mulib_html_dombridge_status_t status)
{
  b->status = status;
  return 0;
}

static int
mulib_hn_dom_fail_ (mulib_hn_builder_ *b, mulib_dom_error_t e)
{
  return mulib_hn_fail_ (b, e == MULIB_DOM_LIMIT_ERR
                               ? MULIB_HTML_DOMBRIDGE_LIMIT
                           : e == MULIB_DOM_OUT_OF_MEMORY
                               ? MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY
                               : MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
}

static int
mulib_hn_room_ (mulib_hn_builder_ *b, size_t depth)
{
  if (b->nodes >= b->limits->max_nodes || b->nodes >= MULIB_DOM_NODE_LIMIT
      || depth > b->limits->max_depth || depth > MULIB_DOM_DEPTH_LIMIT)
    return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_LIMIT);
  ++b->nodes;
  return 1;
}

static int
mulib_hn_append_ (mulib_hn_builder_ *b, mulib_dom_node_t *parent,
                  mulib_dom_node_t *node, mulib_dom_error_t e)
{
  if (!node || !mulib_dom_append_child (parent, node, &e))
    return mulib_hn_dom_fail_ (b, e);
  return 1;
}

static int
mulib_hn_chars_ (mulib_hn_builder_ *b, mulib_dom_node_t *parent, size_t depth,
                 mulib_dom_type_t type, mulib_hn_slice_ s)
{
  if ((type == MULIB_DOM_TEXT && !s.len))
    return 1;
  if (!mulib_hn_room_ (b, depth))
    return 0;
  mulib_buffer_t utf16 = { 0 };
  if (!mulib_dom_utf8_to_string (s.data, s.len, &utf16))
    return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY);
  mulib_dom_error_t e;
  mulib_dom_string_t str = { (const uint16_t *)utf16.data,
                             utf16.length / sizeof (uint16_t) };
  mulib_dom_node_t *node =
      mulib_dom_create_character_data (b->document, type, NULL, str, &e);
  mulib_buffer_free (&utf16);
  return mulib_hn_append_ (b, parent, node, e);
}

static mulib_dom_node_t *
mulib_hn_element_ (mulib_hn_builder_ *b, mulib_dom_node_t *parent, size_t depth,
                   const char *tag, const mulib_hn_token_ *t)
{
  if (!mulib_hn_room_ (b, depth))
    return NULL;
  mulib_dom_error_t e;
  mulib_dom_node_t *node = mulib_dom_create_element (
      b->document, MULIB_DOM_HTML_NAMESPACE, tag, &e);
  if (!mulib_hn_append_ (b, parent, node, e))
    return NULL;
  for (size_t i = 0; t && i < t->attrs; ++i)
    {
      const char *name = t->attr[i].name;
      if (mulib_hn_is_ (name, "xmlns"))
        {
          mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
          return NULL;
        }
      if (mulib_dom_get_attribute_node (node, NULL, name))
        continue; /* First spelling wins after ASCII folding. */
      if (!mulib_hn_room_ (b, depth))
        return NULL;
      mulib_hn_slice_ value = t->attr[i].value;
      muba_buf decoded = { 0 };
      mulib_buffer_t utf16 = { 0 };
      int converted = mulib_html_entities_decode_context (
          (muba_span){ (const unsigned char *)value.data, value.len }, 1,
          &decoded)
          && mulib_dom_utf8_to_string ((const char *)decoded.data, decoded.len,
                                       &utf16);
      muba_free (&decoded);
      if (!converted)
        {
          mulib_buffer_free (&utf16);
          mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY);
          return NULL;
        }
      mulib_dom_string_t str = { (const uint16_t *)utf16.data,
                                 utf16.length / sizeof (uint16_t) };
      int ok = mulib_dom_set_attribute (node, NULL, name, str, &e);
      mulib_buffer_free (&utf16);
      if (!ok)
        {
          mulib_hn_dom_fail_ (b, e);
          return NULL;
        }
    }
  return node;
}

static void
mulib_hn_pop_ (mulib_hn_builder_ *b)
{
  b->active = mulib_dom_parent (b->active);
  --b->depth;
}

#define MULIB_HN_BLOCK_ \
  "address article aside blockquote div footer header main nav section " \
  "h1 h2 h3 h4 h5 h6 pre p ul ol li dl dt dd hr"
#define MULIB_HN_INLINE_ \
  "a b br code em i img input small span strong sub sup textarea u wbr"
#define MULIB_HN_BODY_VOID_ "br hr img input wbr"
#define MULIB_HN_HEAD_VOID_ "base link meta"
#define MULIB_HN_OPTIONAL_ "p li dt dd"

static int
mulib_hn_body_ (mulib_hn_builder_ *b, const mulib_hn_token_ *t,
                 mulib_hn_slice_ text)
{
  const char *tag = t->name;
  const char *top = mulib_dom_local_name (b->active);
  if (t->kind == HN_TEXT_)
    {
      if (b->raw && mulib_hn_is_ (b->raw, "textarea") && text.len
          && text.data[0] == '\n')
        ++text.data, --text.len;
      /* HTML pre ignores a leading LF before its first child. */
      if (mulib_hn_is_ (top, "pre") && !mulib_dom_first_child (b->active)
          && text.len && text.data[0] == '\n')
        ++text.data, --text.len;
      return mulib_hn_chars_ (b, b->active, b->depth + 1, MULIB_DOM_TEXT, text);
    }
  if (t->kind == HN_EOF_)
    {
      while (b->active != b->body
             && mulib_hn_one_of_ (mulib_dom_local_name (b->active),
                                  MULIB_HN_OPTIONAL_))
        mulib_hn_pop_ (b);
      return b->active == b->body
             || mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_INVALID_INPUT);
    }
  if (t->kind == HN_END_)
    {
      /* These envelope closes change insertion mode, not the open stack.
         Trailing whitespace is still inserted at the current body node. */
      if (mulib_hn_one_of_ (tag, "body html")
          && (b->active == b->body
              || (mulib_hn_is_ (top, "p")
                  && mulib_dom_parent (b->active) == b->body)))
        {
          b->mode = mulib_hn_is_ (tag, "body") ? HN_AFTER_BODY_ : HN_AFTER_HTML_;
          return 1;
        }
      if (mulib_hn_is_ (top, "p") && !mulib_hn_is_ (tag, "p")
          && (mulib_hn_is_ (tag, mulib_dom_local_name (mulib_dom_parent (b->active)))
              || (mulib_hn_is_ (tag, "ul") || mulib_hn_is_ (tag, "ol")
                  || mulib_hn_is_ (tag, "dl"))))
        {
          mulib_hn_pop_ (b);
          top = mulib_dom_local_name (b->active);
        }
      if ((mulib_hn_is_ (top, "li") && mulib_hn_one_of_ (tag, "ol ul"))
          || (mulib_hn_one_of_ (top, "dt dd") && mulib_hn_is_ (tag, "dl")))
        {
          mulib_hn_pop_ (b);
          top = mulib_dom_local_name (b->active);
        }
      if (b->active == b->body || !mulib_hn_is_ (tag, top))
        return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_INVALID_INPUT);
      b->raw = NULL;
      mulib_hn_pop_ (b);
      return 1;
    }
  if (t->kind != HN_START_)
    return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
  int block = mulib_hn_one_of_ (tag, MULIB_HN_BLOCK_);
  if (!block && !mulib_hn_one_of_ (tag, MULIB_HN_INLINE_))
    return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
  int is_void = mulib_hn_one_of_ (tag, MULIB_HN_BODY_VOID_);
  if (t->slash && !is_void)
    return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
  if (block && mulib_hn_is_ (top, "p"))
    {
      mulib_hn_pop_ (b);
      top = mulib_dom_local_name (b->active);
    }
  int list_item = mulib_hn_is_ (tag, "li");
  int dl_item = mulib_hn_one_of_ (tag, "dt dd");
  if ((list_item && mulib_hn_is_ (top, "li"))
      || (dl_item && mulib_hn_one_of_ (top, "dt dd")))
    {
      mulib_hn_pop_ (b);
      top = mulib_dom_local_name (b->active);
    }
  if ((list_item && !mulib_hn_one_of_ (top, "ol ul"))
      || (dl_item && !mulib_hn_is_ (top, "dl"))
      || (block && mulib_hn_one_of_ (top, MULIB_HN_INLINE_ " h1 h2 h3 h4 h5 h6 pre")))
    return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
  if (mulib_hn_is_ (tag, "a"))
    for (mulib_dom_node_t *n = b->active; n != b->body; n = mulib_dom_parent (n))
      if (mulib_hn_is_ (mulib_dom_local_name (n), "a"))
        return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
  mulib_dom_node_t *node = mulib_hn_element_ (b, b->active, b->depth + 1, tag, t);
  if (!node)
    return 0;
  if (!is_void)
    {
      b->active = node;
      ++b->depth;
      if (mulib_hn_is_ (tag, "textarea"))
        b->raw = "textarea";
    }
  return 1;
}

/* After </body>, whitespace still belongs to body. Stage it across trailing
   comments and do at most one CharacterData replacement at EOF. */
static int
mulib_hn_tail_ (mulib_hn_builder_ *b)
{
  if (!b->tail.len)
    return 1;
  mulib_hn_slice_ s = { (const char *)b->tail.data, b->tail.len };
  mulib_dom_node_t *last = mulib_dom_last_child (b->active);
  if (mulib_dom_node_type (last) != MULIB_DOM_TEXT)
    return mulib_hn_chars_ (b, b->active, b->depth + 1, MULIB_DOM_TEXT, s);
  mulib_buffer_t utf16 = { 0 };
  if (!mulib_dom_utf8_to_string (s.data, s.len, &utf16))
    return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY);
  mulib_dom_string_t str = { (const uint16_t *)utf16.data,
                             utf16.length / sizeof (uint16_t) };
  mulib_dom_error_t e;
  int ok = mulib_dom_replace_data (last, mulib_dom_node_value (last).length, 0,
                                   str, &e);
  mulib_buffer_free (&utf16);
  return ok || mulib_hn_dom_fail_ (b, e);
}

static int
mulib_hn_build_ (mulib_hn_builder_ *b, const mulib_hn_token_ *t,
                  mulib_hn_slice_ text)
{
  mulib_dom_node_t *root = mulib_dom_document_node (b->document);
  if (t->kind == HN_COMMENT_)
    {
      if (b->mode == HN_AFTER_BODY_)
        return mulib_hn_chars_ (b, b->html, 3, MULIB_DOM_COMMENT, t->text);
      if (b->mode == HN_BEFORE_HTML_ || b->mode == HN_AFTER_HTML_)
        return mulib_hn_chars_ (b, root, 2, MULIB_DOM_COMMENT, t->text);
      return mulib_hn_chars_ (b, b->active, b->depth + 1,
                              MULIB_DOM_COMMENT, t->text);
    }
  if (t->kind == HN_DOCTYPE_)
    {
      if (b->mode != HN_BEFORE_HTML_ || b->seen_doctype)
        return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
      if (!mulib_hn_room_ (b, 2))
        return 0;
      b->seen_doctype = 1;
      mulib_dom_error_t e;
      mulib_dom_node_t *n = mulib_dom_create_doctype (b->document, "html", "", "", &e);
      return mulib_hn_append_ (b, root, n, e);
    }
  for (;;)
    {
      if (t->kind == HN_TEXT_ && b->mode != HN_IN_BODY_ && !b->raw)
        {
          size_t n = 0;
          while (n < text.len && mulib_hn_space_ (text.data[n]))
            ++n;
          if (n && b->mode == HN_IN_HEAD_
              && !mulib_hn_chars_ (b, b->head, 4, MULIB_DOM_TEXT,
                                   (mulib_hn_slice_){ text.data, n }))
            return 0;
          if (n && b->mode == HN_AFTER_HEAD_
              && !mulib_hn_chars_ (b, b->html, 3, MULIB_DOM_TEXT,
                                   (mulib_hn_slice_){ text.data, n }))
            return 0;
          if (b->mode != HN_AFTER_BODY_ && b->mode != HN_AFTER_HTML_)
            text.data += n, text.len -= n;
          if (b->mode == HN_AFTER_BODY_ || b->mode == HN_AFTER_HTML_)
            {
              if (n != text.len)
                return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
              return muba_append (
                         &b->tail,
                         (muba_span){ (const unsigned char *)text.data, text.len })
                     || mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY);
            }
          if (!text.len)
            return 1;
        }
      const char *tag = t->name;
      switch (b->mode)
        {
        case HN_BEFORE_HTML_:
        case HN_BEFORE_HEAD_:
        case HN_AFTER_HEAD_:
          {
            const char *wanted = b->mode == HN_BEFORE_HTML_ ? "html"
                                 : b->mode == HN_BEFORE_HEAD_ ? "head" : "body";
            int explicit_tag = t->kind == HN_START_ && mulib_hn_is_ (tag, wanted);
            if (explicit_tag && t->slash)
              return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
            mulib_dom_node_t *parent = b->mode == HN_BEFORE_HTML_ ? root : b->html;
            b->depth = parent == root ? 2 : 3;
            b->active = mulib_hn_element_ (b, parent, b->depth, wanted,
                                           explicit_tag ? t : NULL);
            if (!b->active)
              return 0;
            if (b->mode == HN_BEFORE_HTML_)
              b->html = b->active, b->mode = HN_BEFORE_HEAD_;
            else if (b->mode == HN_BEFORE_HEAD_)
              b->head = b->active, b->mode = HN_IN_HEAD_;
            else
              b->body = b->active, b->mode = HN_IN_BODY_;
            if (explicit_tag)
              return 1;
            break;
          }
        case HN_IN_HEAD_:
          if (b->raw)
            {
              if (t->kind == HN_TEXT_)
                return mulib_hn_chars_ (b, b->active, 5, MULIB_DOM_TEXT, text);
              if (t->kind != HN_END_ || !mulib_hn_is_ (tag, b->raw))
                return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_INVALID_INPUT);
              b->raw = NULL;
              mulib_hn_pop_ (b);
              return 1;
            }
          if (t->kind == HN_START_ && mulib_hn_one_of_ (tag, MULIB_HN_HEAD_VOID_))
            return mulib_hn_element_ (b, b->head, 4, tag, t) != NULL;
          if (t->kind == HN_START_ && mulib_hn_one_of_ (tag, "title style"))
            {
              if (t->slash)
                return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
              b->active = mulib_hn_element_ (b, b->head, 4, tag, t);
              if (!b->active)
                return 0;
              b->depth = 4;
              b->raw = mulib_hn_is_ (tag, "title") ? "title" : "style";
              return 1;
            }
          b->mode = HN_AFTER_HEAD_;
          mulib_hn_pop_ (b);
          if (t->kind == HN_END_ && mulib_hn_is_ (tag, "head"))
            return 1;
          break;
        case HN_IN_BODY_:
          return mulib_hn_body_ (b, t, text);
        case HN_AFTER_BODY_:
          if (t->kind == HN_END_ && mulib_hn_is_ (tag, "html"))
            {
              b->mode = HN_AFTER_HTML_;
              return 1;
            }
          /* Fall through. */
        case HN_AFTER_HTML_:
          if (t->kind == HN_EOF_)
            return mulib_hn_tail_ (b);
          return mulib_hn_fail_ (b, MULIB_HTML_DOMBRIDGE_UNSUPPORTED);
        }
    }
}

mulib_html_dombridge_status_t
mulib_html_dombridge_parse (const char *html, size_t length,
                           const mulib_html_dombridge_limits_t *limits,
                           mulib_dom_document_t **out)
{
  if ((!html && length) || !limits || !out)
    return MULIB_HTML_DOMBRIDGE_INVALID_ARGUMENT;
  if (length > limits->max_bytes || length > MULIB_DOM_STRING_LIMIT
      || !limits->max_depth || !limits->max_nodes)
    return MULIB_HTML_DOMBRIDGE_LIMIT;
  if (length && (memchr (html, 0, length)
                 || !mulib_utf8_validate ((const unsigned char *)html, length)))
    return MULIB_HTML_DOMBRIDGE_INVALID_INPUT;
  if (length >= 3 && !memcmp (html, "\xef\xbb\xbf", 3))
    html += 3, length -= 3;
  char *normalized = NULL;
  if (length && memchr (html, '\r', length))
    {
      normalized = malloc (length);
      if (!normalized)
        return MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY;
      size_t written = 0;
      for (size_t i = 0; i < length; ++i)
        if (html[i] == '\r')
          {
            normalized[written++] = '\n';
            if (i + 1 < length && html[i + 1] == '\n')
              ++i;
          }
        else
          normalized[written++] = html[i];
      html = normalized;
      length = written;
    }
  mulib_hn_builder_ b = { 0 };
  b.document = mulib_dom_document_new ();
  b.limits = limits;
  b.nodes = 1; /* Document node counts toward the budget. */
  b.depth = 1;
  size_t cursor = 0;
  if (!b.document)
    b.status = MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY;
  while (b.status == MULIB_HTML_DOMBRIDGE_OK)
    {
      mulib_hn_token_ token;
      b.status = mulib_hn_lex_ (html, length, &cursor, b.raw, &token);
      if (b.status != MULIB_HTML_DOMBRIDGE_OK)
        break;
      mulib_hn_slice_ text = { "", 0 };
      muba_buf decoded = { 0 };
      if (token.kind == HN_TEXT_)
        {
          text = token.text;
          if ((!b.raw || !mulib_hn_is_ (b.raw, "style"))
              && memchr (text.data, '&', text.len))
            {
              if (!mulib_html_entities_decode (
                      (muba_span){ (const unsigned char *)text.data, text.len },
                      &decoded))
                {
                  b.status = MULIB_HTML_DOMBRIDGE_OUT_OF_MEMORY;
                  break;
                }
              text = (mulib_hn_slice_){ (const char *)decoded.data, decoded.len };
            }
        }
      int ok = mulib_hn_build_ (&b, &token, text);
      muba_free (&decoded);
      if (!ok || token.kind == HN_EOF_)
        break;
    }
  free (normalized);
  muba_free (&b.tail);
  if (b.status == MULIB_HTML_DOMBRIDGE_OK)
    *out = b.document;
  else
    mulib_dom_document_destroy (b.document);
  return b.status;
}
