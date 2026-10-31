#include "../container-stack-common.h"
int main (void)
{
  mulib_rootfs_t root;
  mulib_containerd_mount mounts;
  mulib_containerd_mount_lease lease;
  mulib_container_vfs *filesystem;
  assert (!mulib_rootfs_init (&root, 4, 4096));
  assert (!mulib_containerd_mount_init (&mounts, 4));
  assert (!mulib_containerd_mount_add (&mounts, "root", &root, 1, 4, 4096));
  assert (!mulib_containerd_mount_acquire (&mounts, "root", &lease, &filesystem));
  assert (filesystem->read_only);
  assert (mulib_containerd_mount_remove (&mounts, "root") == MULIB_CT_EBUSY);
  assert (!mulib_containerd_mount_release (&mounts, lease));
  assert (!mulib_containerd_mount_remove (&mounts, "root"));
  assert (!mulib_containerd_mount_free (&mounts)); mulib_rootfs_free (&root);
  return 0;
}
