#define MULIB_IMPL_ROOTFSBUILDER
#define MULIB_IMPL_ROOTFS
#define MULIB_IMPL_VFS
#define MULIB_IMPL_ARCHIVE
#define MULIB_IMPL_BUFFER
#include "mulib-rootfsbuilder.h"
#include <assert.h>
int
main (void)
{
  mulib_rootfs_t root, copy;
  mulib_buffer_t tar = { 0 };
  assert (!mulib_rootfs_init (&root, 16, 4096));
  assert (!mulib_vfs_mem_put (&root.storage, "/file", "x", 1));
  assert (!mulib_rootfsbuilder_export (&root, &tar));
  assert (!mulib_rootfs_import (tar.data, tar.length, 16, 4096, &copy));
  mulib_rootfs_free (&root);
  mulib_rootfs_free (&copy);
  mulib_buffer_free (&tar);
  return 0;
}
