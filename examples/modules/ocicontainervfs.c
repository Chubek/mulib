#define MULIB_IMPL_OCICONTAINERVFS
#define MULIB_IMPL_ROOTFS
#define MULIB_IMPL_VFS
#define MULIB_IMPL_ARCHIVE
#define MULIB_IMPL_BUFFER
#include "mulib-ocicontainervfs.h"
#include <assert.h>
int
main (void)
{
  mulib_rootfs_t root;
  mulib_vfs v;
  size_t n;
  char data[4];
  assert (!mulib_rootfs_init (&root, 16, 4096));
  assert (!mulib_vfs_mem_put (&root.storage, "/file", "abc", 3));
  assert (!mulib_vfs_init (&v, 1));
  assert (!mulib_ocicontainervfs_mount (&v, "/", &root, 1));
  assert (!mulib_vfs_read (&v, "/file", 0, data, 4, &n) && n == 3);
  mulib_vfs_free (&v);
  mulib_rootfs_free (&root);
  return 0;
}
