#define MULIB_IMPL_FILECTL
#define MULIB_IMPL_DATAGRAM
#define MULIB_IMPL_SHM
#define MULIB_IMPL_MQUEUE
#define MULIB_IMPL_NETDEVICE
#define MULIB_IMPL_ABSTUN
#define MULIB_IMPL_TUNTAP
#define MULIB_IMPL_SCHEDUTILS
#include "mulib-datagram.h"
#include "mulib-mqueue.h"
#include "mulib-netdevice.h"
#include "mulib-schedutils.h"
#include "mulib-shm.h"
#include "mulib-tuntap.h"
#include <assert.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
static void
wait_ok (pid_t pid)
{
  int status;
  assert (pid > 0 && waitpid (pid, &status, 0) == pid);
  assert (WIFEXITED (status) && WEXITSTATUS (status) == 0);
}
static void
memory (void)
{
  mulib_shm_t s = MULIB_SHM_INIT, ro = MULIB_SHM_INIT;
  pid_t child;
  assert (mulib_shm_create (&s, 0) == -1 && s.id == -1);
  assert (!mulib_shm_create (&s, 4096));
  assert (mulib_shm_create (&s, 4096) == -1);
  assert (!mulib_shm_attach (&ro, s.id, 1) && ro.size == 4096);
  memcpy (s.data, "parent", 7);
  child = fork ();
  assert (child >= 0);
  if (!child)
    {
      assert (!strcmp (ro.data, "parent"));
      memcpy (s.data, "child", 6);
      _exit (0);
    }
  wait_ok (child);
  assert (!strcmp (ro.data, "child"));
  assert (!mulib_shm_remove (s.id));
  assert (!mulib_shm_close (&s));
  assert (!strcmp (ro.data, "child"));
  assert (!mulib_shm_close (&ro));
  assert (!mulib_shm_close (&ro));
}
static void
queues (void)
{
  int id;
  long type;
  char bytes[16];
  size_t n;
  pid_t child;
  assert (!mulib_mqueue_create (&id));
  assert (mulib_mqueue_receive (id, 0, &type, bytes, sizeof bytes, &n)
              == MULIB_MQUEUE_AGAIN
          && !n);
  assert (!mulib_mqueue_send (id, 2, "parent", 6));
  assert (mulib_mqueue_receive (id, 0, &type, bytes, 2, &n)
              == MULIB_MQUEUE_TOOSMALL
          && !n);
  child = fork ();
  assert (child >= 0);
  if (!child)
    {
      assert (!mulib_mqueue_receive (id, 2, &type, bytes, sizeof bytes, &n)
              && type == 2 && n == 6);
      assert (!memcmp (bytes, "parent", 6));
      assert (!mulib_mqueue_send (id, 3, "child", 5));
      _exit (0);
    }
  wait_ok (child);
  assert (!mulib_mqueue_receive (id, -3, &type, bytes, sizeof bytes, &n));
  assert (type == 3 && n == 5 && !memcmp (bytes, "child", 5));
  assert (!mulib_mqueue_send (id, 1, NULL, 0));
  assert (!mulib_mqueue_receive (id, 0, &type, NULL, 0, &n) && n == 0);
  assert (mulib_mqueue_send (id, 0, NULL, 0) == -1);
  assert (!mulib_mqueue_remove (id));
  assert (mulib_mqueue_send (id, 1, "x", 1) == MULIB_MQUEUE_EIO);
}
static void
descriptors (void)
{
  int fd = open ("filectl-test", O_RDWR | O_CREAT | O_TRUNC, 0600), pipes[2];
  pid_t child;
  assert (fd >= 0);
  assert (!mulib_filectl_cloexec (fd, 1));
  assert (fcntl (fd, F_GETFD) & FD_CLOEXEC);
  assert (!mulib_filectl_lock (fd, 2));
  child = fork ();
  assert (child >= 0);
  if (!child)
    {
      assert (mulib_filectl_lock (fd, 2) == MULIB_FILECTL_BUSY);
      _exit (0);
    }
  wait_ok (child);
  assert (!mulib_filectl_lock (fd, 0));
  assert (!mulib_filectl_lock (fd, 1));
  assert (!mulib_filectl_lock (fd, 0));
  close (fd);
  unlink ("filectl-test");
  assert (!pipe (pipes));
  assert (!mulib_filectl_nonblocking (pipes[0], 1));
  assert (fcntl (pipes[0], F_GETFL) & O_NONBLOCK);
  assert (!mulib_filectl_nonblocking (pipes[0], 0));
  assert (!(fcntl (pipes[0], F_GETFL) & O_NONBLOCK));
  close (pipes[0]);
  close (pipes[1]);
  assert (mulib_filectl_lock (-1, 1) == -1);
}
static void
udp (void)
{
  mulib_datagram_t a = MULIB_DATAGRAM_INIT, b = MULIB_DATAGRAM_INIT;
  mulib_datagram_address_t bind_address = { { 127, 0, 0, 1 }, 0, 4, 0 }, aa,
                           bb, source;
  size_t n;
  char bytes[8];
  assert (!mulib_datagram_open (&a, &bind_address, 1, &aa));
  assert (!mulib_datagram_open (&b, &bind_address, 1, &bb));
  assert (aa.port && bb.port && aa.port != bb.port);
  assert (fcntl (a.fd, F_GETFD) & FD_CLOEXEC);
  assert (fcntl (a.fd, F_GETFL) & O_NONBLOCK);
  assert (mulib_datagram_receive (&b, &source, bytes, sizeof bytes, &n)
              == MULIB_DATAGRAM_AGAIN
          && !n);
  assert (!mulib_datagram_send (&a, &bb, "abcdefgh", 8));
  assert (mulib_datagram_receive (&b, &source, bytes, 3, &n)
              == MULIB_DATAGRAM_TRUNCATED
          && n == 3);
  assert (source.port == aa.port && !memcmp (bytes, "abc", 3));
  assert (!mulib_datagram_send (&a, &bb, NULL, 0));
  assert (!mulib_datagram_receive (&b, &source, NULL, 0, &n) && n == 0);
  assert (!mulib_datagram_close (&a));
  assert (!mulib_datagram_close (&a));
  assert (!mulib_datagram_close (&b));
}
static int
interface_visit (void *ctx, const mulib_netdevice_info_t *info)
{
  ++*(int *)ctx;
  assert (info->name && info->index == mulib_netdevice_index (info->name));
  return 1;
}
#endif
int
main (void)
{
  uint64_t next = 123;
  assert (!mulib_schedutils_next (0, 0, 1, &next) && next == 123);
  assert (mulib_schedutils_next (100, 7, 99, &next) && next == 100);
  assert (mulib_schedutils_next (100, 7, 100, &next) && next == 107);
  assert (!mulib_schedutils_next (0, 1, UINT64_MAX, &next) && next == 107);
  assert (mulib_schedutils_next (0, UINT64_MAX, 0, &next)
          && next == UINT64_MAX);
  {
    mulib_tuntap_t t = MULIB_TUNTAP_INIT;
    assert (mulib_tuntap_open (&t, "", 0, 0) == -1);
    assert (mulib_tuntap_open (&t, "01234567890123456", 0, 1500) == -1);
  }
#if defined(__unix__) || defined(__APPLE__)
  {
    int policy, priority, count = 0;
    assert (!mulib_schedutils_current (&policy, &priority));
    assert (!mulib_schedutils_yield ());
    assert (mulib_netdevice_list (interface_visit, &count) == 1 && count == 1);
  }
  memory ();
  queues ();
  descriptors ();
  udp ();
#endif
  return 0;
}
