#include "../container-native-common.h"
int main (void)
{
  mulib_rootfs_t root;
  mulib_oix_config_t config;
  mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
  mulib_oix_info_t info;
  assert (!mulib_rootfs_init (&root, 8, 4096));
  assert (!mulib_rootfs_put (&root, "/message", "hello OIX", 9, 0));
  mulib_oix_config_init (&config);
  assert (!mulib_container_oixboost_compile (&root, &config, &image, NULL));
  assert (mulib_oix_validate (image.data, image.length, &info, NULL) == MULIB_OIX_OK);
  assert (info.files == 1);
  mulib_oix_image_free (&image); mulib_rootfs_free (&root);
  return 0;
}
