/** @file mulib-dbus-client.h
 * @brief Minimal system D-Bus method-call client with zero, one, or two string
 * arguments C11, single-header, define MULIB_IMPL_DBUS_CLIENT exactly once.
 */
#ifndef MULIB_DBUS_CLIENT_H
#define MULIB_DBUS_CLIENT_H
#include "mulib-dbus-transport.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_dbus_client
  {
    int fd;
    uint32_t serial;
  } mulib_dbus_client_t;
#define MULIB_DBUS_CLIENT_INIT { -1, 0 }
  int mulib_dbus_client_open (mulib_dbus_client_t *c,
                              const char *system_bus_path);
  int mulib_dbus_client_close (mulib_dbus_client_t *c);
  int mulib_dbus_client_call (mulib_dbus_client_t *c, const char *destination,
                              const char *object_path, const char *interface,
                              const char *member, const char *arg1,
                              const char *arg2, void *response,
                              size_t capacity, size_t *received);
  size_t mulib_dbus_client_body_offset (const void *message, size_t size);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DBUS_CLIENT) && !defined(MULIB_DBUS_CLIENT_IMPLEMENTED)
#define MULIB_DBUS_CLIENT_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <unistd.h>
#endif
static size_t
mulib_dbus_client_align_ (size_t x, size_t a)
{
  return (x + a - 1) & ~(a - 1);
}
static void
mulib_dbus_client_put32_ (uint8_t *p, uint32_t n)
{
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (i * 8));
}
static uint32_t
mulib_dbus_client_get32_ (const uint8_t *p)
{
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16
         | (uint32_t)p[3] << 24;
}
static int
mulib_dbus_client_field_ (uint8_t *buf, size_t cap, size_t *used, uint8_t code,
                          char type, const char *s)
{
  size_t n = strlen (s);
  size_t o = mulib_dbus_client_align_ (*used, 8);
  if (n > 1024 || o + 12 + n > cap)
    return -ENOSPC;
  memset (buf + *used, 0, o - *used);
  buf[o++] = code;
  buf[o++] = 1;
  buf[o++] = (uint8_t)type;
  buf[o++] = 0;
  o = mulib_dbus_client_align_ (o, 4);
  mulib_dbus_client_put32_ (buf + o, (uint32_t)n);
  o += 4;
  memcpy (buf + o, s, n);
  o += n;
  buf[o++] = 0;
  *used = o;
  return 0;
}
size_t
mulib_dbus_client_body_offset (const void *p, size_t n)
{
  if (!p || n < 16)
    return 0;
  const uint8_t *b = p;
  if (b[0] != 'l' || b[3] != 1 || b[1] < 1 || b[1] > 4
      || !mulib_dbus_client_get32_ (b + 8))
    return 0;
  uint32_t h = mulib_dbus_client_get32_ (b + 12),
           body = mulib_dbus_client_get32_ (b + 4);
  if (h > 1048576 - 16 || body > 1048576 - 16 - h)
    return 0;
  size_t off = 16 + mulib_dbus_client_align_ ((size_t)h, 8);
  return off <= n && body <= n - off ? off : 0;
}
static int
mulib_dbus_client_reply_ (const uint8_t *b, size_t n, uint32_t serial)
{
  if (!mulib_dbus_client_body_offset (b, n))
    return -EBADMSG;
  uint32_t hl = mulib_dbus_client_get32_ (b + 12);
  size_t pos = 16, end = 16 + (size_t)hl;
  if (end > n)
    return -EBADMSG;
  uint32_t reply = 0;
  unsigned seen = 0;
  while (pos < end)
    {
      pos = mulib_dbus_client_align_ (pos, 8);
      if (pos == end)
        break;
      if (pos + 4 > end)
        return -EBADMSG;
      unsigned key = b[pos++], slen = b[pos++];
      if (slen != 1 || pos + slen + 1 > end || b[pos + slen] != 0)
        return -EBADMSG;
      if (key == 0)
        return -EBADMSG;
      if (key <= 9)
        {
          if (seen & (1u << key))
            return -EBADMSG;
          seen |= 1u << key;
        }
      char type = (char)b[pos];
      pos += slen + 1;
      if ((key == 5 || key == 9) && type != 'u')
        return -EBADMSG;
      if ((key == 2 || key == 3 || key == 4 || key == 6 || key == 7)
          && type != 's')
        return -EBADMSG;
      if (key == 1 && type != 'o')
        return -EBADMSG;
      if (key == 8 && type != 'g')
        return -EBADMSG;
      if (type == 'u')
        {
          pos = mulib_dbus_client_align_ (pos, 4);
          if (pos + 4 > end)
            return -EBADMSG;
          if (key == 5)
            reply = mulib_dbus_client_get32_ (b + pos);
          pos += 4;
        }
      else if (type == 's' || type == 'o')
        {
          pos = mulib_dbus_client_align_ (pos, 4);
          if (pos + 4 > end)
            return -EBADMSG;
          uint32_t len = mulib_dbus_client_get32_ (b + pos);
          pos += 4;
          if (pos >= end || len > end - pos - 1 || b[pos + len] != 0
              || memchr (b + pos, 0, len))
            return -EBADMSG;
          pos += len + 1;
        }
      else if (type == 'g')
        {
          if (pos >= end)
            return -EBADMSG;
          unsigned len = b[pos++];
          if (pos >= end || len > end - pos - 1 || b[pos + len] != 0
              || memchr (b + pos, 0, len))
            return -EBADMSG;
          pos += len + 1;
        }
      else
        return -EPROTONOSUPPORT;
    }
  if (b[1] != 2 && b[1] != 3)
    return 1;
  if (!reply)
    return -EBADMSG;
  if (reply != serial)
    return 1;
  if (b[1] == 3)
    return -EREMOTEIO;
  return 0;
}
int
mulib_dbus_client_call (mulib_dbus_client_t *c, const char *dst,
                        const char *path, const char *iface,
                        const char *member, const char *a, const char *b,
                        void *response, size_t cap, size_t *got)
{
  if (!c || c->fd < 0 || !dst || !path || !iface || !member || !response
      || !got || cap < 16 || (!a && b))
    return -EINVAL;
  *got = 0;
  uint8_t msg[8192] = { 0 };
  size_t pos = 0;
  int r;
  if ((r = mulib_dbus_client_field_ (msg + 16, 4000, &pos, 1, 'o', path))
      || (r = mulib_dbus_client_field_ (msg + 16, 4000, &pos, 2, 's', iface))
      || (r = mulib_dbus_client_field_ (msg + 16, 4000, &pos, 3, 's', member))
      || (r = mulib_dbus_client_field_ (msg + 16, 4000, &pos, 6, 's', dst)))
    return r;
  const char *sig = b ? "ss" : a ? "s" : "";
  if (a)
    {
      size_t o = mulib_dbus_client_align_ (pos, 8);
      if (o + 4 + strlen (sig) > 4000)
        return -ENOSPC;
      memset (msg + 16 + pos, 0, o - pos);
      pos = o;
      msg[16 + pos++] = 8;
      msg[16 + pos++] = 1;
      msg[16 + pos++] = 'g';
      msg[16 + pos++] = 0;
      // Field 8 variant of signature requires the g string's length byte and
      // terminator.
      msg[16 + pos++] = (uint8_t)strlen (sig);
      memcpy (msg + 16 + pos, sig, strlen (sig));
      pos += strlen (sig);
      msg[16 + pos++] = 0;
    }
  size_t hlen = pos, off = 16 + mulib_dbus_client_align_ (hlen, 8);
  if (off > sizeof msg)
    return -ENOSPC;
  const char *args[2] = { a, b };
  for (unsigned i = 0; i < 2; i++)
    if (args[i])
      {
        size_t n = strlen (args[i]);
        size_t v = mulib_dbus_client_align_ (off, 4);
        if (n > 1024 || v + 5 + n > sizeof msg)
          return -ENOSPC;
        off = v;
        mulib_dbus_client_put32_ (msg + off, (uint32_t)n);
        off += 4;
        memcpy (msg + off, args[i], n);
        off += n;
        msg[off++] = 0;
      }
  msg[0] = 'l';
  msg[1] = 1;
  msg[2] = 0;
  msg[3] = 1;
  mulib_dbus_client_put32_ (
      msg + 4, (uint32_t)(off - 16 - mulib_dbus_client_align_ (hlen, 8)));
  if (++c->serial == 0)
    ++c->serial;
  uint32_t serial = c->serial;
  mulib_dbus_client_put32_ (msg + 8, serial);
  mulib_dbus_client_put32_ (msg + 12, (uint32_t)hlen);
  r = mulib_dbus_transport_write (c->fd, msg, off);
  if (r)
    return r;
  for (unsigned tries = 0; tries < 64; tries++)
    {
      size_t used = 0;
      r = mulib_dbus_transport_read (c->fd, response, cap, &used);
      if (r)
        return r;
      r = mulib_dbus_client_reply_ (response, used, serial);
      if (r == 1)
        continue;
      if (r)
        return r;
      *got = used;
      return 0;
    }
  return -EOVERFLOW;
}
int
mulib_dbus_client_open (mulib_dbus_client_t *c, const char *path)
{
  if (!c || c->fd != -1)
    return -EINVAL;
  int fd = mulib_dbus_transport_connect (path);
  if (fd < 0)
    return fd;
  c->fd = fd;
  c->serial = 0;
  uint8_t reply[4096];
  size_t size = 0;
  int r = mulib_dbus_client_call (
      c, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "Hello", NULL, NULL, reply, sizeof reply, &size);
  if (r)
    {
      mulib_dbus_client_close (c);
      return r;
    }
  return 0;
}
int
mulib_dbus_client_close (mulib_dbus_client_t *c)
{
  if (!c)
    return -EINVAL;
#ifdef __linux__
  if (c->fd >= 0)
    close (c->fd);
  c->fd = -1;
  c->serial = 0;
  return 0;
#else
  return -ENOSYS;
#endif
}
#endif
