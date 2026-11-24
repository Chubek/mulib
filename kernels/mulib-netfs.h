/** @file mulib-netfs.h
 * @brief Network filesystem: bounded wire RPC to mulib-fuse and remote VFS
 * mounts. Define MULIB_IMPL_NETFS and FUSE, VFS, ENDIAN, XTKS implementations
 * once. MuLIB-specific protocol, synchronous, one outstanding call per client.
 */
#ifndef MULIB_NETFS_H
#define MULIB_NETFS_H
#include "mulib-endian.h"
#include "mulib-fuse.h"
#include "mulib-xtks.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Exchange one complete request/response body, 1 success/0 transport
   * failure. All pointers borrowed for call; reply length <= capacity. A
   * failed WRITE may already have taken effect remotely; never retry it
   * implicitly. */
  typedef int (*mulib_netfs_exchange_fn) (void *, const void *, size_t, void *,
                                          size_t, size_t *);
  typedef struct mulib_netfs
  {
    mulib_netfs_exchange_fn exchange;
    void *context;
    size_t max_frame;
    uint64_t next_id;
    int busy;
  } mulib_netfs_t;
  /** @brief Initialize remote client, max_frame 64..16 MiB including headers.
   * Borrow transport/context. 1 success/0 invalid. IDs never wrap. */
  int mulib_netfs_init (mulib_netfs_t *, mulib_netfs_exchange_fn, void *,
                        size_t max_frame);
  /** @brief VFS backend operations; mount with a live mulib_netfs_t context.
   * Supports GETATTR, READ, WRITE, READDIR through mulib_fuse_dispatch
   * remotely. Each operation allocates at most two max_frame buffers.
   * read/write require count, stat requires info, list requires visit. */
  mulib_vfs_ops mulib_netfs_ops (void);
  /** @brief Dispatch one wire request through FUSE. Returns 1 with an encoded
   * response (which may contain a filesystem error), 0 for invalid arguments,
   * malformed envelope or insufficient response header capacity. used=0 on 0.
   * READDIR must fit one response; overflow returns ELIMIT, no partial
   * listing. Output/input must not overlap. Transport authentication is caller
   * policy. */
  int mulib_netfs_serve (mulib_fuse *, const void *, size_t, void *, size_t,
                         size_t *used);
  /** @brief Blocking X transport adapter: 32-bit BE length then body. A
   * partial transfer/AGAIN/error permanently poisons this adapter; caller must
   * reconnect. The stream and context are borrowed, with no automatic close.
   */
  typedef struct mulib_netfs_stream
  {
    mulib_xtks_t io;
    int failed;
  } mulib_netfs_stream_t;
  int mulib_netfs_stream_exchange (void *, const void *, size_t, void *,
                                   size_t, size_t *);
  /** @brief Serve one framed request using caller's two disjoint scratch
   * buffers. Capacity 64..16 MiB. 1 served, 0 failure (stream becomes
   * poisoned). */
  int mulib_netfs_stream_serve (mulib_netfs_stream_t *, mulib_fuse *,
                                void *request, void *response,
                                size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETFS) && !defined(MULIB_NETFS_IMPLEMENTED)
#define MULIB_NETFS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
static uint64_t
mulib_netfs_u64_ (const unsigned char *p)
{
  return (uint64_t)mulib_endian_load_be32 (p) << 32
         | mulib_endian_load_be32 (p + 4);
}
static void
mulib_netfs_put64_ (unsigned char *p, uint64_t v)
{
  mulib_endian_store_be32 (p, (uint32_t)(v >> 32));
  mulib_endian_store_be32 (p + 4, (uint32_t)v);
}
static int
mulib_netfs_name_ (const char *p, size_t n)
{
  if (!n || n >= MULIB_VFS_PATH_CAP || (n == 1 && p[0] == '.')
      || (n == 2 && p[0] == '.' && p[1] == '.'))
    return 0;
  for (size_t i = 0; i < n; ++i)
    if ((unsigned char)p[i] < 32 || p[i] == 127 || p[i] == '/' || p[i] == '\\')
      return 0;
  return 1;
}
typedef struct mulib_netfs_listing_
{
  unsigned char *p;
  size_t used, capacity;
  int error;
} mulib_netfs_listing_;
static int
mulib_netfs_list_entry_ (void *ctx, const char *name,
                         const mulib_vfs_stat *info)
{
  mulib_netfs_listing_ *s = ctx;
  size_t n;
  if (!name || !info
      || (info->kind != MULIB_VFS_FILE && info->kind != MULIB_VFS_DIRECTORY))
    {
      s->error = MULIB_VFS_EIO;
      return 1;
    }
  n = strlen (name);
  if (!mulib_netfs_name_ (name, n))
    {
      s->error = MULIB_VFS_EIO;
      return 1;
    }
  if (s->capacity - s->used < 16 || n > s->capacity - s->used - 16)
    {
      s->error = MULIB_VFS_ELIMIT;
      return 1;
    }
  mulib_endian_store_be32 (s->p + s->used, (uint32_t)info->kind);
  mulib_netfs_put64_ (s->p + s->used + 4, info->size);
  mulib_endian_store_be32 (s->p + s->used + 12, (uint32_t)n);
  memcpy (s->p + s->used + 16, name, n);
  s->used += n + 16;
  return 0;
}
int
mulib_netfs_serve (mulib_fuse *fuse, const void *data, size_t n, void *output,
                   size_t cap, size_t *used)
{
  const unsigned char *p = data;
  unsigned char *out = output;
  char path[MULIB_VFS_PATH_CAP], normalized[MULIB_VFS_PATH_CAP];
  mulib_fuse_request req;
  mulib_fuse_response response;
  mulib_netfs_listing_ listing;
  uint64_t off;
  size_t pn, length, body = 0;
  mulib_vfs_result r;
  if (used)
    *used = 0;
  if (!fuse || !p || !out || !used || n < 32 || n > 16777216 || cap < 36
      || cap > 16777216 || memcmp (p, "MNF1", 4) || p[5] || p[6] || p[7])
    return 0;
  pn = mulib_endian_load_be32 (p + 28);
  length = mulib_endian_load_be32 (p + 24);
  off = mulib_netfs_u64_ (p + 16);
  if (!pn || pn >= sizeof (path) || pn > n - 32 || memchr (p + 32, 0, pn)
      || off > SIZE_MAX)
    return 0;
  memcpy (path, p + 32, pn);
  path[pn] = 0;
  if (mulib_vfs_normalize (path, normalized, sizeof (normalized))
          != MULIB_VFS_OK
      || strcmp (path, normalized))
    return 0;
  if (p[4] == MULIB_FUSE_WRITE ? length != n - 32 - pn : n != 32 + pn)
    return 0;
  if (p[4] != MULIB_FUSE_READ && p[4] != MULIB_FUSE_WRITE && (off || length))
    return 0;
  memset (&req, 0, sizeof (req));
  req.id = mulib_netfs_u64_ (p + 8);
  req.opcode = (mulib_fuse_opcode)p[4];
  req.path = path;
  req.offset = (size_t)off;
  req.length = length;
  req.data = p + 32 + pn;
  listing.p = out + 36;
  listing.used = 0;
  listing.capacity = cap - 36;
  listing.error = 0;
  r = mulib_fuse_dispatch (fuse, &req, out + 36, cap - 36,
                           mulib_netfs_list_entry_, &listing, &response);
  if (listing.error)
    r = (mulib_vfs_result)listing.error;
  if (r > 0 || r < MULIB_VFS_EFORMAT)
    r = MULIB_VFS_EIO;
  if (r == MULIB_VFS_OK)
    {
      if (req.opcode == MULIB_FUSE_READ)
        body = response.transferred;
      if (req.opcode == MULIB_FUSE_READDIR)
        body = listing.used;
      if (body > cap - 36 || response.transferred > UINT32_MAX)
        {
          r = MULIB_VFS_EIO;
          body = 0;
        }
    }
  memset (out, 0, 36);
  memcpy (out, "MNR1", 4);
  mulib_endian_store_be32 (out + 4, (uint32_t)-(int)r);
  mulib_netfs_put64_ (out + 8, req.id);
  if (r == MULIB_VFS_OK)
    {
      mulib_endian_store_be32 (out + 16, (uint32_t)response.info.kind);
      mulib_netfs_put64_ (out + 20, response.info.size);
      mulib_endian_store_be32 (out + 28, (uint32_t)response.transferred);
      mulib_endian_store_be32 (out + 32, (uint32_t)body);
    }
  *used = 36 + body;
  return 1;
}
int
mulib_netfs_init (mulib_netfs_t *s, mulib_netfs_exchange_fn fn, void *ctx,
                  size_t limit)
{
  if (!s || !fn || limit < 64 || limit > 16777216)
    return 0;
  s->exchange = fn;
  s->context = ctx;
  s->max_frame = limit;
  s->next_id = 1;
  s->busy = 0;
  return 1;
}
static mulib_vfs_result
mulib_netfs_call_ (mulib_netfs_t *s, unsigned opcode, const char *path,
                   size_t offset, const void *input, void *output,
                   size_t length, size_t *count, mulib_vfs_stat *info,
                   mulib_vfs_visit visit, void *user)
{
  unsigned char *req = NULL, *reply = NULL;
  size_t pn, rn = 0, request_size, body, transferred;
  uint64_t id;
  mulib_vfs_result r = MULIB_VFS_EINVAL;
  unsigned status, kind;
  uint64_t size;
  if (count)
    *count = 0;
  if (!s || !s->exchange || s->busy || !s->next_id || !path
      || s->max_frame < 64 || s->max_frame > 16777216)
    return r;
  if (((opcode == MULIB_FUSE_READ || opcode == MULIB_FUSE_WRITE) && !count)
      || (opcode == MULIB_FUSE_GETATTR && !info)
      || (opcode == MULIB_FUSE_READDIR && !visit))
    return r;
  pn = strlen (path);
  if (!pn || pn >= MULIB_VFS_PATH_CAP || pn > s->max_frame - 32
      || length > UINT32_MAX)
    return MULIB_VFS_ELIMIT;
  request_size = 32 + pn;
  if (opcode == MULIB_FUSE_WRITE)
    {
      if ((!input && length) || length > s->max_frame - request_size)
        return MULIB_VFS_ELIMIT;
      request_size += length;
    }
  if (opcode == MULIB_FUSE_READ
      && ((!output && length) || length > s->max_frame - 36))
    return MULIB_VFS_ELIMIT;
  req = malloc (request_size);
  reply = malloc (s->max_frame);
  if (!req || !reply)
    {
      r = MULIB_VFS_ENOMEM;
      goto done;
    }
  id = s->next_id++;
  s->busy = 1;
  memset (req, 0, 32);
  memcpy (req, "MNF1", 4);
  req[4] = (unsigned char)opcode;
  mulib_netfs_put64_ (req + 8, id);
  mulib_netfs_put64_ (req + 16, offset);
  mulib_endian_store_be32 (req + 24, (uint32_t)length);
  mulib_endian_store_be32 (req + 28, (uint32_t)pn);
  memcpy (req + 32, path, pn);
  if (opcode == MULIB_FUSE_WRITE && length)
    memcpy (req + 32 + pn, input, length);
  r = MULIB_VFS_EIO;
  if (s->exchange (s->context, req, request_size, reply, s->max_frame, &rn)
          != 1
      || rn < 36 || rn > s->max_frame || memcmp (reply, "MNR1", 4)
      || mulib_netfs_u64_ (reply + 8) != id)
    goto done;
  status = mulib_endian_load_be32 (reply + 4);
  kind = mulib_endian_load_be32 (reply + 16);
  size = mulib_netfs_u64_ (reply + 20);
  transferred = mulib_endian_load_be32 (reply + 28);
  body = mulib_endian_load_be32 (reply + 32);
  if (body != rn - 36 || status > 12)
    goto done;
  if (status)
    {
      if (!body && !kind && !size && !transferred)
        r = (mulib_vfs_result) - (int)status;
      goto done;
    }
  if (opcode == MULIB_FUSE_GETATTR)
    {
      if (body || transferred
          || (kind != MULIB_VFS_FILE && kind != MULIB_VFS_DIRECTORY))
        goto done;
      info->kind = (mulib_vfs_kind)kind;
      info->size = size;
    }
  else
    {
      if (kind || size)
        goto done;
      if (opcode == MULIB_FUSE_READ || opcode == MULIB_FUSE_WRITE)
        {
          if (transferred > length
              || body != (opcode == MULIB_FUSE_READ ? transferred : 0))
            goto done;
          if (body)
            memcpy (output, reply + 36, body);
          *count = transferred;
        }
      else
        {
          size_t pos = 36, entries = 0;
          /* Validate the complete listing before any user callbacks. */
          while (pos < rn)
            {
              size_t namesize;
              unsigned k;
              if (rn - pos < 16)
                goto done;
              k = mulib_endian_load_be32 (reply + pos);
              namesize = mulib_endian_load_be32 (reply + pos + 12);
              if ((k != MULIB_VFS_FILE && k != MULIB_VFS_DIRECTORY)
                  || namesize > rn - pos - 16
                  || !mulib_netfs_name_ ((const char *)reply + pos + 16,
                                         namesize))
                goto done;
              pos += 16 + namesize;
              ++entries;
            }
          if (entries != transferred)
            goto done;
          for (pos = 36; pos < rn;)
            {
              char name[MULIB_VFS_PATH_CAP];
              mulib_vfs_stat st;
              size_t namesize = mulib_endian_load_be32 (reply + pos + 12);
              st.kind = (mulib_vfs_kind)mulib_endian_load_be32 (reply + pos);
              st.size = mulib_netfs_u64_ (reply + pos + 4);
              memcpy (name, reply + pos + 16, namesize);
              name[namesize] = 0;
              if (visit (user, name, &st))
                {
                  r = MULIB_VFS_ECANCELED;
                  goto done;
                }
              pos += 16 + namesize;
            }
        }
    }
  r = MULIB_VFS_OK;
done:
  s->busy = 0;
  free (req);
  free (reply);
  return r;
}
static mulib_vfs_result
mulib_netfs_read_ (void *s, const char *p, size_t off, void *out, size_t n,
                   size_t *count)
{
  return mulib_netfs_call_ (s, MULIB_FUSE_READ, p, off, NULL, out, n, count,
                            NULL, NULL, NULL);
}
static mulib_vfs_result
mulib_netfs_write_ (void *s, const char *p, size_t off, const void *in,
                    size_t n, size_t *count)
{
  return mulib_netfs_call_ (s, MULIB_FUSE_WRITE, p, off, in, NULL, n, count,
                            NULL, NULL, NULL);
}
static mulib_vfs_result
mulib_netfs_stat_ (void *s, const char *p, mulib_vfs_stat *st)
{
  return mulib_netfs_call_ (s, MULIB_FUSE_GETATTR, p, 0, NULL, NULL, 0, NULL,
                            st, NULL, NULL);
}
static mulib_vfs_result
mulib_netfs_list_ (void *s, const char *p, mulib_vfs_visit visit, void *user)
{
  return mulib_netfs_call_ (s, MULIB_FUSE_READDIR, p, 0, NULL, NULL, 0, NULL,
                            NULL, visit, user);
}
mulib_vfs_ops
mulib_netfs_ops (void)
{
  mulib_vfs_ops ops;
  ops.read = mulib_netfs_read_;
  ops.write = mulib_netfs_write_;
  ops.stat = mulib_netfs_stat_;
  ops.list = mulib_netfs_list_;
  return ops;
}
int
mulib_netfs_stream_exchange (void *ctx, const void *req, size_t n, void *reply,
                             size_t cap, size_t *count)
{
  mulib_netfs_stream_t *s = ctx;
  unsigned char header[4];
  size_t off = 0, len;
  if (count)
    *count = 0;
  if (!s || s->failed || !req || !reply || !count || n > 16777216 || n < 32
      || cap < 36 || cap > 16777216)
    return 0;
  mulib_endian_store_be32 (header, (uint32_t)n);
  if (mulib_xtks_write_all (&s->io, header, 4, &off))
    goto fail;
  off = 0;
  if (mulib_xtks_write_all (&s->io, req, n, &off))
    goto fail;
  off = 0;
  if (mulib_xtks_read_exact (&s->io, header, 4, &off))
    goto fail;
  len = mulib_endian_load_be32 (header);
  if (len < 36 || len > cap)
    goto fail;
  off = 0;
  if (mulib_xtks_read_exact (&s->io, reply, len, &off))
    goto fail;
  *count = len;
  return 1;
fail:
  s->failed = 1;
  return 0;
}
int
mulib_netfs_stream_serve (mulib_netfs_stream_t *s, mulib_fuse *fuse, void *req,
                          void *reply, size_t cap)
{
  unsigned char header[4];
  size_t off = 0, len, rn;
  if (!s || s->failed || !fuse || !req || !reply || req == reply || cap < 64
      || cap > 16777216)
    return 0;
  if (mulib_xtks_read_exact (&s->io, header, 4, &off))
    goto fail;
  len = mulib_endian_load_be32 (header);
  if (len < 32 || len > cap)
    goto fail;
  off = 0;
  if (mulib_xtks_read_exact (&s->io, req, len, &off))
    goto fail;
  if (!mulib_netfs_serve (fuse, req, len, reply, cap, &rn))
    goto fail;
  mulib_endian_store_be32 (header, (uint32_t)rn);
  off = 0;
  if (mulib_xtks_write_all (&s->io, header, 4, &off))
    goto fail;
  off = 0;
  if (mulib_xtks_write_all (&s->io, reply, rn, &off))
    goto fail;
  return 1;
fail:
  s->failed = 1;
  return 0;
}
#endif
