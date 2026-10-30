#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_SSHCLIENT
#include "mulib-sshclient.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  unsigned char wire[256];
  char software[256];
  size_t n, used;
  assert (
      mulib_sshclient_identification ("MuLIB_example", wire, sizeof wire, &n));
  assert (mulib_sshclient_identify (wire, n, software, sizeof software, &used)
          == 1);
  assert (n == used && !strcmp (software, "MuLIB_example"));
  return 0;
}
