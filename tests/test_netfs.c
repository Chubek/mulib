#define MULIB_IMPL_VFS
#define MULIB_IMPL_FUSE
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_XTKS
#define MULIB_IMPL_NETFS
#define MULIB_IMPL_SOCKET
#include "mulib-netfs.h"
#include "mulib-socket.h"
#include <assert.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#endif
typedef struct fixture
{
  mulib_vfs local, remote;
  mulib_vfs_mem mem;
  mulib_fuse fuse;
  mulib_netfs_t client;
  int corrupt, exchanges;
} fixture;
static int
exchange (void *ctx, const void *p, size_t n, void *out, size_t cap,
          size_t *used)
{
  fixture *f = ctx;
  int result;
  ++f->exchanges;
  result = mulib_netfs_serve (&f->fuse, p, n, out, cap, used);
  if (result && f->corrupt == 1)
    ((unsigned char *)out)[15] ^= 1;
  if (result && f->corrupt == 2)
    mulib_endian_store_be32 ((unsigned char *)out + 32, UINT32_MAX);
  if (result && f->corrupt == 3 && *used > 52)
    ((unsigned char *)out)[52] = '/';
  return result;
}
static void
setup (fixture *f)
{
  mulib_vfs_ops memory = mulib_vfs_mem_ops (), remote = mulib_netfs_ops ();
  memset (f, 0, sizeof *f);
  assert (mulib_vfs_init (&f->local, 1) == MULIB_VFS_OK);
  assert (mulib_vfs_init (&f->remote, 1) == MULIB_VFS_OK);
  assert (mulib_vfs_mem_init (&f->mem, 32, 8192) == MULIB_VFS_OK);
  assert (mulib_vfs_mount (&f->local, "/", &memory, &f->mem) == MULIB_VFS_OK);
  assert (mulib_fuse_init (&f->fuse, &f->local, 0) == MULIB_VFS_OK);
  assert (mulib_netfs_init (&f->client, exchange, f, 4096));
  assert (mulib_vfs_mount (&f->remote, "/remote", &remote, &f->client)
          == MULIB_VFS_OK);
}
static void
teardown (fixture *f)
{
  mulib_vfs_free (&f->remote);
  mulib_vfs_free (&f->local);
  mulib_vfs_mem_free (&f->mem);
}
static int
listing (void *ctx, const char *name, const mulib_vfs_stat *info)
{
  assert (name && *name && info->kind == MULIB_VFS_FILE);
  ++*(int *)ctx;
  return 0;
}
static int
stop_listing (void *ctx, const char *name, const mulib_vfs_stat *info)
{
  listing (ctx, name, info);
  return 1;
}
static void
direct (void)
{
  fixture f;
  char out[64];
  size_t n;
  mulib_vfs_stat stat;
  int entries = 0;
  setup (&f);
  {
    mulib_vfs_ops ops = mulib_netfs_ops ();
    assert (ops.read (&f.client, "/a", 0, out, sizeof out, NULL)
            == MULIB_VFS_EINVAL);
    assert (ops.write (&f.client, "/a", 0, "x", 1, NULL)
            == MULIB_VFS_EINVAL);
    assert (ops.stat (&f.client, "/", NULL) == MULIB_VFS_EINVAL);
    assert (ops.list (&f.client, "/", NULL, NULL) == MULIB_VFS_EINVAL);
    assert (!f.exchanges && f.client.next_id == 1 && !f.client.busy);
  }
  assert (mulib_vfs_write (&f.remote, "/remote/a", 0, "a\0bc", 4, &n)
              == MULIB_VFS_OK
          && n == 4);
  assert (mulib_vfs_write (&f.remote, "/remote/a", 2, "X", 1, &n)
          == MULIB_VFS_OK);
  assert (mulib_vfs_read (&f.remote, "/remote/a", 1, out, sizeof out, &n)
              == MULIB_VFS_OK
          && n == 3 && !memcmp (out, "\0Xc", 3));
  assert (mulib_vfs_getstat (&f.remote, "/remote/a", &stat) == MULIB_VFS_OK
          && stat.size == 4 && stat.kind == MULIB_VFS_FILE);
  assert (mulib_vfs_read (&f.remote, "/remote/a", 10, out, sizeof out, &n)
              == MULIB_VFS_OK
          && n == 0);
  assert (mulib_vfs_list (&f.remote, "/remote", listing, &entries)
              == MULIB_VFS_OK
          && entries == 1);
  entries = 0;
  assert (mulib_vfs_list (&f.remote, "/remote", stop_listing, &entries)
              == MULIB_VFS_ECANCELED
          && entries == 1);
  assert (mulib_vfs_read (&f.remote, "/remote/missing", 0, out, sizeof out, &n)
              == MULIB_VFS_ENOENT
          && n == 0);
  f.fuse.read_only = 1;
  assert (mulib_vfs_write (&f.remote, "/remote/a", 0, "bad", 3, &n)
              == MULIB_VFS_EROFS
          && n == 0);
  f.fuse.read_only = 0;
  for (int i = 1; i <= 2; ++i)
    {
      f.corrupt = i;
      memset (out, 0x55, sizeof out);
      assert (mulib_vfs_read (&f.remote, "/remote/a", 0, out, sizeof out, &n)
                  == MULIB_VFS_EIO
              && n == 0);
      for (size_t k = 0; k < sizeof out; ++k)
        assert (out[k] == 0x55);
    }
  entries = 0;
  f.corrupt = 3;
  assert (mulib_vfs_list (&f.remote, "/remote", listing, &entries)
              == MULIB_VFS_EIO
          && entries == 0);
  f.corrupt = 0;
  f.client.max_frame = 64;
  assert (mulib_vfs_mem_put (&f.mem, "/another", NULL, 0) == MULIB_VFS_OK);
  assert (mulib_vfs_list (&f.remote, "/remote", listing, &entries)
              == MULIB_VFS_ELIMIT
          && entries == 0);
  f.client.max_frame = 4096;
  f.client.next_id = UINT64_MAX;
  assert (mulib_vfs_getstat (&f.remote, "/remote/a", &stat) == MULIB_VFS_OK);
  assert (mulib_vfs_getstat (&f.remote, "/remote/a", &stat)
          == MULIB_VFS_EINVAL);
  /* Malformed paths and lengths are rejected before FUSE dispatch. */
  {
    unsigned char request[64] = { 0 }, reply[64];
    size_t used;
    memcpy (request, "MNF1", 4);
    request[4] = MULIB_FUSE_WRITE;
    mulib_endian_store_be32 (request + 28, 5);
    memcpy (request + 32, "/../x", 5);
    assert (
        !mulib_netfs_serve (&f.fuse, request, 37, reply, sizeof reply, &used)
        && used == 0);
    memcpy (request + 32, "/file", 5);
    mulib_endian_store_be32 (request + 24, 1);
    assert (
        !mulib_netfs_serve (&f.fuse, request, 37, reply, sizeof reply, &used));
    assert (mulib_vfs_getstat (&f.local, "/file", &stat) == MULIB_VFS_ENOENT);
  }
  teardown (&f);
}
#if defined(__unix__) || defined(__APPLE__)
static int
socket_read (void *ctx, void *p, size_t n, size_t *count)
{
  return mulib_socket_receive (ctx, p, n > 7 ? 7 : n, count);
}
static int
socket_write (void *ctx, const void *p, size_t n, size_t *count)
{
  return mulib_socket_send (ctx, p, n > 5 ? 5 : n, count);
}
typedef struct server_thread
{
  mulib_netfs_stream_t stream;
  mulib_fuse *fuse;
} server_thread;
static void *
serve_thread (void *ctx)
{
  server_thread *s = ctx;
  unsigned char request[4096], reply[4096];
  for (unsigned i = 0; i < 6; ++i)
    assert (mulib_netfs_stream_serve (&s->stream, s->fuse, request, reply,
                                      sizeof request));
  return NULL;
}
static void
socket_transport (void)
{
  fixture f;
  mulib_socket_t a = MULIB_SOCKET_INIT, b = MULIB_SOCKET_INIT;
  pthread_t thread;
  mulib_netfs_stream_t client;
  server_thread server;
  char bytes[16];
  size_t n;
  int entries = 0;
  mulib_vfs_stat stat;
  setup (&f);
  assert (!mulib_socket_pair (&a, &b));
  client.io.context = &a;
  client.io.read = socket_read;
  client.io.write = socket_write;
  client.failed = 0;
  server.stream.io.context = &b;
  server.stream.io.read = socket_read;
  server.stream.io.write = socket_write;
  server.stream.failed = 0;
  server.fuse = &f.fuse;
  assert (mulib_netfs_init (&f.client, mulib_netfs_stream_exchange, &client,
                            4096));
  assert (!pthread_create (&thread, NULL, serve_thread, &server));
  assert (mulib_vfs_getstat (&f.remote, "/remote", &stat) == MULIB_VFS_OK
          && stat.kind == MULIB_VFS_DIRECTORY);
  assert (
      mulib_vfs_write (&f.remote, "/remote/file", 0, "over the wire", 13, &n)
          == MULIB_VFS_OK
      && n == 13);
  assert (
      mulib_vfs_read (&f.remote, "/remote/file", 0, bytes, sizeof bytes, &n)
          == MULIB_VFS_OK
      && n == 13 && !memcmp (bytes, "over the wire", 13));
  assert (mulib_vfs_getstat (&f.remote, "/remote/file", &stat) == MULIB_VFS_OK
          && stat.size == 13);
  assert (mulib_vfs_list (&f.remote, "/remote", listing, &entries)
              == MULIB_VFS_OK
          && entries == 1);
  assert (
      mulib_vfs_read (&f.remote, "/remote/missing", 0, bytes, sizeof bytes, &n)
      == MULIB_VFS_ENOENT);
  assert (!pthread_join (thread, NULL));
  assert (!mulib_socket_close (&b));
  assert (mulib_vfs_getstat (&f.remote, "/remote/file", &stat) == MULIB_VFS_EIO
          && client.failed);
  assert (!mulib_socket_close (&a));
  teardown (&f);
}
#endif
int
main (void)
{
  direct ();
#if defined(__unix__) || defined(__APPLE__)
  socket_transport ();
#endif
  return 0;
}
