#include "../binary-formats-common.h"
int main (int argc, char **argv)
{
  mulib_ct_blob image = { 0 }, object = { 0 }; mulib_imgformat_view v;
  const unsigned char *data; size_t n;
  example_binary_image (&image);
  assert (!mulib_container_packpe_object (image.data, image.length, 0x8664, &object, 65536));
  assert (!mulib_container_packpe_open (object.data, object.length, 65536, &data, &n, &v));
  assert (n == image.length && !memcmp (data, image.data, n));
  example_binary_write (argc, argv, object.data, object.length);
  mulib_ct_blob_free (&image); mulib_ct_blob_free (&object); return 0;
}
