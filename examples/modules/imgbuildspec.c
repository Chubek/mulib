#include "../container-stack-common.h"
int main (void)
{
  mulib_imgbuildspec spec, copy;
  assert (!mulib_imgbuildspec_init (&spec, "service"));
  assert (!mulib_container_spec_arg (&spec.process, "/init"));
  assert (!mulib_imgbuildspec_directory (&spec, "/etc"));
  assert (!mulib_imgbuildspec_file (&spec, "/etc/message", "ready", 5));
  assert (!mulib_imgbuildspec_validate (&spec));
  assert (!mulib_imgbuildspec_clone (&spec, &copy));
  mulib_imgbuildspec_free (&spec);
  assert (copy.filesystem.count == 2 && copy.process.argc == 1);
  mulib_imgbuildspec_free (&copy);
  return 0;
}
