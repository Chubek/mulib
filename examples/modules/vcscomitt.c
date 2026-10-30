#define MULIB_IMPL_GIT
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_VCSREPO
#define MULIB_IMPL_VCSCOMMIT
#define MULIB_IMPL_VCSCOMITT
#include "mulib-vcscomitt.h"
#include <assert.h>
#include <stdlib.h>
int
main (void)
{
  mulib_vcscomitt_t commit = { 0 };
  char *bytes;
  size_t n;
  commit.author.name = "Example";
  commit.author.email = "example@example.test";
  commit.committer = commit.author;
  assert (mulib_vcscomitt_encode (&commit, &bytes, &n));
  assert (n > 40);
  free (bytes);
  return 0;
}
