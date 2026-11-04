#include "../exec-common.h"
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
  {
    const char *args[] = {"--version"};
    mulib_exec_options_t options = {0};
    mulib_exec_result_t result = {0};
    int rc;
    options.capture_stdout = options.capture_stderr = 1;
    rc = mulib_git_run (NULL, NULL, args, 1, &options, &result);
    /* Git is an optional runtime executable, not a build dependency. */
    if (!rc) assert (result.code == 0 && result.out_size > 0);
    mulib_exec_result_destroy (&result);
  }
  return 0;
}
