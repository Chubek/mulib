#define MULIB_IMPL_ROOTFS
#define MULIB_IMPL_VFS
#define MULIB_IMPL_ARCHIVE
#define MULIB_IMPL_BUFFER
#include "mulib-rootfs.h"
#include <assert.h>
int
main (void)
{
  mulib_rootfs_t root;
  assert (!mulib_rootfs_init (&root, 16, 4096));
  assert (!mulib_vfs_mem_mkdir (&root.storage, "/etc"));
  assert (!mulib_vfs_mem_put (&root.storage, "/etc/config", "data", 4));
  mulib_rootfs_free (&root);
  return 0;
}
