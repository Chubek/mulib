/**
 * @file mulib-fuse.h
 * @brief Dependency-free userspace filesystem request dispatch to MuLIB VFS.
 * @details This is NOT libfuse, the Linux FUSE wire protocol, a /dev/fuse
 * transport, or a kernel mount implementation. Requests are synchronous typed
 * records useful for embedding, tests and transport adapters. Define
 * MULIB_IMPL_FUSE and separately provide MULIB_IMPL_VFS once.
 */
#ifndef MULIB_FUSE_H
#define MULIB_FUSE_H
#include "mulib-vfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_fuse_opcode
  {
    MULIB_FUSE_GETATTR = 1,
    MULIB_FUSE_READ = 2,
    MULIB_FUSE_WRITE = 3,
    MULIB_FUSE_READDIR = 4
  } mulib_fuse_opcode;
  typedef struct mulib_fuse_request
  {
    uint64_t id;
    mulib_fuse_opcode opcode;
    const char *path;
    size_t offset, length;
    const void *data;
  } mulib_fuse_request;
  typedef struct mulib_fuse_response
  {
    uint64_t id;
    mulib_vfs_result result;
    mulib_vfs_stat info;
    size_t transferred;
  } mulib_fuse_response;
  typedef struct mulib_fuse
  {
    mulib_vfs *vfs;
    int read_only;
  } mulib_fuse;
  /** @brief Optional bounded session. Counters saturate; no request is retained. */
  typedef struct mulib_fuse_session
  {
    mulib_fuse adapter;
    size_t max_transfer;
    uint64_t requests, errors, read_bytes, written_bytes;
    int closed, busy;
  } mulib_fuse_session;
  /** @brief Initialize an adapter borrowing vfs; no allocation or mounting
   * occurs. */
  mulib_vfs_result mulib_fuse_init (mulib_fuse *adapter, mulib_vfs *vfs,
                                    int read_only);
  /** @brief Dispatch one request and initialize response even on failure.
   * READ requires output_capacity >= request.length; WRITE borrows
   * request.data. READDIR calls visit with borrowed immediate-child names and
   * counts delivered entries, including the one that requests cancellation.
   * Other operations ignore output/visit. Unknown opcodes return ENOTSUP. No
   * request pointer is retained.
   */
  mulib_vfs_result mulib_fuse_dispatch (mulib_fuse *adapter,
                                        const mulib_fuse_request *request,
                                        void *output, size_t output_capacity,
                                        mulib_vfs_visit visit, void *user,
                                         mulib_fuse_response *response);
  /** @brief Initialize a session over a borrowed router; transfer quota may be zero. */
  mulib_vfs_result mulib_fuse_session_init (mulib_fuse_session *session,
                                            mulib_vfs *vfs, int read_only,
                                            size_t max_transfer);
  /** @brief Dispatch with transfer limits, statistics and callback reentry rejection. */
  mulib_vfs_result mulib_fuse_session_dispatch (mulib_fuse_session *session,
      const mulib_fuse_request *request, void *output, size_t capacity,
      mulib_vfs_visit visit, void *user, mulib_fuse_response *response);
  /** @brief Close an idle session. Idempotent; subsequent requests are canceled. */
  mulib_vfs_result mulib_fuse_session_close (mulib_fuse_session *session);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_FUSE) && !defined(MULIB_FUSE_IMPLEMENTED)
#define MULIB_FUSE_IMPLEMENTED
#include "include-mulib-container-core.h"
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_vfs_result
  mulib_fuse_init (mulib_fuse *adapter, mulib_vfs *vfs, int read_only)
  {
    if (!adapter || !vfs || !vfs->max_mounts)
      return MULIB_VFS_EINVAL;
    adapter->vfs = vfs;
    adapter->read_only = read_only != 0;
    return MULIB_VFS_OK;
  }
  typedef struct mulib_fuse_listing_
  {
    mulib_vfs_visit visit;
    void *user;
    size_t count;
  } mulib_fuse_listing_;
  static int
  mulib_fuse_visit_ (void *context, const char *name,
                     const mulib_vfs_stat *info)
  {
    mulib_fuse_listing_ *listing = (mulib_fuse_listing_ *)context;
    ++listing->count;
    return listing->visit (listing->user, name, info);
  }
  mulib_vfs_result
  mulib_fuse_dispatch (mulib_fuse *adapter, const mulib_fuse_request *request,
                       void *output, size_t output_capacity,
                       mulib_vfs_visit visit, void *user,
                       mulib_fuse_response *response)
  {
    mulib_vfs_result r = MULIB_VFS_EINVAL;
    mulib_fuse_listing_ listing;
    if (!response)
      return r;
    memset (response, 0, sizeof (*response));
    response->result = r;
    if (request)
      response->id = request->id;
    if (!adapter || !adapter->vfs || !request || !request->path)
      return r;
    switch (request->opcode)
      {
      case MULIB_FUSE_GETATTR:
        r = mulib_vfs_getstat (adapter->vfs, request->path, &response->info);
        break;
      case MULIB_FUSE_READ:
        if (request->length > output_capacity)
          r = MULIB_VFS_ELIMIT;
        else
          r = mulib_vfs_read (adapter->vfs, request->path, request->offset,
                              output, request->length, &response->transferred);
        break;
      case MULIB_FUSE_WRITE:
        if (adapter->read_only)
          r = MULIB_VFS_EROFS;
        else
          r = mulib_vfs_write (adapter->vfs, request->path, request->offset,
                               request->data, request->length,
                               &response->transferred);
        break;
      case MULIB_FUSE_READDIR:
        if (!visit)
          break;
        listing.visit = visit;
        listing.user = user;
        listing.count = 0;
        r = mulib_vfs_list (adapter->vfs, request->path, mulib_fuse_visit_,
                            &listing);
        response->transferred = listing.count;
        break;
      default:
        r = MULIB_VFS_ENOTSUP;
        break;
      }
    response->result = r;
    return r;
  }
  mulib_vfs_result
  mulib_fuse_session_init (mulib_fuse_session *s, mulib_vfs *vfs, int ro,
                           size_t limit)
  {
    mulib_fuse_session next;
    mulib_vfs_result rc;
    if (!s) return MULIB_VFS_EINVAL;
    memset (&next, 0, sizeof (next));
    rc = mulib_fuse_init (&next.adapter, vfs, ro);
    if (rc) return rc;
    next.max_transfer = limit; *s = next;
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_fuse_session_dispatch (mulib_fuse_session *s,
      const mulib_fuse_request *q, void *out, size_t cap,
      mulib_vfs_visit visit, void *user, mulib_fuse_response *r)
  {
    mulib_vfs_result rc;
    if (!r) return MULIB_VFS_EINVAL;
    memset (r, 0, sizeof (*r)); r->id = q ? q->id : 0;
    r->result = MULIB_VFS_EINVAL;
    if (!s || !q) return r->result;
    if (s->closed || s->busy) return r->result = MULIB_VFS_ECANCELED;
    s->requests = mulib_ct_counter_add (s->requests, 1);
    if ((q->opcode == MULIB_FUSE_READ || q->opcode == MULIB_FUSE_WRITE)
        && q->length > s->max_transfer)
      rc = r->result = MULIB_VFS_ELIMIT;
    else
      {
        s->busy = 1;
        rc = mulib_fuse_dispatch (&s->adapter, q, out, cap, visit, user, r);
        s->busy = 0;
      }
    if (rc) s->errors = mulib_ct_counter_add (s->errors, 1);
    if (q->opcode == MULIB_FUSE_READ)
      s->read_bytes = mulib_ct_counter_add (s->read_bytes, r->transferred);
    if (q->opcode == MULIB_FUSE_WRITE)
      s->written_bytes = mulib_ct_counter_add (s->written_bytes, r->transferred);
    return rc;
  }
  mulib_vfs_result
  mulib_fuse_session_close (mulib_fuse_session *s)
  {
    if (!s) return MULIB_VFS_EINVAL;
    if (s->busy) return MULIB_VFS_ECANCELED;
    s->closed = 1;
    return MULIB_VFS_OK;
  }
#ifdef __cplusplus
}
#endif
#endif
