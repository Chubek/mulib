#define MULIB_IMPL_GIT
#include "mulib-git.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_git_oid_t id;
  char hex[41];
  unsigned char wire[32];
  size_t used, length;
  const unsigned char *payload;
  unsigned kind;
  assert (mulib_git_object_id ("blob", "hello\n", 6, &id));
  assert (mulib_git_oid_format (&id, hex));
  assert (!strcmp (hex, "ce013625030ba8dba906f756967f9e9ca394464a"));
  assert (mulib_git_pkt_encode (0, "hello\n", 6, wire, sizeof wire, &used));
  assert (mulib_git_pkt_decode (wire, used, &payload, &length, &kind, &used)
          == 1);
  assert (!kind && length == 6 && !memcmp (payload, "hello\n", 6));
  return 0;
}
