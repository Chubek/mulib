#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_SSHSERVER
#include "mulib-sshserver.h"
#include <assert.h>
#include <string.h>
static int
verify (void *ctx, const mulib_sshserver_auth_t *auth)
{
  (void)ctx;
  return auth->user_length == 1 && auth->user[0] == 'u'
         && auth->password_length == 1 && auth->password[0] == 'p';
}
int
main (void)
{
  /* A payload from an already authenticated/encrypted SSH transport. */
  const unsigned char payload[] = {
    50,  0,   0,   0,   1,   'u', 0,   0,   0,   14,  's', 's', 'h', '-',
    'c', 'o', 'n', 'n', 'e', 'c', 't', 'i', 'o', 'n', 0,   0,   0,   8,
    'p', 'a', 's', 's', 'w', 'o', 'r', 'd', 0,   0,   0,   0,   1,   'p'
  };
  mulib_sshserver_t server;
  unsigned char reply[14];
  size_t used;
  assert (mulib_sshserver_init (&server, 3));
  assert (mulib_sshserver_authenticate (&server, payload, sizeof payload,
                                        verify, NULL)
          == 1);
  assert (mulib_sshserver_auth_reply (1, reply, sizeof reply, &used)
          && used == 1 && reply[0] == 52);
  return 0;
}
