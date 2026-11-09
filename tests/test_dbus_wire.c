
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#define MULIB_IMPL_DBUS_AUTH
#define MULIB_IMPL_DBUS_CLIENT
#define MULIB_IMPL_DBUS_PROPERTIES
#define MULIB_IMPL_DBUS_TRANSPORT
#include "mulib-dbus-properties.h"

static void
readall (int fd, void *data, size_t n)
{
  uint8_t *p = data;
  while (n)
    {
      ssize_t r = read (fd, p, n);
      assert (r > 0);
      p += (size_t)r;
      n -= (size_t)r;
    }
}
static void
serve (int fd, int variant)
{
  uint8_t fixed[16];
  readall (fd, fixed, 16);
  assert (fixed[0] == 'l' && fixed[1] == 1 && fixed[3] == 1);
  uint32_t h = 0, b = 0;
  memcpy (&b, fixed + 4, 4);
  memcpy (&h, fixed + 12, 4);
  assert (h < 1000 && b < 1000);
  uint8_t rest[2048];
  size_t n = (((size_t)h + 7u) & ~(size_t)7u) + b;
  readall (fd, rest, n);
  assert (memchr (rest, 's', h) != NULL); // D-Bus header string fields
  uint8_t reply[32] = { 0 };
  reply[0] = 'l';
  reply[1] = 2;
  reply[3] = 1;
  reply[4] = variant ? 8 : 0;
  reply[8] = 77;
  reply[12] = 8;
  reply[16] = 5;
  reply[17] = 1;
  reply[18] = 'u';
  reply[20] = 1;
  if (variant)
    {
      reply[24] = 1;
      reply[25] = 'u';
      reply[28] = 70;
    }
  size_t size = variant ? 32 : 24;
  assert (write (fd, reply, size) == (ssize_t)size);
  close (fd);
}
int
main (void)
{
  for (int variant = 0; variant < 2; variant++)
    {
      int sockets[2];
      assert (!socketpair (AF_UNIX, SOCK_STREAM, 0, sockets));
      pid_t child = fork ();
      assert (child >= 0);
      if (!child)
        {
          close (sockets[0]);
          serve (sockets[1], variant);
          _exit (0);
        }
      close (sockets[1]);
      mulib_dbus_client_t c = { sockets[0], 0 };
      if (variant)
        {
          uint32_t n = 0;
          assert (!mulib_dbus_properties_u32 (
              &c, "org.example", "/org/example", "org.example", "State", &n));
          assert (n == 70);
        }
      else
        {
          uint8_t buf[256];
          size_t n = 0;
          assert (!mulib_dbus_client_call (&c, "org.example", "/org/example",
                                           "org.example", "Ping", NULL, NULL,
                                           buf, sizeof buf, &n));
          assert (n == 24);
        }
      mulib_dbus_client_close (&c);
      int st = 0;
      assert (waitpid (child, &st, 0) == child);
      assert (WIFEXITED (st) && WEXITSTATUS (st) == 0);
    }
  puts ("PASS D-Bus wire: method framing, reply serial matching, uint32 "
        "variants");
  return 0;
}
