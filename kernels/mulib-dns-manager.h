/** @file mulib-dns-manager.h
 * @brief Read first nameserver IP literal from resolv.conf without changing
 * system DNS C11, single-header, define MULIB_IMPL_DNS_MANAGER exactly once.
 */
#ifndef MULIB_DNS_MANAGER_H
#define MULIB_DNS_MANAGER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_dns_manager_first_nameserver (const char *path, char *out,
                                          size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DNS_MANAGER) && !defined(MULIB_DNS_MANAGER_IMPLEMENTED)
#define MULIB_DNS_MANAGER_IMPLEMENTED
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef __linux__
#include <arpa/inet.h>
#endif
int
mulib_dns_manager_first_nameserver (const char *file, char *out, size_t cap)
{
  if (!file || !out || cap < 2)
    return -EINVAL;
  FILE *f = fopen (file, "r");
  if (!f)
    return -errno;
  char line[512];
  while (fgets (line, sizeof line, f))
    {
      if (!strchr (line, '\n') && !feof (f))
        {
          int c = fgetc (f);
          if (c != EOF)
            {
              fclose (f);
              return -E2BIG;
            }
        }
      char word[32], addr[128];
      if (sscanf (line, "%31s %127s", word, addr) == 2
          && !strcmp (word, "nameserver"))
        {
#ifdef __linux__
          unsigned char parsed[16];
          if (inet_pton (AF_INET, addr, parsed) != 1
              && inet_pton (AF_INET6, addr, parsed) != 1)
            {
              fclose (f);
              return -EBADMSG;
            }
#else
          fclose (f);
          return -ENOSYS;
#endif
          size_t n = strlen (addr);
          if (n >= cap)
            {
              fclose (f);
              return -ENOSPC;
            }
          memcpy (out, addr, n + 1);
          fclose (f);
          return 0;
        }
    }
  int err = ferror (f);
  fclose (f);
  return err ? -EIO : -ENOENT;
}
#endif
