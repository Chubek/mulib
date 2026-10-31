#include "../container-stack-common.h"
int main (void)
{
  mulib_ct_blob bytes = { 0 };
  mulib_image2container image, clone;
  char message[8];
  size_t length;
  assert (!example_container_image (&bytes));
  assert (!mulib_image2container_open (bytes.data, bytes.length, "first", NULL, &image, NULL));
  mulib_ct_blob_free (&bytes);
  assert (!mulib_image2container_clone (&image, "second", &clone));
  mulib_image2container_free (&image);
  assert (!mulib_rootfs_read (&clone.rootfs, "/etc/message", 0, message, sizeof (message), &length));
  assert (length == 6 && !memcmp (message, "hello\n", 6));
  mulib_image2container_free (&clone);
  return 0;
}
