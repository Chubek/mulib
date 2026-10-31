#include "../container-stack-common.h"
#include <stdio.h>
int main (int argc, char **argv)
{
  mulib_ct_blob image = { 0 }, packed = { 0 }, restored = { 0 };
  assert (!example_container_image (&image));
  assert (!mulib_container_packzstd_pack (image.data, image.length, 65536, 65536, &packed, NULL));
  assert (!mulib_container_packzstd_unpack (packed.data, packed.length, 65536, 65536, &restored, NULL));
  assert (image.length == restored.length && !memcmp (image.data, restored.data, image.length));
  if (argc == 3)
    {
      FILE *file = fopen (argv[1], "wb");
      assert (file && fwrite (packed.data, 1, packed.length, file) == packed.length);
      assert (!fclose (file)); file = fopen (argv[2], "wb");
      assert (file && fwrite (image.data, 1, image.length, file) == image.length);
      assert (!fclose (file));
    }
  mulib_ct_blob_free (&image); mulib_ct_blob_free (&packed); mulib_ct_blob_free (&restored);
  return 0;
}
