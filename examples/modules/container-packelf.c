#include "../container-stack-common.h"
#include <stdio.h>
int main (int argc, char **argv)
{
  mulib_ct_blob image = { 0 }, object = { 0 };
  const unsigned char *embedded;
  size_t length;
  mulib_imgformat_view view;
  assert (!example_container_image (&image));
  assert (!mulib_container_packelf_object (image.data, image.length, 62, &object, 65536));
  assert (!mulib_container_packelf_open (object.data, object.length, 65536, &embedded, &length, &view));
  assert (length == image.length && !memcmp (embedded, image.data, length));
  if (argc == 2)
    {
      FILE *file = fopen (argv[1], "wb");
      assert (file && fwrite (object.data, 1, object.length, file) == object.length);
      assert (!fclose (file));
    }
  mulib_ct_blob_free (&image); mulib_ct_blob_free (&object);
  return 0;
}
