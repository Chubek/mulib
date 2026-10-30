#define MULIB_IMPL_VFS
#define MULIB_IMPL_FUSE
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_XTKS
#define MULIB_IMPL_NETFS
#include "mulib-netfs.h"
#include <assert.h>
#include <string.h>
static int
exchange (void *ctx, const void *request, size_t n, void *reply, size_t cap,
          size_t *used)
{
  return mulib_netfs_serve (ctx, request, n, reply, cap, used);
}
int
main (void)
{
  mulib_vfs server, client;
  mulib_vfs_mem storage;
  mulib_fuse fuse;
  mulib_netfs_t net;
  mulib_vfs_ops memory_ops = mulib_vfs_mem_ops (),
                remote_ops = mulib_netfs_ops ();
  char bytes[16];
  size_t n;
  assert (mulib_vfs_init (&server, 1) == MULIB_VFS_OK);
  assert (mulib_vfs_init (&client, 1) == MULIB_VFS_OK);
  assert (mulib_vfs_mem_init (&storage, 8, 4096) == MULIB_VFS_OK);
  assert (mulib_vfs_mount (&server, "/", &memory_ops, &storage)
          == MULIB_VFS_OK);
  assert (mulib_fuse_init (&fuse, &server, 0) == MULIB_VFS_OK);
  assert (mulib_netfs_init (&net, exchange, &fuse, 8192));
  assert (mulib_vfs_mount (&client, "/remote", &remote_ops, &net)
          == MULIB_VFS_OK);
  assert (mulib_vfs_write (&client, "/remote/hello", 0, "network", 7, &n)
              == MULIB_VFS_OK
          && n == 7);
  assert (mulib_vfs_read (&client, "/remote/hello", 0, bytes, sizeof bytes, &n)
          == MULIB_VFS_OK);
  assert (n == 7 && !memcmp (bytes, "network", 7));
  mulib_vfs_free (&client);
  mulib_vfs_free (&server);
  mulib_vfs_mem_free (&storage);
  return 0;
}
