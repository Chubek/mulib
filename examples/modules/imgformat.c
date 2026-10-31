#include "../container-stack-common.h"
int main (void)
{
  mulib_ct_blob image = { 0 };
  mulib_imgformat_view view;
  mulib_container_spec process;
  assert (!example_container_image (&image));
  assert (!mulib_imgformat_open (image.data, image.length, 65536, &view));
  assert (!mulib_imgformat_spec_decode (view.config, view.config_length, &process));
  assert (!strcmp (process.id, "demo") && process.argc == 1);
  mulib_container_spec_free (&process); mulib_ct_blob_free (&image);
  return 0;
}
