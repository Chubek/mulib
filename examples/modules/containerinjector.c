#define MULIB_IMPL_CONTAINERINJECTOR
#define MULIB_IMPL_ROOTFS
#define MULIB_IMPL_VFS
#define MULIB_IMPL_ARCHIVE
#define MULIB_IMPL_BUFFER
#include "mulib-containerinjector.h"
#include <assert.h>
int
main (void)
{
  mulib_rootfs_t root;
  assert (!mulib_rootfs_init (&root, 16, 4096));
  assert (!mulib_containerinjector_put (&root, "/file", "data", 4));
  mulib_rootfs_free (&root);
  return 0;
}
