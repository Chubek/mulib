/** @file mulib-static_file.h
 * @brief Strict virtual-filesystem static-file response helper.
 * Define MULIB_IMPL_STATIC_FILE, VFS, MIME, SECURITY, HTTP_RESPONSE and their
 * dependencies. No native filesystem traversal, directory index or ranges. */
#ifndef MULIB_STATIC_FILE_H
#define MULIB_STATIC_FILE_H
#include "mulib-http_response.h"
#include "mulib-mime.h"
#include "mulib-vfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** VFS-absolute path, no query. Limit applies before allocation; output
   * unchanged on error. Missing files return INVALID (caller maps to 404). */
  mulib_http_result mulib_static_file_reply (mulib_vfs *vfs, const char *path,
                                             size_t limit,
                                             mulib_http_message_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_STATIC_FILE) && !defined(MULIB_STATIC_FILE_IMPLEMENTED)
#define MULIB_STATIC_FILE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
mulib_http_result
mulib_static_file_reply (mulib_vfs *v, const char *path, size_t limit,
                         mulib_http_message_t *out)
{
  char safe[MULIB_VFS_PATH_CAP];
  mulib_vfs_stat st;
  unsigned char *p;
  size_t got;
  mulib_http_result rc;
  if (!v || !out || !mulib_security_path (path, safe, sizeof (safe))
      || mulib_vfs_getstat (v, safe, &st) != MULIB_VFS_OK
      || st.kind != MULIB_VFS_FILE)
    return MULIB_HTTP_INVALID;
  if (st.size > limit || st.size > SIZE_MAX)
    return MULIB_HTTP_LIMIT;
  p = (unsigned char *)malloc (st.size ? (size_t)st.size : 1);
  if (!p)
    return MULIB_HTTP_NOMEM;
  if (mulib_vfs_read (v, safe, 0, p, (size_t)st.size, &got) != MULIB_VFS_OK
      || got != st.size)
    {
      free (p);
      return MULIB_HTTP_INVALID;
    }
  rc = mulib_http_response_create (out, 200, mulib_mime_type (safe), p, got);
  free (p);
  return rc;
}
#endif
