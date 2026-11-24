/** @file mulib-epub.h
 * @brief EPUB 3 package generation and checked in-memory publication access.
 * Define MULIB_IMPL_EPUB, ZIP, DEFLATE, BUFFER, CRC32, XML, STRBUF and UTF8.
 */
#ifndef MULIB_EPUB_H
#define MULIB_EPUB_H
#include "mulib-xml.h"
#include "mulib-zip.h"
#include "mulib-html.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief EPUB errors; ZIP failures are mapped to these codes. */
  enum mulib_epub_result
  {
    MULIB_EPUB_OK = 0,
    MULIB_EPUB_EINVAL = -1,
    MULIB_EPUB_EFORMAT = -2,
    MULIB_EPUB_EUNSUPPORTED = -3,
    MULIB_EPUB_ENOMEM = -4,
    MULIB_EPUB_ELIMIT = -5,
    MULIB_EPUB_ENOTFOUND = -6
  };
  /** @brief Writer resource, copied during write. path is relative to EPUB/;
   * title!=NULL puts XHTML in the spine/TOC, in input order. properties may be
   * NULL or space-separated mathml/svg/cover-image. */
  typedef struct mulib_epub_resource
  {
    const char *path, *media_type, *title, *properties;
    const void *data;
    size_t size;
  } mulib_epub_resource_t;
  /** @brief Required metadata and resource array; modified is UTC
   * YYYY-MM-DDThh:mm:ssZ. */
  typedef struct mulib_epub_book
  {
    const char *identifier, *title, *language, *creator, *modified;
    const mulib_epub_resource_t *resources;
    size_t resource_count;
  } mulib_epub_book_t;
  /** @brief Manifest item; strings borrow the reader's package DOM. */
  typedef struct mulib_epub_item
  {
    const char *id, *href, *media_type, *properties;
    size_t zip_index;
  } mulib_epub_item_t;
  /** @brief Zero initialize. Owns index, package DOM, item/spine arrays and
   * path; borrows archive input, which must remain immutable until close. */
  typedef struct mulib_epub_reader
  {
    mulib_zip_reader_t zip;
    mulib_xml_node_t *package;
    char *package_path;
    mulib_epub_item_t *items;
    size_t item_count, *spine, spine_count;
  } mulib_epub_reader_t;
  /** @brief Generate EPUB 3 with compressed resources, OPF, navigation and OCF
   * container. Appends a complete ZIP atomically; inputs may alias out. */
  int mulib_epub_write (const mulib_epub_book_t *book, mulib_buffer_t *out);
  /** @brief Open one-rootfile EPUB 3. xml_limit bounds EACH container/package
   * XML resource after decompression. Validates manifest/spine references;
   * resource CRCs are checked when read. Failure leaves fresh reader
   * unchanged. */
  int mulib_epub_open (mulib_epub_reader_t *reader, const void *data,
                       size_t size, size_t xml_limit);
  /** @brief Extract a manifest item atomically with decoded-size limit. */
  int mulib_epub_read (const mulib_epub_reader_t *reader, size_t item,
                       size_t limit, mulib_buffer_t *out);
  /** @brief Find exact manifest id, returning SIZE_MAX on absence. */
  size_t mulib_epub_find (const mulib_epub_reader_t *reader, const char *id);
  /** @brief Free owned state/reset; does not release input archive. */
  void mulib_epub_close (mulib_epub_reader_t *reader);
  typedef struct mulib_epub_docir_chapter {
    const char *path,*title;const mulib_docir_node_t *root;
  } mulib_epub_docir_chapter_t;
  /** @brief Adapt DocIR chapters through the existing EPUB writer. Existing
   * metadata/resources are retained; identifiers/dates remain caller supplied. */
  int mulib_epub_write_docir(const mulib_epub_book_t *metadata,const mulib_epub_docir_chapter_t *chapters,
                             size_t count,mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EPUB) && !defined(MULIB_EPUB_IMPLEMENTED)
#define MULIB_EPUB_IMPLEMENTED
static int
mulib_epub_zip_result_ (int rc)
{
  if (!rc)
    return 0;
  if (rc == MULIB_ZIP_EINVAL)
    return MULIB_EPUB_EINVAL;
  if (rc == MULIB_ZIP_ENOMEM)
    return MULIB_EPUB_ENOMEM;
  if (rc == MULIB_ZIP_ELIMIT)
    return MULIB_EPUB_ELIMIT;
  if (rc == MULIB_ZIP_EUNSUPPORTED)
    return MULIB_EPUB_EUNSUPPORTED;
  return MULIB_EPUB_EFORMAT;
}
static int
mulib_epub_path_ (const char *s)
{
  const unsigned char *p = (const unsigned char *)s;
  if (!s || !mulib_zip_safe_path (s) || s[strlen (s) - 1] == '/')
    return 0;
  while (*p)
    {
      if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
            || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.'
            || *p == '/' || *p == '~'))
        return 0;
      ++p;
    }
  return 1;
}
static int
mulib_epub_token_ (const char *list, const char *token)
{
  size_t n = strlen (token);
  if (!list)
    return 0;
  while (*list)
    {
      size_t len;
      while (*list == ' ' || *list == '\t' || *list == '\r' || *list == '\n')
        ++list;
      len = strcspn (list, " \t\r\n");
      if (len == n && !memcmp (list, token, n))
        return 1;
      list += len;
    }
  return 0;
}
static int
mulib_epub_properties_ (const char *list)
{
  if (!list)
    return 1;
  while (*list)
    {
      size_t len;
      while (*list == ' ')
        ++list;
      if (!*list)
        break;
      len = strcspn (list, " ");
      if (!((len == 6 && !memcmp (list, "mathml", 6))
            || (len == 3 && !memcmp (list, "svg", 3))
            || (len == 11 && !memcmp (list, "cover-image", 11))))
        return 0;
      list += len;
    }
  return 1;
}
static const char *
mulib_epub_namespace_ (const mulib_xml_node_t *node)
{
  const char *name = mulib_xml_name (node), *colon, *value;
  size_t prefix, i;
  if (!name)
    return NULL;
  colon = strchr (name, ':');
  if (colon && (colon == name || !colon[1] || strchr (colon + 1, ':')))
    return NULL;
  prefix = colon ? (size_t)(colon - name) : 0;
  while (node)
    {
      if (!prefix)
        {
          value = mulib_xml_attr (node, "xmlns");
          if (value)
            return value;
        }
      else
        for (i = 0; i < mulib_xml_attr_count (node); ++i)
          {
            const char *key = mulib_xml_attr_name (node, i);
            if (!strncmp (key, "xmlns:", 6) && strlen (key + 6) == prefix
                && !memcmp (key + 6, name, prefix))
              return mulib_xml_attr_value (node, i);
          }
      node = mulib_xml_parent (node);
    }
  return NULL;
}
static mulib_xml_node_t *
mulib_epub_child_ (const mulib_xml_node_t *parent, const char *name)
{
  mulib_xml_node_t *n, *found = NULL;
  const char *ns = mulib_epub_namespace_ (parent);
  for (n = mulib_xml_first (parent); n; n = mulib_xml_next (n))
    if (mulib_xml_name (n))
      {
        const char *local = strchr (mulib_xml_name (n), ':');
        const char *child_ns = mulib_epub_namespace_ (n);
        local = local ? local + 1 : mulib_xml_name (n);
        if (!strcmp (local, name) && ns && child_ns && !strcmp (ns, child_ns))
          {
            if (found)
              return NULL;
            found = n;
          }
      }
  return found;
}
static int
mulib_epub_named_ (const mulib_xml_node_t *n, const char *name, const char *ns)
{
  const char *v = mulib_epub_namespace_ (n);
  const char *local = mulib_xml_name (n),
             *colon = local ? strchr (local, ':') : NULL;
  if (colon)
    local = colon + 1;
  return local && !strcmp (local, name) && v && !strcmp (v, ns);
}
static int
mulib_epub_xml_error_ (const mulib_xml_error_t *error)
{
  return error->message && !strcmp (error->message, "allocation failed")
             ? MULIB_EPUB_ENOMEM
             : MULIB_EPUB_EFORMAT;
}
static int
mulib_epub_nonblank_ (const char *value)
{
  return value && value[strspn (value, " \t\r\n")];
}
static int
mulib_epub_media_type_ (const char *value)
{
  size_t i, slash = 0;
  if (!value || !*value || *value == '/')
    return 0;
  for (i = 0; value[i]; ++i)
    {
      unsigned char c = (unsigned char)value[i];
      if (c == '/')
        {
          if (slash || !value[i + 1])
            return 0;
          slash = i;
        }
      else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                 || (c >= '0' && c <= '9') || strchr ("!#$&^_.+-", c)))
        return 0;
    }
  return slash != 0;
}
static int
mulib_epub_text_value_ (const mulib_xml_node_t *node, mulib_strbuf_t *out)
{
  const mulib_xml_node_t *child;
  for (child = mulib_xml_first (node); child; child = mulib_xml_next (child))
    {
      mulib_xml_type_t type = mulib_xml_type (child);
      if (type == MULIB_XML_COMMENT)
        continue;
      if (type != MULIB_XML_TEXT && type != MULIB_XML_CDATA)
        return MULIB_EPUB_EFORMAT;
      if (!mulib_strbuf_append (out, mulib_xml_value (child)))
        return MULIB_EPUB_ENOMEM;
    }
  return mulib_epub_nonblank_ (out->data) ? 0 : MULIB_EPUB_EFORMAT;
}
static int
mulib_epub_element_content_ (const mulib_xml_node_t *node)
{
  const mulib_xml_node_t *child;
  for (child = mulib_xml_first (node); child; child = mulib_xml_next (child))
    if (mulib_xml_type (child) != MULIB_XML_ELEMENT
        && mulib_xml_type (child) != MULIB_XML_COMMENT
        && mulib_epub_nonblank_ (mulib_xml_value (child)))
      return 0;
  return 1;
}
static int
mulib_epub_escape_ (mulib_strbuf_t *out, const char *s)
{
  return s && mulib_xml_escape (out, s, strlen (s), 1);
}
static int
mulib_epub_tag_ (mulib_strbuf_t *out, const char *tag, const char *value)
{
  return mulib_strbuf_putc (out, '<') && mulib_strbuf_append (out, tag)
         && mulib_strbuf_putc (out, '>') && mulib_epub_escape_ (out, value)
         && mulib_strbuf_append (out, "</") && mulib_strbuf_append (out, tag)
         && mulib_strbuf_append (out, ">\n");
}
static int
mulib_epub_date_ (const char *s)
{
  static const unsigned days[]
      = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  unsigned y, m, d, h, min, sec, max;
  size_t i;
  if (!s || strlen (s) != 20)
    return 0;
  for (i = 0; i < 20; ++i)
    {
      char expected = i == 4 || i == 7     ? '-'
                      : i == 10            ? 'T'
                      : i == 13 || i == 16 ? ':'
                      : i == 19            ? 'Z'
                                           : 0;
      if (expected ? s[i] != expected : s[i] < '0' || s[i] > '9')
        return 0;
    }
  y = (unsigned)(s[0] - '0') * 1000 + (unsigned)(s[1] - '0') * 100
      + (unsigned)(s[2] - '0') * 10 + (unsigned)(s[3] - '0');
  m = (unsigned)(s[5] - '0') * 10 + (unsigned)(s[6] - '0');
  d = (unsigned)(s[8] - '0') * 10 + (unsigned)(s[9] - '0');
  h = (unsigned)(s[11] - '0') * 10 + (unsigned)(s[12] - '0');
  min = (unsigned)(s[14] - '0') * 10 + (unsigned)(s[15] - '0');
  sec = (unsigned)(s[17] - '0') * 10 + (unsigned)(s[18] - '0');
  if (!y || m < 1 || m > 12 || h > 23 || min > 59 || sec > 59)
    return 0;
  max = days[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
  return d >= 1 && d <= max;
}
static int
mulib_epub_metadata_ (const mulib_xml_node_t *package,
                      const mulib_xml_node_t *metadata)
{
  static const char dc[] = "http://purl.org/dc/elements/1.1/";
  const char *uid = mulib_xml_attr (package, "unique-identifier");
  const mulib_xml_node_t *node;
  unsigned identifiers = 0, titles = 0, languages = 0, modified = 0;
  if (!uid || !mulib_xml_valid_name (uid) || strchr (uid, ':'))
    return MULIB_EPUB_EFORMAT;
  for (node = mulib_xml_first (metadata); node; node = mulib_xml_next (node))
    {
      int identifier = mulib_epub_named_ (node, "identifier", dc);
      int title = mulib_epub_named_ (node, "title", dc);
      int language = mulib_epub_named_ (node, "language", dc);
      const char *property = mulib_xml_attr (node, "property");
      const char *id = mulib_xml_attr (node, "id");
      int date
          = mulib_epub_named_ (node, "meta", "http://www.idpf.org/2007/opf")
            && property && !strcmp (property, "dcterms:modified")
            && !mulib_xml_attr (node, "refines");
      if (id && !strcmp (uid, id) && !identifier)
        return MULIB_EPUB_EFORMAT;
      if (identifier || title || language || date)
        {
          mulib_strbuf_t text = { 0 };
          int rc = mulib_epub_text_value_ (node, &text);
          if (!rc && date && !mulib_epub_date_ (text.data))
            rc = MULIB_EPUB_EFORMAT;
          mulib_strbuf_free (&text);
          if (rc)
            return rc;
          if (identifier && id && !strcmp (uid, id))
            ++identifiers;
          if (title)
            ++titles;
          if (language)
            ++languages;
          if (date)
            ++modified;
        }
    }
  return identifiers == 1 && titles && languages && modified == 1
             ? 0
             : MULIB_EPUB_EFORMAT;
}
int
mulib_epub_write (const mulib_epub_book_t *book, mulib_buffer_t *out)
{
  static const char container[]
      = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<container "
        "version=\"1.0\" "
        "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\"><rootfiles>"
        "<rootfile full-path=\"EPUB/package.opf\" "
        "media-type=\"application/oebps-package+xml\"/></rootfiles></"
        "container>";
  mulib_zip_writer_t zip = { 0 };
  mulib_strbuf_t opf = { 0 }, nav = { 0 }, path = { 0 };
  size_t i, spine = 0, covers = 0;
  int rc = MULIB_EPUB_ENOMEM;
  char id[64];
  if (!book || !out || !mulib_epub_nonblank_ (book->identifier)
      || !mulib_epub_nonblank_ (book->title)
      || !mulib_epub_nonblank_ (book->language)
      || !mulib_epub_date_ (book->modified) || !book->resources
      || !book->resource_count)
    return MULIB_EPUB_EINVAL;
  if (book->resource_count > 65530)
    return MULIB_EPUB_ELIMIT;
  if (!mulib_xml_valid_text (book->identifier, strlen (book->identifier))
      || !mulib_xml_valid_text (book->title, strlen (book->title))
      || !mulib_xml_valid_text (book->language, strlen (book->language))
      || (book->creator
          && !mulib_xml_valid_text (book->creator, strlen (book->creator))))
    return MULIB_EPUB_EINVAL;
  for (i = 0; i < book->resource_count; ++i)
    {
      const mulib_epub_resource_t *r = &book->resources[i];
      if (!mulib_epub_path_ (r->path) || !strcmp (r->path, "package.opf")
          || !strcmp (r->path, "nav.xhtml")
          || !mulib_epub_media_type_ (r->media_type) || (!r->data && r->size)
          || !mulib_epub_properties_ (r->properties))
        return MULIB_EPUB_EINVAL;
      if (mulib_epub_token_ (r->properties, "cover-image")
          && (++covers > 1 || strncmp (r->media_type, "image/", 6)))
        return MULIB_EPUB_EINVAL;
      if (r->title)
        {
          if (strcmp (r->media_type, "application/xhtml+xml")
              || !mulib_xml_valid_text (r->title, strlen (r->title)))
            return MULIB_EPUB_EINVAL;
          ++spine;
        }
      if (!strcmp (r->media_type, "application/xhtml+xml"))
        {
          mulib_xml_error_t error;
          mulib_xml_node_t *doc
              = mulib_xml_parse ((const char *)r->data, r->size, &error);
          if (!doc)
            return mulib_epub_xml_error_ (&error);
          int valid = doc
                      && mulib_epub_named_ (mulib_xml_root (doc), "html",
                                            "http://www.w3.org/1999/xhtml");
          mulib_xml_destroy (doc);
          if (!valid)
            return MULIB_EPUB_EFORMAT;
        }
    }
  if (!spine)
    return MULIB_EPUB_EINVAL;
  if (!mulib_strbuf_append (
          &opf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<package "
                "xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" "
                "unique-identifier=\"book-id\"><metadata "
                "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:identifier "
                "id=\"book-id\">")
      || !mulib_epub_escape_ (&opf, book->identifier)
      || !mulib_strbuf_append (&opf, "</dc:identifier>\n")
      || !mulib_epub_tag_ (&opf, "dc:title", book->title)
      || !mulib_epub_tag_ (&opf, "dc:language", book->language)
      || (book->creator
          && !mulib_epub_tag_ (&opf, "dc:creator", book->creator))
      || !mulib_strbuf_append (&opf, "<meta property=\"dcterms:modified\">")
      || !mulib_epub_escape_ (&opf, book->modified)
      || !mulib_strbuf_append (
          &opf,
          "</meta></metadata><manifest><item id=\"nav\" href=\"nav.xhtml\" "
          "media-type=\"application/xhtml+xml\" properties=\"nav\"/>"))
    goto done;
  if (!mulib_strbuf_append (
          &nav, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<html "
                "xmlns=\"http://www.w3.org/1999/xhtml\" "
                "xmlns:epub=\"http://www.idpf.org/2007/ops\"><head>")
      || !mulib_epub_tag_ (&nav, "title", book->title)
      || !mulib_strbuf_append (&nav, "</head><body><nav epub:type=\"toc\" "
                                     "id=\"toc\"><h1>Contents</h1><ol>"))
    goto done;
  for (i = 0; i < book->resource_count; ++i)
    {
      const mulib_epub_resource_t *r = &book->resources[i];
      (void)snprintf (id, sizeof (id), "<item id=\"r%zu\" href=\"", i);
      if (!mulib_strbuf_append (&opf, id)
          || !mulib_epub_escape_ (&opf, r->path)
          || !mulib_strbuf_append (&opf, "\" media-type=\"")
          || !mulib_epub_escape_ (&opf, r->media_type))
        goto done;
      if (r->properties
          && (!mulib_strbuf_append (&opf, "\" properties=\"")
              || !mulib_epub_escape_ (&opf, r->properties)))
        goto done;
      if (!mulib_strbuf_append (&opf, "\"/>"))
        goto done;
      if (r->title
          && (!mulib_strbuf_append (&nav, "<li><a href=\"")
              || !mulib_epub_escape_ (&nav, r->path)
              || !mulib_strbuf_append (&nav, "\">")
              || !mulib_epub_escape_ (&nav, r->title)
              || !mulib_strbuf_append (&nav, "</a></li>")))
        goto done;
    }
  if (!mulib_strbuf_append (&opf, "</manifest><spine>"))
    goto done;
  for (i = 0; i < book->resource_count; ++i)
    if (book->resources[i].title)
      {
        (void)snprintf (id, sizeof (id), "<itemref idref=\"r%zu\"/>", i);
        if (!mulib_strbuf_append (&opf, id))
          goto done;
      }
  if (!mulib_strbuf_append (&opf, "</spine></package>")
      || !mulib_strbuf_append (&nav, "</ol></nav></body></html>"))
    goto done;
  rc = mulib_epub_zip_result_ (
      mulib_zip_add (&zip, "mimetype", "application/epub+zip", 20, 0));
  if (rc)
    goto done;
  rc = mulib_epub_zip_result_ (mulib_zip_add (
      &zip, "META-INF/container.xml", container, sizeof (container) - 1, 8));
  if (rc)
    goto done;
  rc = mulib_epub_zip_result_ (
      mulib_zip_add (&zip, "EPUB/package.opf", opf.data, opf.length, 8));
  if (rc)
    goto done;
  rc = mulib_epub_zip_result_ (
      mulib_zip_add (&zip, "EPUB/nav.xhtml", nav.data, nav.length, 8));
  if (rc)
    goto done;
  for (i = 0; i < book->resource_count; ++i)
    {
      const mulib_epub_resource_t *r = &book->resources[i];
      mulib_strbuf_free (&path);
      if (!mulib_strbuf_append (&path, "EPUB/")
          || !mulib_strbuf_append (&path, r->path))
        {
          rc = MULIB_EPUB_ENOMEM;
          goto done;
        }
      rc = mulib_epub_zip_result_ (
          mulib_zip_add (&zip, path.data, r->data, r->size, 8));
      if (rc)
        goto done;
    }
  rc = mulib_epub_zip_result_ (mulib_zip_finish (&zip));
  if (!rc && !mulib_buffer_append (out, zip.bytes.data, zip.bytes.length))
    rc = MULIB_EPUB_ENOMEM;
done:
  mulib_zip_writer_free (&zip);
  mulib_strbuf_free (&opf);
  mulib_strbuf_free (&nav);
  mulib_strbuf_free (&path);
  return rc;
}
void
mulib_epub_close (mulib_epub_reader_t *r)
{
  if (r)
    {
      mulib_zip_close (&r->zip);
      mulib_xml_destroy (r->package);
      free (r->package_path);
      free (r->items);
      free (r->spine);
      memset (r, 0, sizeof (*r));
    }
}
size_t
mulib_epub_find (const mulib_epub_reader_t *r, const char *id)
{
  size_t i;
  if (!r || !id)
    return SIZE_MAX;
  for (i = 0; i < r->item_count; ++i)
    if (!strcmp (r->items[i].id, id))
      return i;
  return SIZE_MAX;
}
static int
mulib_epub_xml_ (const mulib_zip_reader_t *zip, const char *path, size_t limit,
                 mulib_xml_node_t **out)
{
  const mulib_zip_entry_t *e = mulib_zip_find (zip, path);
  mulib_buffer_t bytes = { 0 };
  int rc;
  if (!e)
    return MULIB_EPUB_EFORMAT;
  rc = mulib_epub_zip_result_ (
      mulib_zip_extract (zip, (size_t)(e - zip->entries), limit, &bytes));
  if (!rc)
    {
      mulib_xml_error_t error;
      *out = mulib_xml_parse ((const char *)bytes.data, bytes.length, &error);
      if (!*out)
        rc = mulib_epub_xml_error_ (&error);
    }
  mulib_buffer_free (&bytes);
  return rc;
}
int
mulib_epub_open (mulib_epub_reader_t *out, const void *data, size_t size,
                 size_t limit)
{
  mulib_epub_reader_t r;
  mulib_xml_node_t *container = NULL, *root, *node, *rootfiles, *manifest,
                   *spine, *metadata;
  const mulib_zip_entry_t *entry;
  const char *path, *value, *slash;
  mulib_strbuf_t resolved = { 0 };
  size_t count = 0, i, base, nav = 0, covers = 0;
  int rc;
  if (!out || (!data && size) || out->zip.data || out->zip.entries
      || out->package || out->items || out->spine || out->package_path)
    return MULIB_EPUB_EINVAL;
  memset (&r, 0, sizeof (r));
  rc = mulib_epub_zip_result_ (mulib_zip_open (&r.zip, data, size));
  if (rc)
    return rc;
  rc = MULIB_EPUB_EFORMAT;
  if (!r.zip.count)
    goto done;
  entry = &r.zip.entries[0];
  if (entry->offset || entry->name_length != 8
      || memcmp (entry->name, "mimetype", 8) || entry->method
      || entry->flags & 8 || entry->size != 20 || entry->data_offset != 38
      || memcmp (r.zip.data + entry->data_offset, "application/epub+zip", 20)
      || entry->crc != mulib_crc32 ("application/epub+zip", 20))
    goto done;
  if (mulib_zip_find (&r.zip, "META-INF/encryption.xml"))
    {
      rc = MULIB_EPUB_EUNSUPPORTED;
      goto done;
    }
  rc = mulib_epub_xml_ (&r.zip, "META-INF/container.xml", limit, &container);
  if (rc)
    goto done;
  rc = MULIB_EPUB_EFORMAT;
  root = mulib_xml_root (container);
  if (!mulib_epub_named_ (root, "container",
                          "urn:oasis:names:tc:opendocument:xmlns:container"))
    goto done;
  value = mulib_xml_attr (root, "version");
  if (!value || strcmp (value, "1.0"))
    goto done;
  rootfiles = mulib_epub_child_ (root, "rootfiles");
  if (!rootfiles || !mulib_epub_element_content_ (root)
      || !mulib_epub_element_content_ (rootfiles))
    goto done;
  node = NULL;
  for (root = mulib_xml_first (rootfiles); root; root = mulib_xml_next (root))
    if (mulib_xml_type (root) == MULIB_XML_ELEMENT)
      {
        if (!mulib_epub_named_ (
                root, "rootfile",
                "urn:oasis:names:tc:opendocument:xmlns:container"))
          goto done;
        if (node)
          {
            rc = MULIB_EPUB_EUNSUPPORTED;
            goto done;
          }
        node = root;
      }
  if (!node)
    goto done;
  path = mulib_xml_attr (node, "full-path");
  value = mulib_xml_attr (node, "media-type");
  if (!mulib_epub_path_ (path) || !value
      || strcmp (value, "application/oebps-package+xml"))
    goto done;
  r.package_path = (char *)malloc (strlen (path) + 1);
  if (!r.package_path)
    {
      rc = MULIB_EPUB_ENOMEM;
      goto done;
    }
  strcpy (r.package_path, path);
  rc = mulib_epub_xml_ (&r.zip, path, limit, &r.package);
  if (rc)
    goto done;
  rc = MULIB_EPUB_EFORMAT;
  root = mulib_xml_root (r.package);
  if (!mulib_epub_named_ (root, "package", "http://www.idpf.org/2007/opf"))
    goto done;
  value = mulib_xml_attr (root, "version");
  if (!value || strcmp (value, "3.0"))
    {
      rc = MULIB_EPUB_EUNSUPPORTED;
      goto done;
    }
  metadata = mulib_epub_child_ (root, "metadata");
  manifest = mulib_epub_child_ (root, "manifest");
  spine = mulib_epub_child_ (root, "spine");
  if (!metadata || !manifest || !spine || !mulib_epub_element_content_ (root)
      || !mulib_epub_element_content_ (metadata)
      || !mulib_epub_element_content_ (manifest)
      || !mulib_epub_element_content_ (spine))
    goto done;
  rc = mulib_epub_metadata_ (root, metadata);
  if (rc)
    goto done;
  rc = MULIB_EPUB_EFORMAT;
  for (node = mulib_xml_first (manifest); node; node = mulib_xml_next (node))
    if (mulib_xml_type (node) == MULIB_XML_ELEMENT)
      ++count;
  if (!count || count > r.zip.count)
    goto done;
  r.items = (mulib_epub_item_t *)calloc (count, sizeof (*r.items));
  if (!r.items)
    {
      rc = MULIB_EPUB_ENOMEM;
      goto done;
    }
  slash = strrchr (r.package_path, '/');
  base = slash ? (size_t)(slash - r.package_path) + 1 : 0;
  for (node = mulib_xml_first (manifest); node; node = mulib_xml_next (node))
    {
      mulib_epub_item_t item;
      if (mulib_xml_type (node) != MULIB_XML_ELEMENT)
        continue;
      if (!mulib_epub_named_ (node, "item", "http://www.idpf.org/2007/opf"))
        goto done;
      item.id = mulib_xml_attr (node, "id");
      item.href = mulib_xml_attr (node, "href");
      item.media_type = mulib_xml_attr (node, "media-type");
      item.properties = mulib_xml_attr (node, "properties");
      if (!mulib_xml_valid_name (item.id) || strchr (item.id, ':')
          || !strcmp (item.id, mulib_xml_attr (root, "unique-identifier"))
          || !mulib_epub_media_type_ (item.media_type)
          || !mulib_epub_path_ (item.href)
          || mulib_epub_find (&r, item.id) != SIZE_MAX)
        goto done;
      if (mulib_epub_token_ (item.properties, "cover-image")
          && (++covers > 1 || strncmp (item.media_type, "image/", 6)))
        goto done;
      mulib_strbuf_free (&resolved);
      if (!mulib_strbuf_append_n (&resolved, r.package_path, base)
          || !mulib_strbuf_append (&resolved, item.href))
        {
          rc = MULIB_EPUB_ENOMEM;
          goto done;
        }
      entry = mulib_zip_find (&r.zip, resolved.data);
      if (!entry)
        goto done;
      item.zip_index = (size_t)(entry - r.zip.entries);
      for (i = 0; i < r.item_count; ++i)
        if (r.items[i].zip_index == item.zip_index)
          goto done;
      if (mulib_epub_token_ (item.properties, "nav"))
        {
          if (strcmp (item.media_type, "application/xhtml+xml"))
            goto done;
          ++nav;
        }
      r.items[r.item_count++] = item;
    }
  if (nav != 1)
    goto done;
  count = 0;
  for (node = mulib_xml_first (spine); node; node = mulib_xml_next (node))
    if (mulib_xml_type (node) == MULIB_XML_ELEMENT)
      ++count;
  if (!count || count > r.item_count)
    goto done;
  r.spine = (size_t *)calloc (count, sizeof (*r.spine));
  if (!r.spine)
    {
      rc = MULIB_EPUB_ENOMEM;
      goto done;
    }
  for (node = mulib_xml_first (spine); node; node = mulib_xml_next (node))
    {
      size_t item;
      if (mulib_xml_type (node) != MULIB_XML_ELEMENT)
        continue;
      item = mulib_epub_find (&r, mulib_xml_attr (node, "idref"));
      value = mulib_xml_attr (node, "linear");
      if (!mulib_epub_named_ (node, "itemref", "http://www.idpf.org/2007/opf")
          || item == SIZE_MAX
          || strcmp (r.items[item].media_type, "application/xhtml+xml")
          || (value && strcmp (value, "yes") && strcmp (value, "no")))
        goto done;
      for (i = 0; i < r.spine_count; ++i)
        if (r.spine[i] == item)
          goto done;
      r.spine[r.spine_count++] = item;
    }
  *out = r;
  memset (&r, 0, sizeof (r));
  rc = 0;
done:
  mulib_xml_destroy (container);
  mulib_strbuf_free (&resolved);
  mulib_epub_close (&r);
  return rc;
}
int
mulib_epub_read (const mulib_epub_reader_t *r, size_t item, size_t limit,
                 mulib_buffer_t *out)
{
  if (!r || item >= r->item_count || !out)
    return MULIB_EPUB_EINVAL;
  return mulib_epub_zip_result_ (
      mulib_zip_extract (&r->zip, r->items[item].zip_index, limit, out));
}
#include "detail/mulib-epub-docir.h"
#endif
