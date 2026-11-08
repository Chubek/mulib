/** @file mulib-dom.h
 * @brief Native DOM trees, UTF-16 character data, attributes and event
 * dispatch. Define MULIB_IMPL_DOM plus BUFFER and UTF8 in one translation
 * unit. Documents own all their nodes, including detached nodes. See
 * manual/mulib-dom.md.
 */
#ifndef MULIB_DOM_H
#define MULIB_DOM_H
#include "mulib-buffer.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_dom_document mulib_dom_document_t;
  typedef struct mulib_dom_node mulib_dom_node_t;
  typedef struct mulib_dom_event mulib_dom_event_t;
  /** Borrowed DOMString; length counts UTF-16 units, including NUL/surrogates.
   */
  typedef struct mulib_dom_string
  {
    const uint16_t *data;
    size_t length;
  } mulib_dom_string_t;
  /** DOM nodeType values. Historical entity/notation nodes are not
   * constructed. */
  typedef enum mulib_dom_type
  {
    MULIB_DOM_ELEMENT = 1,
    MULIB_DOM_ATTRIBUTE = 2,
    MULIB_DOM_TEXT = 3,
    MULIB_DOM_CDATA = 4,
    MULIB_DOM_PROCESSING_INSTRUCTION = 7,
    MULIB_DOM_COMMENT = 8,
    MULIB_DOM_DOCUMENT = 9,
    MULIB_DOM_DOCUMENT_TYPE = 10,
    MULIB_DOM_DOCUMENT_FRAGMENT = 11
  } mulib_dom_type_t;
  /** Error values match DOMException legacy codes; allocation/limit are
   * native. */
  typedef enum mulib_dom_error
  {
    MULIB_DOM_OK = 0,
    MULIB_DOM_INDEX_SIZE_ERR = 1,
    MULIB_DOM_HIERARCHY_REQUEST_ERR = 3,
    MULIB_DOM_WRONG_DOCUMENT_ERR = 4,
    MULIB_DOM_INVALID_CHARACTER_ERR = 5,
    MULIB_DOM_NOT_FOUND_ERR = 8,
    MULIB_DOM_NOT_SUPPORTED_ERR = 9,
    MULIB_DOM_INUSE_ATTRIBUTE_ERR = 10,
    MULIB_DOM_INVALID_STATE_ERR = 11,
    MULIB_DOM_NAMESPACE_ERR = 14,
    MULIB_DOM_OUT_OF_MEMORY = 100,
    MULIB_DOM_LIMIT_ERR = 101
  } mulib_dom_error_t;
  enum
  {
    MULIB_DOM_DOCUMENT_POSITION_DISCONNECTED = 1,
    MULIB_DOM_DOCUMENT_POSITION_PRECEDING = 2,
    MULIB_DOM_DOCUMENT_POSITION_FOLLOWING = 4,
    MULIB_DOM_DOCUMENT_POSITION_CONTAINS = 8,
    MULIB_DOM_DOCUMENT_POSITION_CONTAINED_BY = 16,
    MULIB_DOM_DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC = 32,
    MULIB_DOM_NONE = 0,
    MULIB_DOM_CAPTURING_PHASE = 1,
    MULIB_DOM_AT_TARGET = 2,
    MULIB_DOM_BUBBLING_PHASE = 3
  };
#define MULIB_DOM_NODE_LIMIT 65536
#define MULIB_DOM_DEPTH_LIMIT 256
#define MULIB_DOM_STRING_LIMIT 16777216
#define MULIB_DOM_XML_NAMESPACE "http://www.w3.org/XML/1998/namespace"
#define MULIB_DOM_XMLNS_NAMESPACE "http://www.w3.org/2000/xmlns/"
  /** Allocate an empty XML-style document. NULL on allocation failure. */
  mulib_dom_document_t *mulib_dom_document_new (void);
  /** Free all nodes/listeners. Returns 0 during dispatch; NULL is harmless. */
  int mulib_dom_document_destroy (mulib_dom_document_t *document);
  /** Borrow the document node, document element, or owner document. */
  mulib_dom_node_t *
  mulib_dom_document_node (const mulib_dom_document_t *document);
  mulib_dom_node_t *
  mulib_dom_document_element (const mulib_dom_document_t *document);
  mulib_dom_document_t *
  mulib_dom_owner_document (const mulib_dom_node_t *node);
  /** Create detached element/attribute; namespace NULL/empty means no
   * namespace. Names use ASCII XML qualified-name syntax. Errors are optional
   * and cleared. */
  mulib_dom_node_t *mulib_dom_create_element (mulib_dom_document_t *,
                                              const char *namespace_uri,
                                              const char *qualified_name,
                                              mulib_dom_error_t *);
  mulib_dom_node_t *mulib_dom_create_attribute (mulib_dom_document_t *,
                                                const char *namespace_uri,
                                                const char *qualified_name,
                                                mulib_dom_error_t *);
  /** Create text, CDATA, comment or processing instruction (target required).
   * Copies the UTF-16 span; arbitrary code units are preserved. */
  mulib_dom_node_t *mulib_dom_create_character_data (mulib_dom_document_t *,
                                                     mulib_dom_type_t,
                                                     const char *target,
                                                     mulib_dom_string_t,
                                                     mulib_dom_error_t *);
  /** UTF-8 convenience constructor; invalid UTF-8 fails. */
  mulib_dom_node_t *mulib_dom_create_text_utf8 (mulib_dom_document_t *,
                                                const char *, size_t,
                                                mulib_dom_error_t *);
  /** Create an empty fragment, or a doctype with copied identifiers. */
  mulib_dom_node_t *mulib_dom_create_fragment (mulib_dom_document_t *,
                                               mulib_dom_error_t *);
  mulib_dom_node_t *mulib_dom_create_doctype (mulib_dom_document_t *,
                                              const char *name,
                                              const char *public_id,
                                              const char *system_id,
                                              mulib_dom_error_t *);
  /** Accessors return borrowed pointers; NULL input returns zero/NULL/empty.
   */
  mulib_dom_type_t mulib_dom_node_type (const mulib_dom_node_t *);
  const char *mulib_dom_node_name (const mulib_dom_node_t *);
  const char *mulib_dom_namespace_uri (const mulib_dom_node_t *);
  const char *mulib_dom_prefix (const mulib_dom_node_t *);
  const char *mulib_dom_local_name (const mulib_dom_node_t *);
  const char *mulib_dom_public_id (const mulib_dom_node_t *);
  const char *mulib_dom_system_id (const mulib_dom_node_t *);
  mulib_dom_node_t *mulib_dom_parent (const mulib_dom_node_t *);
  mulib_dom_node_t *mulib_dom_first_child (const mulib_dom_node_t *);
  mulib_dom_node_t *mulib_dom_last_child (const mulib_dom_node_t *);
  mulib_dom_node_t *mulib_dom_next_sibling (const mulib_dom_node_t *);
  mulib_dom_node_t *mulib_dom_previous_sibling (const mulib_dom_node_t *);
  mulib_dom_node_t *mulib_dom_get_root_node (const mulib_dom_node_t *);
  int mulib_dom_is_connected (const mulib_dom_node_t *);
  int mulib_dom_contains (const mulib_dom_node_t *, const mulib_dom_node_t *);
  /** Live childNodes access (no cached list or ownership transfer). */
  size_t mulib_dom_child_count (const mulib_dom_node_t *);
  mulib_dom_node_t *mulib_dom_child_at (const mulib_dom_node_t *, size_t);
  /** Tree edits are atomic on failure; insertion moves/adopts nodes and
   * empties fragments. Return inserted node (removed node for replace/remove),
   * or NULL. */
  mulib_dom_node_t *mulib_dom_insert_before (mulib_dom_node_t *,
                                             mulib_dom_node_t *,
                                             mulib_dom_node_t *reference,
                                             mulib_dom_error_t *);
  mulib_dom_node_t *mulib_dom_append_child (mulib_dom_node_t *,
                                            mulib_dom_node_t *,
                                            mulib_dom_error_t *);
  mulib_dom_node_t *mulib_dom_replace_child (mulib_dom_node_t *,
                                             mulib_dom_node_t *,
                                             mulib_dom_node_t *old_child,
                                             mulib_dom_error_t *);
  mulib_dom_node_t *mulib_dom_remove_child (mulib_dom_node_t *,
                                            mulib_dom_node_t *,
                                            mulib_dom_error_t *);
  /** Adopt detaches; import/clone copy attributes and optionally descendants.
   * Listeners are not cloned. Documents cannot be adopted/imported/cloned. */
  mulib_dom_node_t *mulib_dom_adopt_node (mulib_dom_document_t *,
                                          mulib_dom_node_t *,
                                          mulib_dom_error_t *);
  mulib_dom_node_t *mulib_dom_import_node (mulib_dom_document_t *,
                                           const mulib_dom_node_t *, int deep,
                                           mulib_dom_error_t *);
  mulib_dom_node_t *mulib_dom_clone_node (const mulib_dom_node_t *, int deep,
                                          mulib_dom_error_t *);
  int mulib_dom_is_equal_node (const mulib_dom_node_t *,
                               const mulib_dom_node_t *);
  /** CompareDocumentPosition flags: disconnected=1, preceding=2, following=4,
   * contains=8, contained-by=16, implementation-specific=32. */
  unsigned mulib_dom_compare_document_position (const mulib_dom_node_t *,
                                                const mulib_dom_node_t *);
  /** Attr/CharacterData nodeValue, empty span otherwise. */
  mulib_dom_string_t mulib_dom_node_value (const mulib_dom_node_t *);
  int mulib_dom_set_node_value (mulib_dom_node_t *, mulib_dom_string_t,
                                mulib_dom_error_t *);
  /** Append UTF-16 textContent atomically to a byte buffer (no terminator).
   * Document/doctype yield empty output; element/fragment exclude comments/PI.
   */
  int mulib_dom_text_content (const mulib_dom_node_t *, mulib_buffer_t *,
                              mulib_dom_error_t *);
  int mulib_dom_set_text_content (mulib_dom_node_t *, mulib_dom_string_t,
                                  mulib_dom_error_t *);
  /** CharacterData offsets/counts are UTF-16 units; count is clamped. */
  int mulib_dom_substring_data (const mulib_dom_node_t *, size_t offset,
                                size_t count, mulib_buffer_t *,
                                mulib_dom_error_t *);
  int mulib_dom_replace_data (mulib_dom_node_t *, size_t offset, size_t count,
                              mulib_dom_string_t, mulib_dom_error_t *);
  mulib_dom_node_t *mulib_dom_split_text (mulib_dom_node_t *, size_t offset,
                                          mulib_dom_error_t *);
  /** Merge adjacent Text nodes and detach empty ones, atomically. */
  int mulib_dom_normalize (mulib_dom_node_t *, mulib_dom_error_t *);
  /** NamedNodeMap access in insertion order; Attr has no parentNode. */
  size_t mulib_dom_attribute_count (const mulib_dom_node_t *);
  mulib_dom_node_t *mulib_dom_attribute_at (const mulib_dom_node_t *, size_t);
  mulib_dom_node_t *mulib_dom_get_attribute_node (const mulib_dom_node_t *,
                                                  const char *namespace_uri,
                                                  const char *local_name);
  mulib_dom_node_t *mulib_dom_owner_element (const mulib_dom_node_t *);
  int mulib_dom_set_attribute (mulib_dom_node_t *, const char *namespace_uri,
                               const char *qualified_name, mulib_dom_string_t,
                               mulib_dom_error_t *);
  int mulib_dom_set_attribute_node (mulib_dom_node_t *, mulib_dom_node_t *,
                                    mulib_dom_error_t *);
  int mulib_dom_remove_attribute (mulib_dom_node_t *,
                                  const char *namespace_uri,
                                  const char *local_name, mulib_dom_error_t *);
  /** Descendant-only tree-order query; UTF-16 id matches the no-namespace id.
   */
  mulib_dom_node_t *mulib_dom_get_element_by_id (const mulib_dom_node_t *,
                                                 mulib_dom_string_t);
  /** Atomic snapshot of borrowed element pointers; namespace/local '*'
   * wildcards. Free the buffer, not its entries. Mutations do not update
   * snapshots. */
  int mulib_dom_get_elements_by_tag_name (const mulib_dom_node_t *,
                                          const char *namespace_uri,
                                          const char *local_name,
                                          mulib_buffer_t *,
                                          mulib_dom_error_t *);
  /** Namespace lookup appends UTF-8 to out: 1 found, 0 absent, -1 error.
   * Empty prefix means the default namespace. No terminator is appended. */
  int mulib_dom_lookup_namespace_uri (const mulib_dom_node_t *,
                                      const char *prefix, mulib_buffer_t *out,
                                      mulib_dom_error_t *);
  /** UTF-8/UTF-16 conversions append atomically, without a terminator. UTF-16
   * unpaired surrogates become U+FFFD in UTF-8 (USVString conversion). */
  int mulib_dom_utf8_to_string (const char *, size_t, mulib_buffer_t *);
  int mulib_dom_string_to_utf8 (mulib_dom_string_t, mulib_buffer_t *);
  /** Listener identity is (type, callback, userdata, capture). */
  typedef void (*mulib_dom_listener_t) (mulib_dom_event_t *, void *userdata);
  typedef struct mulib_dom_listener_options
  {
    int capture, once, passive;
  } mulib_dom_listener_options_t;
  /** Events own their type string. detail is caller-owned CustomEvent payload.
   */
  mulib_dom_event_t *mulib_dom_event_new (const char *type, int bubbles,
                                          int cancelable, void *detail);
  /** Returns 0 for a dispatching event; NULL is harmless. */
  int mulib_dom_event_destroy (mulib_dom_event_t *);
  const char *mulib_dom_event_type (const mulib_dom_event_t *);
  void *mulib_dom_event_detail (const mulib_dom_event_t *);
  mulib_dom_node_t *mulib_dom_event_target (const mulib_dom_event_t *);
  mulib_dom_node_t *mulib_dom_event_current_target (const mulib_dom_event_t *);
  unsigned mulib_dom_event_phase (const mulib_dom_event_t *);
  int mulib_dom_event_bubbles (const mulib_dom_event_t *);
  int mulib_dom_event_cancelable (const mulib_dom_event_t *);
  /** Snapshot of target-to-root borrowed pointers, empty outside dispatch. */
  int mulib_dom_event_composed_path (const mulib_dom_event_t *,
                                     mulib_buffer_t *, mulib_dom_error_t *);
  int mulib_dom_event_default_prevented (const mulib_dom_event_t *);
  void mulib_dom_event_prevent_default (mulib_dom_event_t *);
  void mulib_dom_event_stop_propagation (mulib_dom_event_t *);
  void mulib_dom_event_stop_immediate_propagation (mulib_dom_event_t *);
  int mulib_dom_add_event_listener (mulib_dom_node_t *, const char *type,
                                    mulib_dom_listener_t, void *,
                                    const mulib_dom_listener_options_t *,
                                    mulib_dom_error_t *);
  void mulib_dom_remove_event_listener (mulib_dom_node_t *, const char *type,
                                        mulib_dom_listener_t, void *,
                                        int capture);
  /** Capture/target/bubble over a snapshotted ancestor path. 1 not canceled,
   * 0 canceled, -1 error. Listener changes and nested dispatch are supported.
   */
  int mulib_dom_dispatch_event (mulib_dom_node_t *, mulib_dom_event_t *,
                                mulib_dom_error_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_DOM) && !defined(MULIB_DOM_IMPLEMENTED)
#define MULIB_DOM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>

typedef struct mulib_dom_listener_
{
  struct mulib_dom_listener_ *next;
  char *type;
  mulib_dom_listener_t callback;
  void *userdata;
  int capture, once, passive, removed;
} mulib_dom_listener_;
struct mulib_dom_node
{
  mulib_dom_type_t type;
  mulib_dom_document_t *document;
  mulib_dom_node_t *allocated_next, *allocated_previous;
  mulib_dom_node_t *parent, *first, *last, *next, *previous;
  mulib_dom_node_t *attributes, *attributes_last, *owner_element;
  char *name, *namespace_uri, *prefix, *local_name, *public_id, *system_id;
  uint16_t *data;
  size_t length;
  mulib_dom_listener_ *listeners, *listeners_last;
  size_t dispatches;
};
struct mulib_dom_document
{
  mulib_dom_node_t *node, *allocated;
  size_t count, dispatches;
};
struct mulib_dom_event
{
  char *type;
  void *detail;
  mulib_dom_node_t *target, *current;
  mulib_dom_node_t **path;
  size_t path_length;
  unsigned phase;
  int bubbles, cancelable, canceled, stopped, immediate, passive, dispatching;
};
static int
mulib_dom_error_ (mulib_dom_error_t *e, mulib_dom_error_t code)
{
  if (e)
    *e = code;
  return code == MULIB_DOM_OK;
}
static char *
mulib_dom_copy_ (const char *s)
{
  size_t n;
  char *p;
  if (!s)
    return NULL;
  n = strlen (s);
  if (n > MULIB_DOM_STRING_LIMIT)
    return NULL;
  p = (char *)malloc (n + 1);
  if (p)
    memcpy (p, s, n + 1);
  return p;
}
static int
mulib_dom_string_valid_ (mulib_dom_string_t s)
{
  return (!s.length || s.data) && s.length <= MULIB_DOM_STRING_LIMIT;
}
static int
mulib_dom_data_ (mulib_dom_node_t *n, mulib_dom_string_t s,
                 mulib_dom_error_t *e)
{
  uint16_t *p = NULL;
  if (!mulib_dom_string_valid_ (s))
    return mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
  if (s.length)
    {
      p = (uint16_t *)malloc (s.length * sizeof *p);
      if (!p)
        return mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
      memcpy (p, s.data, s.length * sizeof *p);
    }
  free (n->data);
  n->data = p;
  n->length = s.length;
  return mulib_dom_error_ (e, MULIB_DOM_OK);
}
static int
mulib_dom_character_ (const mulib_dom_node_t *n)
{
  return n
         && (n->type == MULIB_DOM_TEXT || n->type == MULIB_DOM_CDATA
             || n->type == MULIB_DOM_COMMENT
             || n->type == MULIB_DOM_PROCESSING_INSTRUCTION);
}
static void
mulib_dom_unlink_allocation_ (mulib_dom_node_t *n)
{
  if (n->allocated_previous)
    n->allocated_previous->allocated_next = n->allocated_next;
  else
    n->document->allocated = n->allocated_next;
  if (n->allocated_next)
    n->allocated_next->allocated_previous = n->allocated_previous;
  --n->document->count;
}
static void
mulib_dom_link_allocation_ (mulib_dom_document_t *d, mulib_dom_node_t *n)
{
  n->document = d;
  n->allocated_previous = NULL;
  n->allocated_next = d->allocated;
  if (d->allocated)
    d->allocated->allocated_previous = n;
  d->allocated = n;
  ++d->count;
}
static void
mulib_dom_free_node_ (mulib_dom_node_t *n)
{
  mulib_dom_listener_ *l = n->listeners;
  while (l)
    {
      mulib_dom_listener_ *next = l->next;
      free (l->type);
      free (l);
      l = next;
    }
  mulib_dom_unlink_allocation_ (n);
  free (n->name);
  free (n->namespace_uri);
  free (n->prefix);
  free (n->local_name);
  free (n->public_id);
  free (n->system_id);
  free (n->data);
  free (n);
}
static mulib_dom_node_t *
mulib_dom_new_ (mulib_dom_document_t *d, mulib_dom_type_t type,
                mulib_dom_error_t *e)
{
  mulib_dom_node_t *n;
  mulib_dom_error_ (e, MULIB_DOM_OK);
  if (!d)
    {
      mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
      return NULL;
    }
  if (d->count >= MULIB_DOM_NODE_LIMIT)
    {
      mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
      return NULL;
    }
  n = (mulib_dom_node_t *)calloc (1, sizeof *n);
  if (!n)
    {
      mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
      return NULL;
    }
  n->type = type;
  mulib_dom_link_allocation_ (d, n);
  return n;
}
mulib_dom_document_t *
mulib_dom_document_new (void)
{
  mulib_dom_document_t *d = (mulib_dom_document_t *)calloc (1, sizeof *d);
  if (d && !(d->node = mulib_dom_new_ (d, MULIB_DOM_DOCUMENT, NULL)))
    {
      free (d);
      d = NULL;
    }
  return d;
}
int
mulib_dom_document_destroy (mulib_dom_document_t *d)
{
  if (!d)
    return 1;
  if (d->dispatches)
    return 0;
  while (d->allocated)
    mulib_dom_free_node_ (d->allocated);
  free (d);
  return 1;
}
mulib_dom_node_t *
mulib_dom_document_node (const mulib_dom_document_t *d)
{
  return d ? d->node : NULL;
}
mulib_dom_document_t *
mulib_dom_owner_document (const mulib_dom_node_t *n)
{
  return n && n->type != MULIB_DOM_DOCUMENT ? n->document : NULL;
}
mulib_dom_node_t *
mulib_dom_document_element (const mulib_dom_document_t *d)
{
  mulib_dom_node_t *n;
  for (n = d ? d->node->first : NULL; n; n = n->next)
    if (n->type == MULIB_DOM_ELEMENT)
      return n;
  return NULL;
}
static int
mulib_dom_name_start_ (char c)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
static int
mulib_dom_name_ (const char *s)
{
  int start = 1, colon = 0;
  if (!s || !*s)
    return 0;
  for (; *s; ++s)
    {
      if (*s == ':' && !start && !colon)
        {
          colon = 1;
          start = 1;
          continue;
        }
      if (!mulib_dom_name_start_ (*s)
          && (start || !((*s >= '0' && *s <= '9') || *s == '-' || *s == '.')))
        return 0;
      start = 0;
    }
  return !start;
}
static int
mulib_dom_equal_c_ (const char *a, const char *b)
{
  return !strcmp (a ? a : "", b ? b : "");
}
static mulib_dom_node_t *
mulib_dom_named_ (mulib_dom_document_t *d, mulib_dom_type_t type,
                  const char *ns, const char *name, mulib_dom_error_t *e)
{
  mulib_dom_node_t *n;
  const char *colon;
  if (!mulib_dom_name_ (name))
    {
      mulib_dom_error_ (e, MULIB_DOM_INVALID_CHARACTER_ERR);
      return NULL;
    }
  if (ns && !*ns)
    ns = NULL;
  colon = strchr (name, ':');
  if ((colon && !ns)
      || (colon && colon - name == 3 && !memcmp (name, "xml", 3)
          && !mulib_dom_equal_c_ (ns, MULIB_DOM_XML_NAMESPACE))
      || ((!strcmp (name, "xmlns")
           || (colon && colon - name == 5 && !memcmp (name, "xmlns", 5)))
          != mulib_dom_equal_c_ (ns, MULIB_DOM_XMLNS_NAMESPACE)))
    {
      mulib_dom_error_ (e, MULIB_DOM_NAMESPACE_ERR);
      return NULL;
    }
  n = mulib_dom_new_ (d, type, e);
  if (!n)
    return NULL;
  n->name = mulib_dom_copy_ (name);
  n->local_name = mulib_dom_copy_ (colon ? colon + 1 : name);
  if (ns)
    n->namespace_uri = mulib_dom_copy_ (ns);
  if (colon && (n->prefix = mulib_dom_copy_ (name)))
    n->prefix[colon - name] = 0;
  if (!n->name || !n->local_name || (ns && !n->namespace_uri)
      || (colon && !n->prefix))
    {
      mulib_dom_free_node_ (n);
      mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
      return NULL;
    }
  return n;
}
mulib_dom_node_t *
mulib_dom_create_element (mulib_dom_document_t *d, const char *ns,
                          const char *name, mulib_dom_error_t *e)
{
  return mulib_dom_named_ (d, MULIB_DOM_ELEMENT, ns, name, e);
}
mulib_dom_node_t *
mulib_dom_create_attribute (mulib_dom_document_t *d, const char *ns,
                            const char *name, mulib_dom_error_t *e)
{
  return mulib_dom_named_ (d, MULIB_DOM_ATTRIBUTE, ns, name, e);
}
mulib_dom_node_t *
mulib_dom_create_fragment (mulib_dom_document_t *d, mulib_dom_error_t *e)
{
  return mulib_dom_new_ (d, MULIB_DOM_DOCUMENT_FRAGMENT, e);
}
mulib_dom_node_t *
mulib_dom_create_character_data (mulib_dom_document_t *d,
                                 mulib_dom_type_t type, const char *target,
                                 mulib_dom_string_t s, mulib_dom_error_t *e)
{
  mulib_dom_node_t *n;
  size_t i;
  if (type != MULIB_DOM_TEXT && type != MULIB_DOM_CDATA
      && type != MULIB_DOM_COMMENT && type != MULIB_DOM_PROCESSING_INSTRUCTION)
    {
      mulib_dom_error_ (e, MULIB_DOM_NOT_SUPPORTED_ERR);
      return NULL;
    }
  if (type == MULIB_DOM_PROCESSING_INSTRUCTION && !mulib_dom_name_ (target))
    {
      mulib_dom_error_ (e, MULIB_DOM_INVALID_CHARACTER_ERR);
      return NULL;
    }
  if (!mulib_dom_string_valid_ (s))
    {
      mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
      return NULL;
    }
  for (i = 1; i < s.length; ++i)
    if ((type == MULIB_DOM_CDATA && i >= 2 && s.data[i - 2] == ']'
         && s.data[i - 1] == ']' && s.data[i] == '>')
        || (type == MULIB_DOM_PROCESSING_INSTRUCTION && s.data[i - 1] == '?'
            && s.data[i] == '>'))
      {
        mulib_dom_error_ (e, MULIB_DOM_INVALID_CHARACTER_ERR);
        return NULL;
      }
  n = mulib_dom_new_ (d, type, e);
  if (!n)
    return NULL;
  if (type == MULIB_DOM_PROCESSING_INSTRUCTION
      && !(n->name = mulib_dom_copy_ (target)))
    {
      mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
      mulib_dom_free_node_ (n);
      return NULL;
    }
  if (!mulib_dom_data_ (n, s, e))
    {
      mulib_dom_free_node_ (n);
      return NULL;
    }
  return n;
}
mulib_dom_node_t *
mulib_dom_create_doctype (mulib_dom_document_t *d, const char *name,
                          const char *pub, const char *sys,
                          mulib_dom_error_t *e)
{
  mulib_dom_node_t *n;
  if (!mulib_dom_name_ (name))
    {
      mulib_dom_error_ (e, MULIB_DOM_INVALID_CHARACTER_ERR);
      return NULL;
    }
  n = mulib_dom_new_ (d, MULIB_DOM_DOCUMENT_TYPE, e);
  if (!n)
    return NULL;
  n->name = mulib_dom_copy_ (name);
  n->public_id = mulib_dom_copy_ (pub ? pub : "");
  n->system_id = mulib_dom_copy_ (sys ? sys : "");
  if (!n->name || !n->public_id || !n->system_id)
    {
      mulib_dom_free_node_ (n);
      mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
      return NULL;
    }
  return n;
}
mulib_dom_type_t
mulib_dom_node_type (const mulib_dom_node_t *n)
{
  return n ? n->type : (mulib_dom_type_t)0;
}
const char *
mulib_dom_node_name (const mulib_dom_node_t *n)
{
  if (!n)
    return NULL;
  switch (n->type)
    {
    case MULIB_DOM_DOCUMENT:
      return "#document";
    case MULIB_DOM_DOCUMENT_FRAGMENT:
      return "#document-fragment";
    case MULIB_DOM_TEXT:
      return "#text";
    case MULIB_DOM_CDATA:
      return "#cdata-section";
    case MULIB_DOM_COMMENT:
      return "#comment";
    default:
      return n->name;
    }
}
const char *
mulib_dom_namespace_uri (const mulib_dom_node_t *n)
{
  return n ? n->namespace_uri : NULL;
}
const char *
mulib_dom_prefix (const mulib_dom_node_t *n)
{
  return n ? n->prefix : NULL;
}
const char *
mulib_dom_local_name (const mulib_dom_node_t *n)
{
  return n ? n->local_name : NULL;
}
const char *
mulib_dom_public_id (const mulib_dom_node_t *n)
{
  return n ? n->public_id : NULL;
}
const char *
mulib_dom_system_id (const mulib_dom_node_t *n)
{
  return n ? n->system_id : NULL;
}
mulib_dom_node_t *
mulib_dom_parent (const mulib_dom_node_t *n)
{
  return n ? n->parent : NULL;
}
mulib_dom_node_t *
mulib_dom_first_child (const mulib_dom_node_t *n)
{
  return n ? n->first : NULL;
}
mulib_dom_node_t *
mulib_dom_last_child (const mulib_dom_node_t *n)
{
  return n ? n->last : NULL;
}
mulib_dom_node_t *
mulib_dom_next_sibling (const mulib_dom_node_t *n)
{
  return n && !n->owner_element ? n->next : NULL;
}
mulib_dom_node_t *
mulib_dom_previous_sibling (const mulib_dom_node_t *n)
{
  return n && !n->owner_element ? n->previous : NULL;
}
mulib_dom_node_t *
mulib_dom_get_root_node (const mulib_dom_node_t *n)
{
  if (n)
    while (n->parent)
      n = n->parent;
  return (mulib_dom_node_t *)n;
}
int
mulib_dom_is_connected (const mulib_dom_node_t *n)
{
  n = mulib_dom_get_root_node (n);
  return n && n->type == MULIB_DOM_DOCUMENT;
}
int
mulib_dom_contains (const mulib_dom_node_t *n, const mulib_dom_node_t *other)
{
  if (n)
    for (; other; other = other->parent)
      if (other == n)
        return 1;
  return 0;
}
size_t
mulib_dom_child_count (const mulib_dom_node_t *n)
{
  size_t count = 0;
  for (n = n ? n->first : NULL; n; n = n->next)
    ++count;
  return count;
}
mulib_dom_node_t *
mulib_dom_child_at (const mulib_dom_node_t *n, size_t i)
{
  for (n = n ? n->first : NULL; n && i; n = n->next)
    --i;
  return (mulib_dom_node_t *)n;
}

/* Iterative preorder bounded by a root, excluding attribute lists. */
static mulib_dom_node_t *
mulib_dom_next_ (const mulib_dom_node_t *n, const mulib_dom_node_t *root)
{
  if (n->first)
    return n->first;
  while (n != root)
    {
      if (n->next)
        return n->next;
      n = n->parent;
    }
  return NULL;
}
static void
mulib_dom_detach_ (mulib_dom_node_t *n)
{
  mulib_dom_node_t *p = n->parent ? n->parent : n->owner_element;
  if (!p)
    return;
  if (n->previous)
    n->previous->next = n->next;
  else if (n->owner_element)
    p->attributes = n->next;
  else
    p->first = n->next;
  if (n->next)
    n->next->previous = n->previous;
  else if (n->owner_element)
    p->attributes_last = n->previous;
  else
    p->last = n->previous;
  n->next = n->previous = n->parent = n->owner_element = NULL;
}
static void
mulib_dom_attach_ (mulib_dom_node_t *p, mulib_dom_node_t *n,
                   mulib_dom_node_t *ref)
{
  n->parent = p;
  n->next = ref;
  n->previous = ref ? ref->previous : p->last;
  if (n->previous)
    n->previous->next = n;
  else
    p->first = n;
  if (ref)
    ref->previous = n;
  else
    p->last = n;
}
static size_t
mulib_dom_subtree_size_ (const mulib_dom_node_t *root)
{
  size_t count = 0;
  const mulib_dom_node_t *n, *a;
  for (n = root; n; n = mulib_dom_next_ (n, root))
    {
      ++count;
      for (a = n->attributes; a; a = a->next)
        ++count;
    }
  return count;
}
static int
mulib_dom_can_adopt_ (mulib_dom_document_t *d, mulib_dom_node_t *n,
                      mulib_dom_error_t *e)
{
  if (!d || !n)
    return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
  if (n->type == MULIB_DOM_DOCUMENT)
    return mulib_dom_error_ (e, MULIB_DOM_NOT_SUPPORTED_ERR);
  if (n->document != d)
    {
      if (n->document->dispatches || d->dispatches)
        return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
      if (mulib_dom_subtree_size_ (n) > MULIB_DOM_NODE_LIMIT - d->count)
        return mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
    }
  return 1;
}
static void
mulib_dom_transfer_ (mulib_dom_document_t *d, mulib_dom_node_t *root)
{
  mulib_dom_node_t *n, *a;
  if (root->document == d)
    return;
  for (n = root; n; n = mulib_dom_next_ (n, root))
    {
      mulib_dom_unlink_allocation_ (n);
      mulib_dom_link_allocation_ (d, n);
      for (a = n->attributes; a; a = a->next)
        {
          mulib_dom_unlink_allocation_ (a);
          mulib_dom_link_allocation_ (d, a);
        }
    }
}
mulib_dom_node_t *
mulib_dom_adopt_node (mulib_dom_document_t *d, mulib_dom_node_t *n,
                      mulib_dom_error_t *e)
{
  mulib_dom_error_ (e, MULIB_DOM_OK);
  if (!mulib_dom_can_adopt_ (d, n, e))
    return NULL;
  mulib_dom_detach_ (n);
  mulib_dom_transfer_ (d, n);
  return n;
}
static int
mulib_dom_document_child_ (const mulib_dom_node_t *n, int *element,
                           int *doctype)
{
  switch (n->type)
    {
    case MULIB_DOM_ELEMENT:
      return ++*element == 1;
    case MULIB_DOM_DOCUMENT_TYPE:
      return !*element && ++*doctype == 1;
    case MULIB_DOM_COMMENT:
    case MULIB_DOM_PROCESSING_INSTRUCTION:
      return 1;
    default:
      return 0;
    }
}
static int
mulib_dom_preinsert_ (mulib_dom_node_t *p, mulib_dom_node_t *n,
                      mulib_dom_node_t *ref, mulib_dom_node_t *old,
                      mulib_dom_error_t *e)
{
  const mulib_dom_node_t *v, *c;
  size_t depth = 0;
  int element = 0, doctype = 0;
  if (!p || !n)
    return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
  if (p->type != MULIB_DOM_ELEMENT && p->type != MULIB_DOM_DOCUMENT
      && p->type != MULIB_DOM_DOCUMENT_FRAGMENT)
    return mulib_dom_error_ (e, MULIB_DOM_HIERARCHY_REQUEST_ERR);
  if ((ref && ref->parent != p) || (old && old->parent != p))
    return mulib_dom_error_ (e, MULIB_DOM_NOT_FOUND_ERR);
  if (mulib_dom_contains (n, p) || n->type == MULIB_DOM_ATTRIBUTE
      || n->type == MULIB_DOM_DOCUMENT
      || (n->type == MULIB_DOM_DOCUMENT_TYPE && p->type != MULIB_DOM_DOCUMENT))
    return mulib_dom_error_ (e, MULIB_DOM_HIERARCHY_REQUEST_ERR);
  if (!mulib_dom_can_adopt_ (p->document, n, e))
    return 0;
  for (v = p; v; v = v->parent)
    ++depth;
  for (v = n; v; v = mulib_dom_next_ (v, n))
    {
      size_t level = depth;
      for (c = v; c != n; c = c->parent)
        ++level;
      if (n->type != MULIB_DOM_DOCUMENT_FRAGMENT)
        ++level;
      if (level > MULIB_DOM_DEPTH_LIMIT)
        return mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
    }
  if (p->type != MULIB_DOM_DOCUMENT)
    return 1;
  /* Validate the complete prospective child sequence before any detach. */
  for (v = p->first;; v = v->next)
    {
      if (v == ref)
        {
          c = n->type == MULIB_DOM_DOCUMENT_FRAGMENT ? n->first : n;
          for (; c;
               c = n->type == MULIB_DOM_DOCUMENT_FRAGMENT ? c->next : NULL)
            if (!mulib_dom_document_child_ (c, &element, &doctype))
              return mulib_dom_error_ (e, MULIB_DOM_HIERARCHY_REQUEST_ERR);
        }
      if (!v)
        break;
      if (v != old && v != n
          && !mulib_dom_document_child_ (v, &element, &doctype))
        return mulib_dom_error_ (e, MULIB_DOM_HIERARCHY_REQUEST_ERR);
    }
  return 1;
}
static mulib_dom_node_t *
mulib_dom_insert_ (mulib_dom_node_t *p, mulib_dom_node_t *n,
                   mulib_dom_node_t *ref, mulib_dom_node_t *old,
                   mulib_dom_error_t *e)
{
  mulib_dom_error_ (e, MULIB_DOM_OK);
  if (ref && (!p || ref->parent != p))
    {
      mulib_dom_error_ (e, MULIB_DOM_NOT_FOUND_ERR);
      return NULL;
    }
  if (n && ref == n)
    ref = n->next;
  if (!mulib_dom_preinsert_ (p, n, ref, old, e))
    return NULL;
  if (old == n)
    return old;
  if (old)
    mulib_dom_detach_ (old);
  mulib_dom_detach_ (n);
  mulib_dom_transfer_ (p->document, n);
  if (n->type == MULIB_DOM_DOCUMENT_FRAGMENT)
    {
      while (n->first)
        {
          mulib_dom_node_t *c = n->first;
          mulib_dom_detach_ (c);
          mulib_dom_attach_ (p, c, ref);
        }
    }
  else
    mulib_dom_attach_ (p, n, ref);
  return old ? old : n;
}
mulib_dom_node_t *
mulib_dom_insert_before (mulib_dom_node_t *p, mulib_dom_node_t *n,
                         mulib_dom_node_t *ref, mulib_dom_error_t *e)
{
  return mulib_dom_insert_ (p, n, ref, NULL, e);
}
mulib_dom_node_t *
mulib_dom_append_child (mulib_dom_node_t *p, mulib_dom_node_t *n,
                        mulib_dom_error_t *e)
{
  return mulib_dom_insert_ (p, n, NULL, NULL, e);
}
mulib_dom_node_t *
mulib_dom_replace_child (mulib_dom_node_t *p, mulib_dom_node_t *n,
                         mulib_dom_node_t *old, mulib_dom_error_t *e)
{
  if (!old || !p || old->parent != p)
    {
      mulib_dom_error_ (e, MULIB_DOM_NOT_FOUND_ERR);
      return NULL;
    }
  return mulib_dom_insert_ (p, n, old->next, old, e);
}
mulib_dom_node_t *
mulib_dom_remove_child (mulib_dom_node_t *p, mulib_dom_node_t *n,
                        mulib_dom_error_t *e)
{
  if (!p || !n || n->parent != p)
    {
      mulib_dom_error_ (e, MULIB_DOM_NOT_FOUND_ERR);
      return NULL;
    }
  mulib_dom_detach_ (n);
  mulib_dom_error_ (e, MULIB_DOM_OK);
  return n;
}

size_t
mulib_dom_attribute_count (const mulib_dom_node_t *n)
{
  size_t count = 0;
  for (n = n ? n->attributes : NULL; n; n = n->next)
    ++count;
  return count;
}
mulib_dom_node_t *
mulib_dom_attribute_at (const mulib_dom_node_t *n, size_t i)
{
  for (n = n ? n->attributes : NULL; n && i; n = n->next)
    --i;
  return (mulib_dom_node_t *)n;
}
mulib_dom_node_t *
mulib_dom_owner_element (const mulib_dom_node_t *n)
{
  return n ? n->owner_element : NULL;
}
mulib_dom_node_t *
mulib_dom_get_attribute_node (const mulib_dom_node_t *n, const char *ns,
                              const char *local)
{
  if (!local)
    return NULL;
  for (n = n ? n->attributes : NULL; n; n = n->next)
    if (mulib_dom_equal_c_ (n->namespace_uri, ns)
        && !strcmp (n->local_name, local))
      return (mulib_dom_node_t *)n;
  return NULL;
}
int
mulib_dom_set_attribute_node (mulib_dom_node_t *n, mulib_dom_node_t *a,
                              mulib_dom_error_t *e)
{
  mulib_dom_node_t *old, *next;
  if (!n || n->type != MULIB_DOM_ELEMENT || !a
      || a->type != MULIB_DOM_ATTRIBUTE)
    return mulib_dom_error_ (e, MULIB_DOM_NOT_SUPPORTED_ERR);
  if (a->document != n->document)
    return mulib_dom_error_ (e, MULIB_DOM_WRONG_DOCUMENT_ERR);
  if (a->owner_element && a->owner_element != n)
    return mulib_dom_error_ (e, MULIB_DOM_INUSE_ATTRIBUTE_ERR);
  if (a->owner_element == n)
    return mulib_dom_error_ (e, MULIB_DOM_OK);
  old = mulib_dom_get_attribute_node (n, a->namespace_uri, a->local_name);
  next = old ? old->next : NULL;
  if (old)
    mulib_dom_detach_ (old);
  a->owner_element = n;
  a->next = next;
  a->previous = next ? next->previous : n->attributes_last;
  if (a->previous)
    a->previous->next = a;
  else
    n->attributes = a;
  if (next)
    next->previous = a;
  else
    n->attributes_last = a;
  return mulib_dom_error_ (e, MULIB_DOM_OK);
}
int
mulib_dom_set_attribute (mulib_dom_node_t *n, const char *ns, const char *name,
                         mulib_dom_string_t s, mulib_dom_error_t *e)
{
  mulib_dom_node_t *a, *old;
  if (!n || n->type != MULIB_DOM_ELEMENT)
    return mulib_dom_error_ (e, MULIB_DOM_NOT_SUPPORTED_ERR);
  a = mulib_dom_create_attribute (n->document, ns, name, e);
  if (!a)
    return 0;
  if (!mulib_dom_data_ (a, s, e))
    {
      mulib_dom_free_node_ (a);
      return 0;
    }
  old = mulib_dom_get_attribute_node (n, ns, a->local_name);
  if (old)
    {
      /* setAttributeNS preserves an existing Attr's identity and list
       * position. */
      free (old->name);
      free (old->prefix);
      free (old->data);
      old->name = a->name;
      old->prefix = a->prefix;
      old->data = a->data;
      old->length = a->length;
      a->name = a->prefix = NULL;
      a->data = NULL;
      mulib_dom_free_node_ (a);
      return mulib_dom_error_ (e, MULIB_DOM_OK);
    }
  return mulib_dom_set_attribute_node (n, a, e);
}
int
mulib_dom_remove_attribute (mulib_dom_node_t *n, const char *ns,
                            const char *local, mulib_dom_error_t *e)
{
  mulib_dom_node_t *a;
  if (!n || n->type != MULIB_DOM_ELEMENT || !local)
    return mulib_dom_error_ (e, MULIB_DOM_NOT_SUPPORTED_ERR);
  a = mulib_dom_get_attribute_node (n, ns, local);
  if (a)
    mulib_dom_detach_ (a);
  return mulib_dom_error_ (e, MULIB_DOM_OK);
}
mulib_dom_string_t
mulib_dom_node_value (const mulib_dom_node_t *n)
{
  mulib_dom_string_t s = { NULL, 0 };
  if (n && (mulib_dom_character_ (n) || n->type == MULIB_DOM_ATTRIBUTE))
    {
      s.data = n->data;
      s.length = n->length;
    }
  return s;
}
int
mulib_dom_set_node_value (mulib_dom_node_t *n, mulib_dom_string_t s,
                          mulib_dom_error_t *e)
{
  if (!n)
    return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
  return (mulib_dom_character_ (n) || n->type == MULIB_DOM_ATTRIBUTE)
             ? mulib_dom_data_ (n, s, e)
             : mulib_dom_error_ (e, MULIB_DOM_OK);
}
static int
mulib_dom_append_data_ (mulib_buffer_t *b, const uint16_t *data, size_t length)
{
  return length <= MULIB_DOM_STRING_LIMIT
         && b->length <= MULIB_DOM_STRING_LIMIT * sizeof *data
         && length * sizeof *data
                <= MULIB_DOM_STRING_LIMIT * sizeof *data - b->length
         && mulib_buffer_append (b, data, length * sizeof *data);
}
int
mulib_dom_text_content (const mulib_dom_node_t *n, mulib_buffer_t *out,
                        mulib_dom_error_t *e)
{
  mulib_buffer_t b = { 0 };
  const mulib_dom_node_t *c;
  int ok = 1;
  if (!n || !out)
    return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
  if (mulib_dom_character_ (n) || n->type == MULIB_DOM_ATTRIBUTE)
    ok = mulib_dom_append_data_ (&b, n->data, n->length);
  else if (n->type == MULIB_DOM_ELEMENT
           || n->type == MULIB_DOM_DOCUMENT_FRAGMENT)
    for (c = n->first; c && ok; c = mulib_dom_next_ (c, n))
      if (c->type == MULIB_DOM_TEXT || c->type == MULIB_DOM_CDATA)
        ok = mulib_dom_append_data_ (&b, c->data, c->length);
  if (ok)
    ok = mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return mulib_dom_error_ (e, ok ? MULIB_DOM_OK : MULIB_DOM_OUT_OF_MEMORY);
}
int
mulib_dom_set_text_content (mulib_dom_node_t *n, mulib_dom_string_t s,
                            mulib_dom_error_t *e)
{
  mulib_dom_node_t *text = NULL;
  if (!n)
    return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
  if (mulib_dom_character_ (n) || n->type == MULIB_DOM_ATTRIBUTE)
    return mulib_dom_data_ (n, s, e);
  if (n->type != MULIB_DOM_ELEMENT && n->type != MULIB_DOM_DOCUMENT_FRAGMENT)
    return mulib_dom_error_ (e, MULIB_DOM_OK);
  if (!mulib_dom_string_valid_ (s))
    return mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
  if (s.length)
    {
      size_t depth = 0;
      const mulib_dom_node_t *p;
      for (p = n; p; p = p->parent)
        ++depth;
      if (depth >= MULIB_DOM_DEPTH_LIMIT)
        return mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
      text = mulib_dom_create_character_data (n->document, MULIB_DOM_TEXT,
                                              NULL, s, e);
      if (!text)
        return 0;
    }
  while (n->first)
    mulib_dom_detach_ (n->first);
  if (text)
    mulib_dom_attach_ (n, text, NULL);
  return mulib_dom_error_ (e, MULIB_DOM_OK);
}
int
mulib_dom_substring_data (const mulib_dom_node_t *n, size_t offset,
                          size_t count, mulib_buffer_t *out,
                          mulib_dom_error_t *e)
{
  if (!mulib_dom_character_ (n) || !out)
    return mulib_dom_error_ (e, MULIB_DOM_NOT_SUPPORTED_ERR);
  if (offset > n->length)
    return mulib_dom_error_ (e, MULIB_DOM_INDEX_SIZE_ERR);
  if (count > n->length - offset)
    count = n->length - offset;
  return mulib_dom_error_ (
      e, mulib_buffer_append (out, count ? n->data + offset : NULL,
                              count * sizeof *n->data)
             ? MULIB_DOM_OK
             : MULIB_DOM_OUT_OF_MEMORY);
}
int
mulib_dom_replace_data (mulib_dom_node_t *n, size_t offset, size_t count,
                        mulib_dom_string_t s, mulib_dom_error_t *e)
{
  uint16_t *data = NULL;
  size_t length, tail;
  if (!mulib_dom_character_ (n))
    return mulib_dom_error_ (e, MULIB_DOM_NOT_SUPPORTED_ERR);
  if (offset > n->length)
    return mulib_dom_error_ (e, MULIB_DOM_INDEX_SIZE_ERR);
  if (count > n->length - offset)
    count = n->length - offset;
  if (!mulib_dom_string_valid_ (s)
      || s.length > MULIB_DOM_STRING_LIMIT - (n->length - count))
    return mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
  length = n->length - count + s.length;
  tail = n->length - offset - count;
  if (length)
    {
      data = (uint16_t *)malloc (length * sizeof *data);
      if (!data)
        return mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
      if (offset)
        memcpy (data, n->data, offset * sizeof *data);
      if (s.length)
        memcpy (data + offset, s.data, s.length * sizeof *data);
      if (tail)
        memcpy (data + offset + s.length, n->data + offset + count,
                tail * sizeof *data);
    }
  free (n->data);
  n->data = data;
  n->length = length;
  return mulib_dom_error_ (e, MULIB_DOM_OK);
}
mulib_dom_node_t *
mulib_dom_split_text (mulib_dom_node_t *n, size_t offset, mulib_dom_error_t *e)
{
  mulib_dom_node_t *tail;
  mulib_dom_string_t s;
  if (!n || (n->type != MULIB_DOM_TEXT && n->type != MULIB_DOM_CDATA))
    {
      mulib_dom_error_ (e, MULIB_DOM_NOT_SUPPORTED_ERR);
      return NULL;
    }
  if (offset > n->length)
    {
      mulib_dom_error_ (e, MULIB_DOM_INDEX_SIZE_ERR);
      return NULL;
    }
  s.length = n->length - offset;
  s.data = s.length ? n->data + offset : NULL;
  tail = mulib_dom_create_character_data (n->document, MULIB_DOM_TEXT, NULL, s,
                                          e);
  if (!tail)
    return NULL;
  if (n->parent)
    mulib_dom_attach_ (n->parent, tail, n->next);
  n->length = offset;
  return tail;
}
typedef struct mulib_dom_normalize_
{
  struct mulib_dom_normalize_ *next;
  mulib_dom_node_t *start, *end, *keep;
  mulib_buffer_t data;
} mulib_dom_normalize_;
int
mulib_dom_normalize (mulib_dom_node_t *root, mulib_dom_error_t *e)
{
  mulib_dom_normalize_ *plans = NULL, *plan;
  mulib_dom_node_t *n, *end, *keep, *v;
  int ok = 1;
  if (!root)
    return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
  for (n = root->first; n && ok;)
    {
      if (n->type != MULIB_DOM_TEXT)
        {
          n = mulib_dom_next_ (n, root);
          continue;
        }
      keep = NULL;
      for (end = n; end && end->type == MULIB_DOM_TEXT; end = end->next)
        if (!keep && end->length)
          keep = end;
      if (n->next == end && n->length)
        {
          n = mulib_dom_next_ (n, root);
          continue;
        }
      plan = (mulib_dom_normalize_ *)calloc (1, sizeof *plan);
      if (!plan)
        {
          ok = 0;
          break;
        }
      plan->next = plans;
      plans = plan;
      plan->start = n;
      plan->end = end;
      plan->keep = keep;
      for (v = n; v != end; v = v->next)
        if (!mulib_dom_append_data_ (&plan->data, v->data, v->length))
          {
            ok = 0;
            break;
          }
      while (n->next != end)
        n = n->next;
      n = mulib_dom_next_ (n, root);
    }
  while (plans)
    {
      plan = plans;
      plans = plan->next;
      if (ok)
        {
          for (n = plan->start; n != plan->end;)
            {
              v = n->next;
              if (n != plan->keep)
                mulib_dom_detach_ (n);
              n = v;
            }
          if (plan->keep)
            {
              free (plan->keep->data);
              plan->keep->data = (uint16_t *)plan->data.data;
              plan->keep->length = plan->data.length / sizeof (uint16_t);
              plan->data.data = NULL;
            }
        }
      mulib_buffer_free (&plan->data);
      free (plan);
    }
  return mulib_dom_error_ (e, ok ? MULIB_DOM_OK : MULIB_DOM_OUT_OF_MEMORY);
}
static mulib_dom_node_t *
mulib_dom_clone_ (mulib_dom_document_t *d, const mulib_dom_node_t *source,
                  int deep, mulib_dom_error_t *e)
{
  mulib_dom_node_t *n, *copy;
  const mulib_dom_node_t *c;
  mulib_dom_string_t s = mulib_dom_node_value (source);
  if (source->type == MULIB_DOM_ELEMENT || source->type == MULIB_DOM_ATTRIBUTE)
    n = mulib_dom_named_ (d, source->type, source->namespace_uri, source->name,
                          e);
  else if (source->type == MULIB_DOM_DOCUMENT_TYPE)
    n = mulib_dom_create_doctype (d, source->name, source->public_id,
                                  source->system_id, e);
  else if (source->type == MULIB_DOM_DOCUMENT_FRAGMENT)
    n = mulib_dom_create_fragment (d, e);
  else
    {
      /* Cloning preserves data even when later CharacterData edits introduced
       * markup delimiters that the document creation methods would reject. */
      n = mulib_dom_new_ (d, source->type, e);
      if (!n)
        return NULL;
      if (source->name && !(n->name = mulib_dom_copy_ (source->name)))
        {
          mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
          return NULL;
        }
      return mulib_dom_data_ (n, s, e) ? n : NULL;
    }
  if (!n
      || (source->type == MULIB_DOM_ATTRIBUTE && !mulib_dom_data_ (n, s, e)))
    return NULL;
  for (c = source->attributes; c; c = c->next)
    {
      copy = mulib_dom_clone_ (d, c, 0, e);
      if (!copy || !mulib_dom_set_attribute_node (n, copy, e))
        return NULL;
    }
  if (deep)
    for (c = source->first; c; c = c->next)
      {
        copy = mulib_dom_clone_ (d, c, 1, e);
        if (!copy)
          return NULL;
        mulib_dom_attach_ (n, copy, NULL);
      }
  return n;
}
mulib_dom_node_t *
mulib_dom_import_node (mulib_dom_document_t *d, const mulib_dom_node_t *n,
                       int deep, mulib_dom_error_t *e)
{
  mulib_dom_node_t *mark, *result;
  mulib_dom_error_ (e, MULIB_DOM_OK);
  if (!d || !n || n->type == MULIB_DOM_DOCUMENT)
    {
      mulib_dom_error_ (e, MULIB_DOM_NOT_SUPPORTED_ERR);
      return NULL;
    }
  mark = d->allocated;
  result = mulib_dom_clone_ (d, n, deep, e);
  if (!result)
    while (d->allocated != mark)
      mulib_dom_free_node_ (d->allocated);
  return result;
}
mulib_dom_node_t *
mulib_dom_clone_node (const mulib_dom_node_t *n, int deep,
                      mulib_dom_error_t *e)
{
  return mulib_dom_import_node (n ? n->document : NULL, n, deep, e);
}
static int
mulib_dom_string_equal_ (mulib_dom_string_t a, mulib_dom_string_t b)
{
  return a.length == b.length
         && (!a.length || !memcmp (a.data, b.data, a.length * sizeof *a.data));
}
int
mulib_dom_is_equal_node (const mulib_dom_node_t *a, const mulib_dom_node_t *b)
{
  const mulib_dom_node_t *c, *d;
  if (a == b)
    return 1;
  if (!a || !b || a->type != b->type
      || !mulib_dom_equal_c_ (
          a->type == MULIB_DOM_ATTRIBUTE ? a->local_name : a->name,
          b->type == MULIB_DOM_ATTRIBUTE ? b->local_name : b->name)
      || !mulib_dom_equal_c_ (a->namespace_uri, b->namespace_uri)
      || !mulib_dom_equal_c_ (a->public_id, b->public_id)
      || !mulib_dom_equal_c_ (a->system_id, b->system_id)
      || !mulib_dom_string_equal_ (mulib_dom_node_value (a),
                                   mulib_dom_node_value (b))
      || mulib_dom_attribute_count (a) != mulib_dom_attribute_count (b))
    return 0;
  for (c = a->attributes; c; c = c->next)
    {
      d = mulib_dom_get_attribute_node (b, c->namespace_uri, c->local_name);
      if (!d
          || !mulib_dom_string_equal_ (mulib_dom_node_value (c),
                                       mulib_dom_node_value (d)))
        return 0;
    }
  for (c = a->first, d = b->first; c && d; c = c->next, d = d->next)
    if (!mulib_dom_is_equal_node (c, d))
      return 0;
  return !c && !d;
}
unsigned
mulib_dom_compare_document_position (const mulib_dom_node_t *a,
                                     const mulib_dom_node_t *b)
{
  const mulib_dom_node_t *root, *n, *aa = NULL, *ba = NULL;
  if (a == b)
    return 0;
  if (!a || !b)
    return 1;
  if (a->owner_element)
    {
      aa = a;
      a = a->owner_element;
    }
  if (b->owner_element)
    {
      ba = b;
      b = b->owner_element;
    }
  if (a == b)
    {
      if (!aa)
        return 4 | 16;
      if (!ba)
        return 2 | 8;
      for (n = a->attributes; n; n = n->next)
        {
          if (n == aa)
            return 32 | 4;
          if (n == ba)
            return 32 | 2;
        }
    }
  root = mulib_dom_get_root_node (a);
  if (root != mulib_dom_get_root_node (b))
    return 1 | 32 | ((uintptr_t)a < (uintptr_t)b ? 4 : 2);
  if (mulib_dom_contains (a, b))
    return 4 | (aa ? 0 : 16);
  if (mulib_dom_contains (b, a))
    return 2 | (ba ? 0 : 8);
  for (n = root; n; n = mulib_dom_next_ (n, root))
    {
      if (n == a)
        return 4;
      if (n == b)
        return 2;
    }
  return 1;
}
mulib_dom_node_t *
mulib_dom_get_element_by_id (const mulib_dom_node_t *root,
                             mulib_dom_string_t id)
{
  const mulib_dom_node_t *n;
  if (!root || !id.length || !mulib_dom_string_valid_ (id))
    return NULL;
  for (n = root->first; n; n = mulib_dom_next_ (n, root))
    {
      mulib_dom_node_t *a = mulib_dom_get_attribute_node (n, NULL, "id");
      if (a && mulib_dom_string_equal_ (mulib_dom_node_value (a), id))
        return (mulib_dom_node_t *)n;
    }
  return NULL;
}
int
mulib_dom_get_elements_by_tag_name (const mulib_dom_node_t *root,
                                    const char *ns, const char *local,
                                    mulib_buffer_t *out, mulib_dom_error_t *e)
{
  mulib_buffer_t b = { 0 };
  mulib_dom_node_t *n;
  int ok = 1;
  if (!root || !local || !out)
    return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
  for (n = root->first; n && ok; n = mulib_dom_next_ (n, root))
    if (n->type == MULIB_DOM_ELEMENT
        && ((!ns || strcmp (ns, "*"))
                ? mulib_dom_equal_c_ (ns, n->namespace_uri)
                : 1)
        && (!strcmp (local, "*") || !strcmp (local, n->local_name)))
      ok = mulib_buffer_append (&b, &n, sizeof n);
  if (ok)
    ok = mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return mulib_dom_error_ (e, ok ? MULIB_DOM_OK : MULIB_DOM_OUT_OF_MEMORY);
}
int
mulib_dom_utf8_to_string (const char *s, size_t length, mulib_buffer_t *out)
{
  mulib_buffer_t b = { 0 };
  size_t i = 0;
  int ok = out && (!length || s) && length <= MULIB_DOM_STRING_LIMIT;
  while (i < length && ok)
    {
      uint32_t cp;
      uint16_t units[2];
      size_t used, count = 1;
      if (!mulib_utf8_decode ((const unsigned char *)s + i, length - i, &cp,
                              &used))
        {
          ok = 0;
          break;
        }
      i += used;
      if (cp > 0xffff)
        {
          cp -= 0x10000;
          units[0] = (uint16_t)(0xd800 + (cp >> 10));
          units[1] = (uint16_t)(0xdc00 + (cp & 1023));
          count = 2;
        }
      else
        units[0] = (uint16_t)cp;
      ok = mulib_dom_append_data_ (&b, units, count);
    }
  if (ok)
    ok = mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
int
mulib_dom_string_to_utf8 (mulib_dom_string_t s, mulib_buffer_t *out)
{
  mulib_buffer_t b = { 0 };
  size_t i;
  int ok = out && mulib_dom_string_valid_ (s);
  for (i = 0; i < s.length && ok; ++i)
    {
      uint32_t cp = s.data[i];
      unsigned char bytes[4];
      size_t count;
      if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < s.length
          && s.data[i + 1] >= 0xdc00 && s.data[i + 1] <= 0xdfff)
        {
          cp = 0x10000 + ((cp - 0xd800) << 10) + (s.data[++i] - 0xdc00);
        }
      else if (cp >= 0xd800 && cp <= 0xdfff)
        cp = 0xfffd;
      if (cp < 0x80)
        {
          bytes[0] = (unsigned char)cp;
          count = 1;
        }
      else if (cp < 0x800)
        {
          bytes[0] = (unsigned char)(0xc0 | (cp >> 6));
          bytes[1] = (unsigned char)(0x80 | (cp & 63));
          count = 2;
        }
      else if (cp < 0x10000)
        {
          bytes[0] = (unsigned char)(0xe0 | (cp >> 12));
          bytes[1] = (unsigned char)(0x80 | ((cp >> 6) & 63));
          bytes[2] = (unsigned char)(0x80 | (cp & 63));
          count = 3;
        }
      else
        {
          bytes[0] = (unsigned char)(0xf0 | (cp >> 18));
          bytes[1] = (unsigned char)(0x80 | ((cp >> 12) & 63));
          bytes[2] = (unsigned char)(0x80 | ((cp >> 6) & 63));
          bytes[3] = (unsigned char)(0x80 | (cp & 63));
          count = 4;
        }
      ok = mulib_buffer_append (&b, bytes, count);
    }
  if (ok)
    ok = mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
mulib_dom_node_t *
mulib_dom_create_text_utf8 (mulib_dom_document_t *d, const char *s,
                            size_t length, mulib_dom_error_t *e)
{
  mulib_buffer_t b = { 0 };
  mulib_dom_string_t value;
  mulib_dom_node_t *n;
  size_t offset = 0;
  if ((!s && length) || length > MULIB_DOM_STRING_LIMIT)
    {
      mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
      return NULL;
    }
  while (offset < length)
    {
      uint32_t cp;
      size_t used;
      if (!mulib_utf8_decode ((const unsigned char *)s + offset,
                              length - offset, &cp, &used))
        {
          mulib_dom_error_ (e, MULIB_DOM_INVALID_CHARACTER_ERR);
          return NULL;
        }
      offset += used;
    }
  if (!mulib_dom_utf8_to_string (s, length, &b))
    {
      mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
      return NULL;
    }
  value.data = (const uint16_t *)b.data;
  value.length = b.length / sizeof (uint16_t);
  n = mulib_dom_create_character_data (d, MULIB_DOM_TEXT, NULL, value, e);
  mulib_buffer_free (&b);
  return n;
}
int
mulib_dom_lookup_namespace_uri (const mulib_dom_node_t *n, const char *prefix,
                                mulib_buffer_t *out, mulib_dom_error_t *e)
{
  const char *ns = NULL;
  mulib_dom_error_ (e, MULIB_DOM_OK);
  if (!n || !out)
    {
      mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
      return -1;
    }
  if (!prefix)
    prefix = "";
  if (!strcmp (prefix, "xml"))
    ns = MULIB_DOM_XML_NAMESPACE;
  else if (!strcmp (prefix, "xmlns"))
    ns = MULIB_DOM_XMLNS_NAMESPACE;
  if (n->type == MULIB_DOM_DOCUMENT)
    n = mulib_dom_document_element (n->document);
  if (n && n->type == MULIB_DOM_ATTRIBUTE)
    n = n->owner_element;
  for (; n && !ns; n = n->parent)
    if (n->type == MULIB_DOM_ELEMENT)
      {
        const mulib_dom_node_t *a;
        if (n->namespace_uri && mulib_dom_equal_c_ (n->prefix, prefix))
          {
            ns = n->namespace_uri;
            break;
          }
        a = mulib_dom_get_attribute_node (n, MULIB_DOM_XMLNS_NAMESPACE,
                                          *prefix ? prefix : "xmlns");
        if (a
            && ((*prefix && mulib_dom_equal_c_ (a->prefix, "xmlns"))
                || (!*prefix && !a->prefix)))
          {
            if (!a->length)
              return 0;
            if (mulib_dom_string_to_utf8 (mulib_dom_node_value (a), out))
              return 1;
            mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
            return -1;
          }
      }
  if (!ns)
    return 0;
  if (mulib_buffer_append (out, ns, strlen (ns)))
    return 1;
  mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
  return -1;
}

mulib_dom_event_t *
mulib_dom_event_new (const char *type, int bubbles, int cancelable,
                     void *detail)
{
  mulib_dom_event_t *event;
  if (!type)
    return NULL;
  event = (mulib_dom_event_t *)calloc (1, sizeof *event);
  if (!event)
    return NULL;
  event->type = mulib_dom_copy_ (type);
  if (!event->type)
    {
      free (event);
      return NULL;
    }
  event->bubbles = !!bubbles;
  event->cancelable = !!cancelable;
  event->detail = detail;
  return event;
}
int
mulib_dom_event_destroy (mulib_dom_event_t *e)
{
  if (!e)
    return 1;
  if (e->dispatching)
    return 0;
  free (e->type);
  free (e);
  return 1;
}
const char *
mulib_dom_event_type (const mulib_dom_event_t *e)
{
  return e ? e->type : NULL;
}
void *
mulib_dom_event_detail (const mulib_dom_event_t *e)
{
  return e ? e->detail : NULL;
}
mulib_dom_node_t *
mulib_dom_event_target (const mulib_dom_event_t *e)
{
  return e ? e->target : NULL;
}
mulib_dom_node_t *
mulib_dom_event_current_target (const mulib_dom_event_t *e)
{
  return e ? e->current : NULL;
}
unsigned
mulib_dom_event_phase (const mulib_dom_event_t *e)
{
  return e ? e->phase : 0;
}
int
mulib_dom_event_bubbles (const mulib_dom_event_t *e)
{
  return e && e->bubbles;
}
int
mulib_dom_event_cancelable (const mulib_dom_event_t *e)
{
  return e && e->cancelable;
}
int
mulib_dom_event_composed_path (const mulib_dom_event_t *event,
                               mulib_buffer_t *out, mulib_dom_error_t *e)
{
  if (!event || !out)
    return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
  return mulib_dom_error_ (
      e, mulib_buffer_append (out, event->path,
                              event->path_length * sizeof *event->path)
             ? MULIB_DOM_OK
             : MULIB_DOM_OUT_OF_MEMORY);
}
int
mulib_dom_event_default_prevented (const mulib_dom_event_t *e)
{
  return e && e->canceled;
}
void
mulib_dom_event_prevent_default (mulib_dom_event_t *e)
{
  if (e && e->cancelable && !e->passive)
    e->canceled = 1;
}
void
mulib_dom_event_stop_propagation (mulib_dom_event_t *e)
{
  if (e)
    e->stopped = 1;
}
void
mulib_dom_event_stop_immediate_propagation (mulib_dom_event_t *e)
{
  if (e)
    e->stopped = e->immediate = 1;
}
static void
mulib_dom_collect_listeners_ (mulib_dom_node_t *n)
{
  mulib_dom_listener_ **link = &n->listeners, *l;
  if (n->dispatches)
    return;
  n->listeners_last = NULL;
  while ((l = *link) != NULL)
    {
      if (l->removed)
        {
          *link = l->next;
          free (l->type);
          free (l);
        }
      else
        {
          n->listeners_last = l;
          link = &l->next;
        }
    }
}
int
mulib_dom_add_event_listener (mulib_dom_node_t *n, const char *type,
                              mulib_dom_listener_t callback, void *userdata,
                              const mulib_dom_listener_options_t *o,
                              mulib_dom_error_t *e)
{
  mulib_dom_listener_ *l;
  size_t count = 0;
  int capture = o && o->capture;
  if (!n || !type)
    return mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
  if (!callback)
    return mulib_dom_error_ (e, MULIB_DOM_OK);
  for (l = n->listeners; l; l = l->next)
    {
      ++count;
      if (!l->removed && l->callback == callback && l->userdata == userdata
          && l->capture == capture && !strcmp (l->type, type))
        return mulib_dom_error_ (e, MULIB_DOM_OK);
    }
  if (count >= MULIB_DOM_NODE_LIMIT)
    return mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
  l = (mulib_dom_listener_ *)calloc (1, sizeof *l);
  if (!l)
    return mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
  l->type = mulib_dom_copy_ (type);
  if (!l->type)
    {
      free (l);
      return mulib_dom_error_ (e, MULIB_DOM_OUT_OF_MEMORY);
    }
  l->callback = callback;
  l->userdata = userdata;
  l->capture = capture;
  l->once = o && o->once;
  l->passive = o && o->passive;
  if (n->listeners_last)
    n->listeners_last->next = l;
  else
    n->listeners = l;
  n->listeners_last = l;
  return mulib_dom_error_ (e, MULIB_DOM_OK);
}
void
mulib_dom_remove_event_listener (mulib_dom_node_t *n, const char *type,
                                 mulib_dom_listener_t callback, void *userdata,
                                 int capture)
{
  mulib_dom_listener_ *l;
  if (!n || !type)
    return;
  for (l = n->listeners; l; l = l->next)
    if (!l->removed && l->callback == callback && l->userdata == userdata
        && l->capture == !!capture && !strcmp (l->type, type))
      {
        l->removed = 1;
        break;
      }
  mulib_dom_collect_listeners_ (n);
}
static void
mulib_dom_invoke_ (mulib_dom_node_t *n, mulib_dom_event_t *e, int capture)
{
  mulib_dom_listener_ *l, *last = n->listeners_last;
  e->current = n;
  ++n->dispatches;
  for (l = n->listeners; l && !e->immediate; l = l->next)
    {
      if (!l->removed && l->capture == capture && !strcmp (l->type, e->type))
        {
          if (l->once)
            l->removed = 1;
          e->passive = l->passive;
          l->callback (e, l->userdata);
          e->passive = 0;
        }
      if (l == last)
        break;
    }
  --n->dispatches;
  mulib_dom_collect_listeners_ (n);
}
int
mulib_dom_dispatch_event (mulib_dom_node_t *n, mulib_dom_event_t *event,
                          mulib_dom_error_t *e)
{
  mulib_dom_node_t *path[MULIB_DOM_DEPTH_LIMIT], *p;
  mulib_dom_document_t *d;
  size_t count = 0, i;
  mulib_dom_error_ (e, MULIB_DOM_OK);
  if (!n || !event || event->dispatching)
    {
      mulib_dom_error_ (e, MULIB_DOM_INVALID_STATE_ERR);
      return -1;
    }
  d = n->document;
  if (d->dispatches >= MULIB_DOM_DEPTH_LIMIT)
    {
      mulib_dom_error_ (e, MULIB_DOM_LIMIT_ERR);
      return -1;
    }
  for (p = n; p; p = p->parent)
    path[count++] = p;
  event->path = path;
  event->path_length = count;
  ++d->dispatches;
  event->dispatching = 1;
  event->target = n;
  event->stopped = event->immediate = 0;
  event->phase = 1;
  for (i = count; i > 1 && !event->stopped; --i)
    mulib_dom_invoke_ (path[i - 1], event, 1);
  if (!event->stopped)
    {
      event->phase = 2;
      mulib_dom_invoke_ (n, event, 1);
      if (!event->immediate)
        mulib_dom_invoke_ (n, event, 0);
    }
  event->phase = 3;
  if (event->bubbles)
    for (i = 1; i < count && !event->stopped; ++i)
      mulib_dom_invoke_ (path[i], event, 0);
  event->phase = 0;
  event->current = NULL;
  event->dispatching = 0;
  event->path = NULL;
  event->path_length = 0;
  event->stopped = event->immediate = event->passive = 0;
  --d->dispatches;
  return !event->canceled;
}
#endif /* MULIB_DOM_IMPLEMENTED */
