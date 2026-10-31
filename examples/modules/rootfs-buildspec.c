#include "../container-stack-common.h"
int main (void)
{
  mulib_rootfs_t root;
  mulib_rootfs_buildspec recipe;
  mulib_vfs_stat info;
  assert (!mulib_rootfs_init (&root, 16, 4096));
  assert (!mulib_rootfs_buildspec_init (&recipe, 8, 8192, 8192));
  assert (!mulib_rootfs_buildspec_add (&recipe, MULIB_ROOTFS_BUILD_FILE,
      "/etc/config", NULL, "enabled", 7, 0));
  assert (!mulib_rootfs_buildspec_add (&recipe, MULIB_ROOTFS_BUILD_COPY,
      "/config.backup", "/etc/config", NULL, 0, 0));
  assert (!mulib_rootfs_buildspec_apply (&recipe, &root, NULL, NULL));
  assert (!mulib_rootfs_stat (&root, "/config.backup", &info) && info.size == 7);
  mulib_rootfs_buildspec_free (&recipe); mulib_rootfs_free (&root);
  return 0;
}
