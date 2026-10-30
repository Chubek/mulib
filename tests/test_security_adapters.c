#define MULIB_IMPL_SOCKET
#define MULIB_IMPL_TSL
#define MULIB_IMPL_EBPF
#define MULIB_IMPL_INITPROC
#define MULIB_IMPL_KVM
#define MULIB_IMPL_KVMCONTAINER
#include "mulib-ebpf.h"
#include "mulib-initproc.h"
#include "mulib-kvmcontainer.h"
#include "mulib-socket.h"
#include "mulib-tsl.h"
#include <assert.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif
typedef struct fake_tls
{
  int auth, closed, again, oversized, stalled;
} fake_tls;
static int
handshake (void *ctx, int *verified)
{
  fake_tls *b = (fake_tls *)ctx;
  if (b->again)
    {
      b->again = 0;
      return 1;
    }
  *verified = b->auth;
  return 0;
}
static int
read_tls (void *ctx, void *out, size_t n, size_t *count)
{
  fake_tls *b = (fake_tls *)ctx;
  if (b->stalled)
    return 0;
  if (b->oversized)
    {
      *count = n + 1;
      return 0;
    }
  if (n)
    {
      *(char *)out = 'x';
      *count = 1;
    }
  return 0;
}
static int
write_tls (void *ctx, const void *out, size_t n, size_t *count)
{
  if (((fake_tls *)ctx)->stalled)
    return 0;
  (void)out;
  *count = n;
  return 0;
}
static void
close_tls (void *ctx)
{
  ++((fake_tls *)ctx)->closed;
}
int
main (void)
{
  mulib_tsl_t tls = { 0 };
  fake_tls fake = { 0 };
  mulib_tsl_ops_t ops = { handshake, read_tls, write_tls, close_tls };
  char data[8];
  size_t count;
  uint64_t value = 77;
  mulib_ebpf_instruction_t code[]
      = { { 0xb7, 0, 0, 0, 40 }, { 0x07, 0, 0, 0, 2 }, { 0x95, 0, 0, 0, 0 } };
  mulib_kvmcontainer_t vm;
  assert (mulib_tsl_init (&tls, &ops, &fake));
  assert (mulib_tsl_read (&tls, data, 8, &count) < 0);
  assert (mulib_tsl_handshake (&tls) == MULIB_TSL_EAUTH);
  fake.auth = 1;
  assert (mulib_tsl_handshake (&tls) == MULIB_TSL_EAUTH);
  mulib_tsl_close (&tls);
  assert (fake.closed == 1);
  mulib_tsl_close (&tls);
  assert (fake.closed == 1);
  fake.again = 1;
  assert (mulib_tsl_init (&tls, &ops, &fake));
  assert (mulib_tsl_handshake (&tls) == 1);
  assert (!mulib_tsl_handshake (&tls)
          && !mulib_tsl_read (&tls, data, 8, &count) && count == 1
          && data[0] == 'x');
  assert (!mulib_tsl_write (&tls, "abc", 3, &count) && count == 3);
  fake.oversized = 1;
  assert (mulib_tsl_read (&tls, data, 8, &count) == MULIB_TSL_EBACKEND
          && !count);
  mulib_tsl_close (&tls);
  fake.oversized = 0;
  fake.stalled = 1;
  for (int write = 0; write < 2; ++write)
    {
      assert (mulib_tsl_init (&tls, &ops, &fake));
      assert (!mulib_tsl_handshake (&tls));
      assert ((write ? mulib_tsl_write (&tls, "x", 1, &count)
                     : mulib_tsl_read (&tls, data, 1, &count))
                  == MULIB_TSL_EBACKEND
              && !count);
      assert (mulib_tsl_handshake (&tls) == MULIB_TSL_EAUTH);
      mulib_tsl_close (&tls);
    }
  assert (mulib_ebpf_validate (code, 3, NULL));
  assert (!mulib_ebpf_run (code, 3, 3, &value) && value == 42);
  assert (mulib_ebpf_run (code, 3, 2, &value) == -2 && value == 42);
  code[0].dst = 10;
  assert (!mulib_ebpf_validate (code, 3, NULL));
  code[0].dst = 0;
  code[1].opcode = 0x05;
  code[1].offset = -1;
  assert (!mulib_ebpf_validate (code, 3, NULL));
  mulib_kvmcontainer_init (&vm);
  assert (!mulib_kvmcontainer_close (&vm));
  assert (mulib_kvmcontainer_open (&vm, "/mulib/no-such-kvm-device") < 0
          && vm.device.fd == -1);
#if defined(__unix__) || defined(__APPLE__)
  {
    mulib_socket_t a = MULIB_SOCKET_INIT, b = MULIB_SOCKET_INIT;
    int pid, exit_code;
    pid_t child;
    assert (!mulib_socket_pair (&a, &b));
    assert (!mulib_socket_nonblocking (&b, 1));
    assert (mulib_socket_receive (&b, data, sizeof (data), &count)
                == MULIB_SOCKET_AGAIN
            && !count);
    assert (!mulib_socket_send (&a, "abc", 3, &count) && count == 3);
    assert (!mulib_socket_receive (&b, data, sizeof (data), &count)
            && count == 3 && !memcmp (data, "abc", 3));
    assert (!mulib_socket_close (&a));
    assert (mulib_socket_receive (&b, data, sizeof (data), &count)
            == MULIB_SOCKET_EOF);
    assert (!mulib_socket_close (&b) && !mulib_socket_close (&b));
    child = fork ();
    assert (child >= 0);
    if (!child)
      _exit (23);
    assert (mulib_initproc_reap ((int)child, 0, &pid, &exit_code) == 1
            && pid == (int)child && exit_code == 23);
    assert (mulib_initproc_reap ((int)child, 1, &pid, &exit_code) == 2);
  }
#endif
  return 0;
}
