#include "../container-stack-common.h"
int main (void)
{
  mulib_rootfs_t root;
  mulib_container_vfs filesystem;
  char output[4];
  size_t count;
  assert (!mulib_rootfs_init (&root, 4, 128));
  assert (!mulib_rootfs_put (&root, "/file", "data", 4, 0));
  assert (!mulib_container_vfs_init (&filesystem, &root, 4, 1));
  assert (!mulib_vfs_read (&filesystem.router, "/file", 0, output, 4, &count) && count == 4);
  assert (mulib_vfs_write (&filesystem.router, "/file", 0, "x", 1, &count) == MULIB_VFS_EROFS);
  mulib_container_vfs_free (&filesystem); mulib_rootfs_free (&root);
  return 0;
}
