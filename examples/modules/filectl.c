#define MULIB_IMPL_FILECTL
#include "mulib-filectl.h"
#include <assert.h>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif
int
main (void)
{
#if defined(__unix__) || defined(__APPLE__)
  int p[2];
  assert (!pipe (p));
  assert (!mulib_filectl_nonblocking (p[0], 1));
  assert (fcntl (p[0], F_GETFL) & O_NONBLOCK);
  assert (!mulib_filectl_cloexec (p[0], 1));
  assert (fcntl (p[0], F_GETFD) & FD_CLOEXEC);
  close (p[0]);
  close (p[1]);
#endif
  return 0;
}
