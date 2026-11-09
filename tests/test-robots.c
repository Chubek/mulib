#define MULIB_IMPL_ROBOTS
#include "mulib-robots.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  assert (!mulib_robots_allowed ("User-agent: *\nDisallow: /private\n",
                                 "MuWeb", "/private/a"));
  assert (mulib_robots_allowed ("User-agent: *\nDisallow: /private\n", "MuWeb",
                                "/public"));
  return 0;
}
