#define MULIB_IMPL_GIT
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_VCSREPO
#define MULIB_IMPL_VCSCOMMIT
#include "mulib-vcscommit.h"
#include <assert.h>
int
main (void)
{
  mulib_vcsrepo_t repo;
  mulib_vcscommit_t commit = { 0 };
  mulib_git_oid_t id;
  assert (mulib_vcsrepo_init (&repo, 4, 2, 4096));
  assert (mulib_vcsrepo_put (&repo, "tree", NULL, 0, &commit.tree) == 1);
  commit.author.name = "Example";
  commit.author.email = "example@example.test";
  commit.committer = commit.author;
  commit.message = "Initial commit\n";
  commit.message_length = 15;
  assert (mulib_vcscommit_store (&repo, &commit, &id));
  assert (mulib_vcsrepo_ref_update (&repo, "refs/heads/main", NULL, &id) == 1);
  mulib_vcsrepo_free (&repo);
  return 0;
}
