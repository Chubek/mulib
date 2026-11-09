/** @file mulib-machobuilder.h
 * @brief Owned multi-section little-endian Mach-O64 MH_OBJECT builder.
 * Define MULIB_IMPL_MACHOBUILDER and OBJFILEBUILDER dependencies.
 * Also enables the historical machobuidler text API without redefining its
 * implementation switch; both entry headers can coexist in one TU. */
#ifndef MULIB_MACHOBUILDER_H
#define MULIB_MACHOBUILDER_H
#include "mulib-objfilebuilder.h"
#include "mulib-binfilewriter.h"
#include "mulib-bycutils.h"
#include "mulib-machobuidler.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_objfilebuilder_t mulib_machobuilder_t;
/** @brief Copy a 1..16-byte section name; alignment <=32768, flags 1/2/4.
 * EXEC sections use __TEXT, others __DATA. Zero initialize builder. */
int mulib_machobuilder_add (mulib_machobuilder_t *b, const char *name,
    const void *data, size_t size, size_t alignment, uint64_t flags);
/** @brief Append x86-64 (0x01000007) or arm64 (0x0100000c) object atomically.
 * No symbols, relocations, executable entry or signatures. */
int mulib_machobuilder_finish (const mulib_machobuilder_t *b, uint32_t cpu, mulib_buffer_t *out);
/** @brief Release copied names/bytes. */
void mulib_machobuilder_free (mulib_machobuilder_t *b);
typedef enum mulib_machobuilder_abi {
  MULIB_MACHOBUILDER_MACOS_X86_64, MULIB_MACHOBUILDER_MACOS_ARM64,
  MULIB_MACHOBUILDER_IOS_ARM64, MULIB_MACHOBUILDER_IOS_SIM_X86_64,
  MULIB_MACHOBUILDER_IOS_SIM_ARM64
} mulib_machobuilder_abi_t;
/** Append MH_OBJECT with CPU/subtype and LC_BUILD_VERSION platform. min_os and
 * sdk use packed major<<16|minor<<8|patch; zero SDK means unspecified. */
int mulib_machobuilder_finish_abi (const mulib_machobuilder_t *b,
    mulib_machobuilder_abi_t abi, uint32_t min_os, uint32_t sdk, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MACHOBUILDER) && !defined(MULIB_MACHOBUILDER_IMPLEMENTED)
#define MULIB_MACHOBUILDER_IMPLEMENTED
int mulib_machobuilder_add (mulib_machobuilder_t *b, const char *name,
    const void *p, size_t n, size_t a, uint64_t flags)
{
  if (!name || strlen (name) > 16 || a > 32768 || n > UINT32_MAX) return 0;
  return mulib_objfilebuilder_add (b, name, p, n, a, flags);
}
void mulib_machobuilder_free (mulib_machobuilder_t *b) { mulib_objfilebuilder_free (b); }
static int mulib_machobuilder_write_ (const mulib_machobuilder_t *b, uint32_t cpu,
    uint32_t platform, uint32_t min_os, uint32_t sdk, mulib_buffer_t *out)
{
  mulib_buffer_t tmp = { 0 };
  unsigned char h[104] = { 0 };
  size_t i, start, alignment = 1;
  uint64_t address = 0;
  int ok = 0;
  if (!b || !out || (cpu != 0x01000007u && cpu != 0x0100000cu)
      || b->count > 255 || (!b->sections && b->count)) return 0;
  mulib_bycutils_store (h, 104, 0, 4, 0xfeedfacf);
  mulib_bycutils_store (h, 104, 4, 4, cpu); mulib_bycutils_store (h, 104, 8, 4, cpu == 0x01000007u ? 3 : 0);
  mulib_bycutils_store (h, 104, 12, 4, 1); mulib_bycutils_store (h, 104, 16, 4, platform ? 2 : 1);
  mulib_bycutils_store (h, 104, 20, 4, 72 + b->count * 80 + (platform ? 24 : 0));
  mulib_bycutils_store (h, 104, 32, 4, 0x19); mulib_bycutils_store (h, 104, 36, 4, 72 + b->count * 80);
  mulib_bycutils_store (h, 104, 88, 4, 7); mulib_bycutils_store (h, 104, 92, 4, 7);
  mulib_bycutils_store (h, 104, 96, 4, b->count);
  if (!mulib_buffer_append (&tmp, h, 104)) goto done;
  memset (h, 0, 80);
  for (i = 0; i < b->count; ++i) if (!mulib_buffer_append (&tmp, h, 80)) goto done;
  if (platform) {
    memset (h, 0, 24);
    mulib_bycutils_store (h, 24, 0, 4, 0x32); mulib_bycutils_store (h, 24, 4, 4, 24);
    mulib_bycutils_store (h, 24, 8, 4, platform); mulib_bycutils_store (h, 24, 12, 4, min_os);
    mulib_bycutils_store (h, 24, 16, 4, sdk);
    if (!mulib_buffer_append (&tmp, h, 24)) goto done;
  }
  for (i = 0; i < b->count; ++i) {
    size_t a = b->sections[i].alignment;
    if (!a || a > 32768 || (a & (a - 1))) goto done;
    if (a > alignment) alignment = a;
  }
  if (!mulib_binfilewriter_align (&tmp, alignment)) goto done;
  start = tmp.length;
  for (i = 0; i < b->count; ++i) {
    const mulib_objfilewriter_section_t *s = b->sections + i;
    unsigned exponent = 0;
    size_t a;
    if (!s->name || !*s->name || strlen (s->name) > 16 || !s->alignment || s->alignment > 32768
        || (s->alignment & (s->alignment - 1)) || (s->flags & ~UINT64_C (7))
        || (!s->data && s->size) || !mulib_binfilewriter_align (&tmp, s->alignment)
        || tmp.length > UINT32_MAX || s->size > UINT32_MAX - tmp.length) goto done;
    address = tmp.length - start;
    for (a = s->alignment; a > 1; a >>= 1) ++exponent;
    memset (h, 0, 80); memcpy (h, s->name, strlen (s->name));
    memcpy (h + 16, s->flags & 4 ? "__TEXT" : "__DATA", 6);
    mulib_bycutils_store (h, 80, 32, 8, address); mulib_bycutils_store (h, 80, 40, 8, s->size);
    mulib_bycutils_store (h, 80, 48, 4, tmp.length); mulib_bycutils_store (h, 80, 52, 4, exponent);
    mulib_bycutils_store (h, 80, 64, 4, s->flags & 4 ? 0x80000400u : 0);
    memcpy (tmp.data + 104 + i * 80, h, 80);
    if (!mulib_buffer_append (&tmp, s->data, s->size)) goto done;
  }
  mulib_binfilewriter_patch (&tmp, 64, 8, tmp.length - start);
  mulib_binfilewriter_patch (&tmp, 72, 8, start);
  mulib_binfilewriter_patch (&tmp, 80, 8, tmp.length - start);
  ok = mulib_buffer_append (out, tmp.data, tmp.length);
done:
  mulib_buffer_free (&tmp); return ok;
}
int mulib_machobuilder_finish (const mulib_machobuilder_t *b, uint32_t cpu, mulib_buffer_t *out)
{ return mulib_machobuilder_write_ (b, cpu, 0, 0, 0, out); }
int mulib_machobuilder_finish_abi (const mulib_machobuilder_t *b,
    mulib_machobuilder_abi_t abi, uint32_t min_os, uint32_t sdk, mulib_buffer_t *out)
{
  static const uint32_t cpu[] = {0x01000007, 0x0100000c, 0x0100000c, 0x01000007, 0x0100000c};
  static const uint32_t platform[] = {1, 1, 2, 7, 7};
  if ((unsigned)abi >= sizeof (cpu) / sizeof (*cpu) || !min_os || (sdk && sdk < min_os)) return 0;
  return mulib_machobuilder_write_ (b, cpu[(unsigned)abi], platform[(unsigned)abi], min_os, sdk, out);
}
#endif
