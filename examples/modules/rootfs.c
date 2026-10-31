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
  mulib_rootfs_t snapshot;
  char bytes[8];
  size_t count;
  assert (!mulib_rootfs_init (&root, 16, 4096));
  assert (!mulib_vfs_mem_mkdir (&root.storage, "/etc"));
  assert (!mulib_vfs_mem_put (&root.storage, "/etc/config", "data", 4));
  assert (!mulib_rootfs_clone (&root, &snapshot));
  assert (!mulib_rootfs_put (&root, "/var/lib/state", "ready", 5, 1));
  assert (!mulib_rootfs_read (&snapshot, "/etc/config", 0, bytes, sizeof (bytes), &count));
  assert (count == 4);
  assert (!mulib_rootfs_remove (&root, "/var", 1));
  mulib_rootfs_free (&snapshot);
  mulib_rootfs_free (&root);
  return 0;
}
