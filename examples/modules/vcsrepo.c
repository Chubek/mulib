#define MULIB_IMPL_GIT
#define MULIB_IMPL_VCSREPO
#include "mulib-vcsrepo.h"
#include <assert.h>
int
main (void)
{
  mulib_vcsrepo_t repo;
  mulib_git_oid_t id, readback;
  assert (mulib_vcsrepo_init (&repo, 8, 4, 1024));
  assert (mulib_vcsrepo_put (&repo, "blob", "hello", 5, &id) == 1);
  assert (mulib_vcsrepo_ref_update (&repo, "refs/heads/main", NULL, &id) == 1);
  assert (mulib_vcsrepo_ref_get (&repo, "refs/heads/main", &readback));
  assert (mulib_vcsrepo_get (&repo, &readback)->length == 5);
  mulib_vcsrepo_free (&repo);
  return 0;
}
