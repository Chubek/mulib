/** @file mulib-elfbuilder.h
 * @brief Owned ELF64 relocatable section builder reusing objfilebuilder.
 * Define MULIB_IMPL_ELFBUILDER and OBJFILEBUILDER dependencies.
 * No symbols, relocations or executable segments; see objfilewriter profile. */
#ifndef MULIB_ELFBUILDER_H
#define MULIB_ELFBUILDER_H
#include "mulib-objfilebuilder.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef mulib_objfilebuilder_t mulib_elfbuilder_t;
  int mulib_elfbuilder_add (mulib_elfbuilder_t *builder, const char *name,
                            const void *data, size_t size, size_t alignment,
                            uint64_t flags);
  int mulib_elfbuilder_finish (const mulib_elfbuilder_t *builder,
                               uint16_t machine, mulib_buffer_t *out);
  void mulib_elfbuilder_free (mulib_elfbuilder_t *builder);
  typedef enum mulib_elfbuilder_abi {
    MULIB_ELFBUILDER_LINUX_X86_64, MULIB_ELFBUILDER_LINUX_AARCH64,
    MULIB_ELFBUILDER_LINUX_RISCV64, MULIB_ELFBUILDER_FREEBSD_X86_64,
    MULIB_ELFBUILDER_ANDROID_ARMEABI_V7A, MULIB_ELFBUILDER_ANDROID_ARM64_V8A,
    MULIB_ELFBUILDER_ANDROID_X86, MULIB_ELFBUILDER_ANDROID_X86_64
  } mulib_elfbuilder_abi_t;
  typedef struct mulib_elfbuilder_abi_info {
    const char *name;
    uint16_t machine;
    unsigned bits, osabi, stack_alignment, long_double_bits;
    uint32_t flags;
    int reserved_x18;
  } mulib_elfbuilder_abi_info_t;
  /** Query a named little-endian C ABI. Android uses ELFOSABI_NONE, not a
   * fictitious Android OSABI. This is format metadata, not call lowering. */
  int mulib_elfbuilder_abi_info (mulib_elfbuilder_abi_t abi,
      mulib_elfbuilder_abi_info_t *out);
  /** Emit ELF32/64 ET_REL with the ABI's machine, OSABI and e_flags. No loader,
   * dynamic linker, symbols or relocations are implied. Atomic append. */
  int mulib_elfbuilder_finish_abi (const mulib_elfbuilder_t *builder,
      mulib_elfbuilder_abi_t abi, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ELFBUILDER) && !defined(MULIB_ELFBUILDER_IMPLEMENTED)
#define MULIB_ELFBUILDER_IMPLEMENTED
#include "mulib-binfilewriter.h"
#include "mulib-bycutils.h"
#include <string.h>
int mulib_elfbuilder_abi_info (mulib_elfbuilder_abi_t abi, mulib_elfbuilder_abi_info_t *out)
{
  static const mulib_elfbuilder_abi_info_t table[] = {
    {"linux-x86_64-sysv", 62, 64, 0, 16, 80, 0, 0},
    {"linux-aarch64-aapcs64", 183, 64, 0, 16, 128, 0, 0},
    {"linux-riscv64-lp64d", 243, 64, 0, 16, 128, 4, 0},
    {"freebsd-x86_64", 62, 64, 9, 16, 80, 0, 0},
    {"armeabi-v7a", 40, 32, 0, 8, 64, 0x05000200u, 0},
    {"arm64-v8a", 183, 64, 0, 16, 128, 0, 1},
    {"x86", 3, 32, 0, 16, 64, 0, 0},
    {"x86_64", 62, 64, 0, 16, 128, 0, 0}
  };
  if (!out || (unsigned)abi >= sizeof (table) / sizeof (*table)) return 0;
  *out = table[(unsigned)abi]; return 1;
}
int mulib_elfbuilder_finish_abi (const mulib_elfbuilder_t *b,
    mulib_elfbuilder_abi_t abi, mulib_buffer_t *out)
{
  mulib_elfbuilder_abi_info_t a;
  mulib_buffer_t tmp = {0}, names = {0}, headers = {0};
  unsigned char h[52] = {0};
  size_t i, j, table;
  int ok = 0;
  if (!b || !out || !mulib_elfbuilder_abi_info (abi, &a)
      || b->count > 65277 || (b->count && !b->sections)) return 0;
  if (a.bits == 64)
    {
      if (mulib_objfilebuilder_finish (b, a.machine, &tmp))
        {
          tmp.data[7] = (unsigned char)a.osabi;
          mulib_binfilewriter_patch (&tmp, 48, 4, a.flags);
          ok = mulib_buffer_append (out, tmp.data, tmp.length);
        }
      mulib_buffer_free (&tmp); return ok;
    }
  memcpy (h, "\177ELF\1\1\1", 7); h[7] = (unsigned char)a.osabi;
  if (!mulib_buffer_append (&tmp, h, 52)
      || !mulib_buffer_append (&headers, h + 8, 40)
      || !mulib_buffer_append (&names, "", 1)) goto done;
  for (i = 0; i < b->count; ++i)
    {
      const mulib_objfilewriter_section_t *s = &b->sections[i];
      size_t name = names.length;
      if (!s->name || !*s->name || !strcmp (s->name, ".shstrtab")
          || !s->alignment || s->alignment > UINT32_MAX
          || (s->alignment & (s->alignment - 1)) || (s->flags & ~UINT64_C (7))
          || (s->size && !s->data) || s->size > UINT32_MAX) goto done;
      for (j = 0; j < i; ++j) if (!strcmp (s->name, b->sections[j].name)) goto done;
      if (name > UINT32_MAX || !mulib_buffer_append (&names, s->name, strlen (s->name) + 1)
          || !mulib_binfilewriter_align (&tmp, s->alignment)
          || tmp.length > UINT32_MAX || s->size > UINT32_MAX - tmp.length) goto done;
      memset (h, 0, 40);
      mulib_bycutils_store (h, 40, 0, 4, name); mulib_bycutils_store (h, 40, 4, 4, 1);
      mulib_bycutils_store (h, 40, 8, 4, s->flags); mulib_bycutils_store (h, 40, 16, 4, tmp.length);
      mulib_bycutils_store (h, 40, 20, 4, s->size); mulib_bycutils_store (h, 40, 32, 4, s->alignment);
      if (!mulib_buffer_append (&headers, h, 40) || !mulib_buffer_append (&tmp, s->data, s->size)) goto done;
    }
  memset (h, 0, 40);
  mulib_bycutils_store (h, 40, 0, 4, names.length); mulib_bycutils_store (h, 40, 4, 4, 3);
  mulib_bycutils_store (h, 40, 16, 4, tmp.length); mulib_bycutils_store (h, 40, 32, 4, 1);
  if (!mulib_buffer_append (&names, ".shstrtab", 10) || names.length > UINT32_MAX
      || tmp.length > UINT32_MAX - names.length) goto done;
  mulib_bycutils_store (h, 40, 20, 4, names.length);
  if (!mulib_buffer_append (&headers, h, 40) || !mulib_buffer_append (&tmp, names.data, names.length)
      || !mulib_binfilewriter_align (&tmp, 4) || tmp.length > UINT32_MAX
      || headers.length > UINT32_MAX - tmp.length) goto done;
  table = tmp.length;
  if (!mulib_buffer_append (&tmp, headers.data, headers.length)) goto done;
  mulib_binfilewriter_patch (&tmp, 16, 2, 1); mulib_binfilewriter_patch (&tmp, 18, 2, a.machine);
  mulib_binfilewriter_patch (&tmp, 20, 4, 1); mulib_binfilewriter_patch (&tmp, 32, 4, table);
  mulib_binfilewriter_patch (&tmp, 36, 4, a.flags); mulib_binfilewriter_patch (&tmp, 40, 2, 52);
  mulib_binfilewriter_patch (&tmp, 46, 2, 40); mulib_binfilewriter_patch (&tmp, 48, 2, b->count + 2);
  mulib_binfilewriter_patch (&tmp, 50, 2, b->count + 1);
  ok = mulib_buffer_append (out, tmp.data, tmp.length);
done:
  mulib_buffer_free (&tmp); mulib_buffer_free (&names); mulib_buffer_free (&headers); return ok;
}
int
mulib_elfbuilder_add (mulib_elfbuilder_t *b, const char *name, const void *p,
                      size_t n, size_t a, uint64_t flags)
{
  return mulib_objfilebuilder_add (b, name, p, n, a, flags);
}
int
mulib_elfbuilder_finish (const mulib_elfbuilder_t *b, uint16_t machine,
                         mulib_buffer_t *out)
{
  return mulib_objfilebuilder_finish (b, machine, out);
}
void
mulib_elfbuilder_free (mulib_elfbuilder_t *b)
{
  mulib_objfilebuilder_free (b);
}
#endif
