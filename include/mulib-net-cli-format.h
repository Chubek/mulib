/** @file mulib-net-cli-format.h
 * @brief Format IPv4 and JSON-escaped interface names without third-party
 * libraries C11, single-header, define MULIB_IMPL_NET_CLI_FORMAT exactly once.
 */
#ifndef MULIB_NET_CLI_FORMAT_H
#define MULIB_NET_CLI_FORMAT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_net_cli_format_ipv4 (const uint8_t ip[4], char *out, size_t cap);
  int mulib_net_cli_format_json (const char *input, char *out, size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NET_CLI_FORMAT)                                        \
    && !defined(MULIB_NET_CLI_FORMAT_IMPLEMENTED)
#define MULIB_NET_CLI_FORMAT_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#include <string.h>
int
mulib_net_cli_format_ipv4 (const uint8_t ip[4], char *out, size_t cap)
{
  if (!ip || !out)
    return -EINVAL;
  int n = snprintf (out, cap, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return n < 0 || (size_t)n >= cap ? -ENOSPC : n;
}
int
mulib_net_cli_format_json (const char *str, char *out, size_t cap)
{
  if (!str || !out || cap < 3)
    return -EINVAL;
  size_t j = 0;
  out[j++] = '"';
  for (const unsigned char *p = (const void *)str; *p; p++)
    {
      if (*p < 32)
        {
          if (j + 6 >= cap)
            return -ENOSPC;
          int n = snprintf (out + j, cap - j, "\\u%04x", *p);
          j += (size_t)n;
        }
      else if (*p == '"' || *p == '\\')
        {
          if (j + 2 >= cap)
            return -ENOSPC;
          out[j++] = '\\';
          out[j++] = (char)*p;
        }
      else
        {
          if (j + 1 >= cap)
            return -ENOSPC;
          out[j++] = (char)*p;
        }
    }
  if (j + 2 > cap)
    return -ENOSPC;
  out[j++] = '"';
  out[j] = 0;
  return (int)j;
}
#endif
