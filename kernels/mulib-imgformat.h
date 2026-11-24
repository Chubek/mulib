/** @file mulib-imgformat.h
 * @brief Versioned, checksummed container image and process-spec wire formats.
 * Define MULIB_IMPL_IMGFORMAT plus CONTAINER and CRC32. All integers are LE. */
#ifndef MULIB_IMGFORMAT_H
#define MULIB_IMGFORMAT_H
#include "mulib-container.h"
#include "mulib-crc32.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_IMGFORMAT_HEADER_SIZE = 64, MULIB_IMGFORMAT_CONFIG_LIMIT = 1048576 };
typedef enum mulib_imgformat_arch
{
  MULIB_IMGFORMAT_PORTABLE = 0, MULIB_IMGFORMAT_AMD64 = 1,
  MULIB_IMGFORMAT_ARM64 = 2, MULIB_IMGFORMAT_OIX = 3, MULIB_IMGFORMAT_QVM = 4
} mulib_imgformat_arch;
/** @brief Borrowed validated envelope; tar syntax is checked by image2container. */
typedef struct mulib_imgformat_view
{
  const unsigned char *config, *rootfs;
  size_t config_length, rootfs_length;
  mulib_imgformat_arch architecture;
  uint32_t checksum;
} mulib_imgformat_view;
/** @brief Encode all process fields. Replace initialized blob atomically. */
int mulib_imgformat_spec_encode (const mulib_container_spec *spec,
                                 mulib_ct_blob *out, size_t limit);
/** @brief Decode into uninitialized output, validating strings, quotas and trailing bytes. */
int mulib_imgformat_spec_decode (const void *data, size_t length,
                                 mulib_container_spec *out);
/** @brief Build a complete envelope; input may alias out. No compression. */
int mulib_imgformat_pack (const mulib_container_spec *spec,
                          mulib_imgformat_arch architecture,
                          const void *rootfs, size_t length,
                          mulib_ct_blob *out, size_t limit);
/** @brief Validate version, flags, reserved bytes, exact lengths and both CRCs. */
int mulib_imgformat_open (const void *data, size_t length, size_t limit,
                          mulib_imgformat_view *view);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IMGFORMAT) && !defined(MULIB_IMGFORMAT_IMPLEMENTED)
#define MULIB_IMGFORMAT_IMPLEMENTED
static int
mulib_imgformat_string_ (mulib_ct_blob *out, const char *text, size_t limit)
{
  unsigned char prefix[4];
  size_t n = strlen (text);
  int rc;
  if (n > UINT32_MAX) return MULIB_CT_ELIMIT;
  mulib_ct_store32 (prefix, (uint32_t)n);
  rc = mulib_ct_blob_append (out, prefix, 4, limit);
  if (!rc) rc = mulib_ct_blob_append (out, text, n, limit);
  return rc;
}
int
mulib_imgformat_spec_encode (const mulib_container_spec *s, mulib_ct_blob *out,
                             size_t limit)
{
  mulib_ct_blob next = { NULL, 0, 0 };
  unsigned char h[64] = { 0 };
  size_t i;
  int rc;
  if (!out) return MULIB_CT_EINVAL;
  rc = mulib_container_spec_validate (s);
  if (rc) return rc;
  if (s->argc > 4096 || s->envc > 4096 || s->argc + s->envc > 4096)
    return MULIB_CT_ELIMIT;
  if (limit > MULIB_IMGFORMAT_CONFIG_LIMIT) limit = MULIB_IMGFORMAT_CONFIG_LIMIT;
  memcpy (h, "MCFG", 4); mulib_ct_store32 (h + 4, 1);
  mulib_ct_store32 (h + 8, (uint32_t)s->argc);
  mulib_ct_store32 (h + 12, (uint32_t)s->envc);
  mulib_ct_store32 (h + 16, s->uid); mulib_ct_store32 (h + 20, s->gid);
  mulib_ct_store32 (h + 24, (uint32_t)s->read_only);
  mulib_ct_store64 (h + 32, s->limits.memory_bytes);
  mulib_ct_store64 (h + 40, s->limits.cpu_budget);
  mulib_ct_store64 (h + 48, s->limits.wall_deadline);
  mulib_ct_store32 (h + 56, s->limits.processes);
  mulib_ct_store32 (h + 60, s->limits.open_files);
  rc = mulib_ct_blob_append (&next, h, sizeof (h), limit);
  if (!rc) rc = mulib_imgformat_string_ (&next, s->id, limit);
  if (!rc) rc = mulib_imgformat_string_ (&next, s->cwd, limit);
  for (i = 0; i < s->argc && !rc; ++i) rc = mulib_imgformat_string_ (&next, s->argv[i], limit);
  for (i = 0; i < s->envc && !rc; ++i) rc = mulib_imgformat_string_ (&next, s->env[i], limit);
  if (rc) { mulib_ct_blob_free (&next); return rc; }
  mulib_ct_blob_free (out); *out = next;
  return 0;
}
static int
mulib_imgformat_take_ (const unsigned char *p, size_t n, size_t *at, char **out)
{
  uint32_t len;
  char *s;
  if (*at > n || n - *at < 4) return MULIB_CT_EFORMAT;
  len = mulib_ct_load32 (p + *at); *at += 4;
  if (len > n - *at || memchr (p + *at, 0, len)) return MULIB_CT_EFORMAT;
  s = (char *)MULIB_CT_MALLOC ((size_t)len + 1);
  if (!s) return MULIB_CT_ENOMEM;
  memcpy (s, p + *at, len); s[len] = 0; *at += len; *out = s;
  return 0;
}
int
mulib_imgformat_spec_decode (const void *data, size_t n, mulib_container_spec *out)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_container_spec s;
  char *text = NULL;
  size_t at = 64, i, argc, envc;
  int rc;
  if (!p || !out) return MULIB_CT_EINVAL;
  if (n > MULIB_IMGFORMAT_CONFIG_LIMIT) return MULIB_CT_ELIMIT;
  if (n < 64 || memcmp (p, "MCFG", 4) || mulib_ct_load32 (p + 4) != 1
      || mulib_ct_load32 (p + 28) || mulib_ct_load32 (p + 24) > 1)
    return MULIB_CT_EFORMAT;
  argc = mulib_ct_load32 (p + 8); envc = mulib_ct_load32 (p + 12);
  if (argc > 4096 || envc > 4096 - argc) return MULIB_CT_ELIMIT;
  rc = mulib_imgformat_take_ (p, n, &at, &text);
  if (rc) return rc;
  rc = mulib_container_spec_init (&s, text); MULIB_CT_FREE (text); text = NULL;
  if (rc) return MULIB_CT_EFORMAT;
  s.max_strings = 4096; s.max_string_bytes = MULIB_IMGFORMAT_CONFIG_LIMIT;
  s.uid = mulib_ct_load32 (p + 16); s.gid = mulib_ct_load32 (p + 20);
  s.read_only = (int)mulib_ct_load32 (p + 24);
  s.limits.memory_bytes = mulib_ct_load64 (p + 32);
  s.limits.cpu_budget = mulib_ct_load64 (p + 40);
  s.limits.wall_deadline = mulib_ct_load64 (p + 48);
  s.limits.processes = mulib_ct_load32 (p + 56);
  s.limits.open_files = mulib_ct_load32 (p + 60);
  rc = mulib_imgformat_take_ (p, n, &at, &text);
  if (!rc) rc = mulib_container_spec_cwd (&s, text);
  MULIB_CT_FREE (text); text = NULL;
  for (i = 0; i < argc + envc && !rc; ++i)
    {
      rc = mulib_imgformat_take_ (p, n, &at, &text);
      if (!rc && i < argc) rc = mulib_container_spec_arg (&s, text);
      else if (!rc)
        {
          char *eq = strchr (text, '=');
          if (!eq) rc = MULIB_CT_EFORMAT;
          else
            {
              *eq = 0;
              if (mulib_container_spec_getenv (&s, text)) rc = MULIB_CT_EFORMAT;
              else rc = mulib_container_spec_env (&s, text, eq + 1);
            }
        }
      MULIB_CT_FREE (text); text = NULL;
    }
  if (!rc && at != n) rc = MULIB_CT_EFORMAT;
  if (rc) { mulib_container_spec_free (&s); return rc; }
  *out = s;
  return 0;
}
static uint32_t
mulib_imgformat_header_crc_ (const unsigned char *p)
{
  return mulib_crc32_update (mulib_crc32 (p, 36), p + 40, 24);
}
int
mulib_imgformat_pack (const mulib_container_spec *s, mulib_imgformat_arch arch,
                      const void *root, size_t n, mulib_ct_blob *out, size_t limit)
{
  mulib_ct_blob config = { NULL, 0, 0 }, image = { NULL, 0, 0 };
  unsigned char h[64] = { 0 };
  size_t total;
  int rc;
  if (!out || (!root && n) || arch < 0 || arch > MULIB_IMGFORMAT_QVM)
    return MULIB_CT_EINVAL;
  rc = mulib_imgformat_spec_encode (s, &config, limit);
  if (rc) return rc;
  if (mulib_ct_add_size (64, config.length, &total)
      || mulib_ct_add_size (total, n, &total) || total > limit)
    { rc = MULIB_CT_ELIMIT; goto done; }
  memcpy (h, "MULIMG1", 7); mulib_ct_store32 (h + 8, 1);
  mulib_ct_store64 (h + 16, config.length); mulib_ct_store64 (h + 24, n);
  mulib_ct_store32 (h + 32, mulib_crc32_update (
      mulib_crc32 (config.data, config.length), root, n));
  mulib_ct_store32 (h + 40, (uint32_t)arch);
  mulib_ct_store32 (h + 36, mulib_imgformat_header_crc_ (h));
  rc = mulib_ct_blob_reserve (&image, total, limit);
  if (!rc) rc = mulib_ct_blob_append (&image, h, 64, limit);
  if (!rc) rc = mulib_ct_blob_append (&image, config.data, config.length, limit);
  if (!rc) rc = mulib_ct_blob_append (&image, root, n, limit);
  if (!rc) { mulib_ct_blob_free (out); *out = image; memset (&image, 0, sizeof (image)); }
done:
  mulib_ct_blob_free (&config); mulib_ct_blob_free (&image);
  return rc;
}
int
mulib_imgformat_open (const void *data, size_t n, size_t limit,
                      mulib_imgformat_view *out)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_imgformat_view v;
  uint64_t a, b;
  size_t i;
  if (!p || !out) return MULIB_CT_EINVAL;
  if (n > limit) return MULIB_CT_ELIMIT;
  if (n < 64 || memcmp (p, "MULIMG1\0", 8) || mulib_ct_load32 (p + 8) != 1
      || mulib_ct_load32 (p + 12) || mulib_ct_load32 (p + 40) > MULIB_IMGFORMAT_QVM)
    return MULIB_CT_EFORMAT;
  for (i = 44; i < 64; ++i) if (p[i]) return MULIB_CT_EFORMAT;
  if (mulib_imgformat_header_crc_ (p) != mulib_ct_load32 (p + 36))
    return MULIB_CT_ECHECKSUM;
  a = mulib_ct_load64 (p + 16); b = mulib_ct_load64 (p + 24);
  if (a > MULIB_IMGFORMAT_CONFIG_LIMIT || a > n - 64 || b != n - 64 - a)
    return MULIB_CT_EFORMAT;
  v.checksum = mulib_ct_load32 (p + 32);
  if (v.checksum != mulib_crc32 (p + 64, n - 64)) return MULIB_CT_ECHECKSUM;
  v.architecture = (mulib_imgformat_arch)mulib_ct_load32 (p + 40);
  v.config = p + 64; v.config_length = (size_t)a;
  v.rootfs = p + 64 + (size_t)a; v.rootfs_length = (size_t)b;
  *out = v;
  return 0;
}
#endif
