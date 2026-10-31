#include "../container-stack-common.h"
#include <stdio.h>
int main (void)
{
  mulib_imgbuildspec spec;
  mulib_imgbuilder_report report;
  mulib_ct_blob image = { 0 };
  assert (!mulib_imgbuildspec_init (&spec, "hello"));
  assert (!mulib_imgbuildspec_file (&spec, "/hello.txt", "hello world\n", 12));
  assert (!mulib_imgbuilder_build (&spec, NULL, &image, &report));
  assert (report.files == 1 && report.file_bytes == 12);
  printf ("built %zu-byte image with %zu files\n", image.length, report.files);
  mulib_ct_blob_free (&image); mulib_imgbuildspec_free (&spec);
  return 0;
}
