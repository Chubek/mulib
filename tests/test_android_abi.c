#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_OBJFILEBUILDER
#define MULIB_IMPL_ELFBUILDER
#define MULIB_IMPL_COFFBUILDER
#define MULIB_IMPL_PEBUILDER
#define MULIB_IMPL_MACHOBUILDER
#include "mulib-elfbuilder.h"
#include "mulib-coffbuilder.h"
#include "mulib-pebuilder.h"
#include "mulib-machobuilder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t get (const mulib_buffer_t *b, size_t at, unsigned width)
{ uint64_t v = 0; assert (mulib_bycutils_load (b->data, b->length, at, width, &v)); return v; }
static void save (const char *dir, const char *prefix, unsigned index, const mulib_buffer_t *b)
{
  if (dir) {
    char path[4096]; FILE *f;
    assert (snprintf (path, sizeof (path), "%s/%s-%u.bin", dir, prefix, index) < (int)sizeof (path));
    f = fopen (path, "wb"); assert (f); assert (fwrite (b->data, 1, b->length, f) == b->length); assert (!fclose (f));
  }
}
int main (int argc, char **argv)
{
  const unsigned char ret[] = {0xc0, 3, 0x5f, 0xd6};
  mulib_objfilebuilder_t builder = {0}; mulib_buffer_t b = {0};
  const char *dir = argc == 2 ? argv[1] : NULL;
  unsigned k;
  assert (mulib_objfilebuilder_add (&builder, ".text", ret, sizeof (ret), 16, 6));
  for (k = 0; k <= MULIB_ELFBUILDER_ANDROID_X86_64; ++k) {
    mulib_elfbuilder_abi_info_t a;
    assert (mulib_elfbuilder_abi_info ((mulib_elfbuilder_abi_t)k, &a));
    assert (mulib_elfbuilder_finish_abi (&builder, (mulib_elfbuilder_abi_t)k, &b));
    assert (!memcmp (b.data, "\177ELF", 4) && b.data[4] == (a.bits == 64 ? 2 : 1) && b.data[5] == 1 && b.data[7] == a.osabi);
    assert (get (&b, 18, 2) == a.machine && get (&b, a.bits == 64 ? 48 : 36, 4) == a.flags);
    if (k >= MULIB_ELFBUILDER_ANDROID_ARMEABI_V7A) assert (!a.osabi);
    if (k == MULIB_ELFBUILDER_ANDROID_ARM64_V8A) assert (a.reserved_x18 && a.long_double_bits == 128);
    save (dir, "elf", k, &b); mulib_buffer_free (&b);
  }
  assert (!mulib_elfbuilder_finish_abi (&builder, (mulib_elfbuilder_abi_t)999, &b) && !b.length);
  for (k = 0; k <= MULIB_COFFBUILDER_MS_ARM64; ++k) {
    static const uint16_t machines[] = {0x14c,0x8664,0x1c4,0xaa64};
    assert (mulib_coffbuilder_finish_abi (&builder, (mulib_coffbuilder_abi_t)k, &b));
    assert (get (&b, 0, 2) == machines[k] && get (&b, 2, 2) == 1);
    save (dir, "coff", k, &b); mulib_buffer_free (&b);
  }
  for (k = 0; k <= MULIB_PEBUILDER_UEFI_ARM64; ++k) {
    assert (mulib_pebuilder_text_abi (&b, ret, sizeof (ret), 0, (mulib_pebuilder_abi_t)k));
    assert (get (&b, 132, 2) == ((k & 1) ? 0xaa64u : 0x8664u));
    assert (get (&b, 152, 2) == 0x20b && get (&b, 220, 2) == (k >= 2 ? 10u : 3u));
    save (dir, "pe", k, &b); mulib_buffer_free (&b);
  }
  assert (!mulib_pebuilder_text_abi (&b, ret, sizeof (ret), 1, MULIB_PEBUILDER_WINDOWS_ARM64) && !b.length);
  assert (mulib_pebuilder_finish_abi (&builder, MULIB_PEBUILDER_WINDOWS_ARM64, &b));
  assert (get (&b, 132, 2) == 0xaa64 && get (&b, 150, 2) == 0x2023); mulib_buffer_free (&b);
  assert (!mulib_pebuilder_finish_abi (&builder, MULIB_PEBUILDER_UEFI_ARM64, &b));
  for (k = 0; k <= MULIB_MACHOBUILDER_IOS_SIM_ARM64; ++k) {
    size_t at;
    assert (mulib_machobuilder_finish_abi (&builder, (mulib_machobuilder_abi_t)k, 0x0e0000, 0, &b));
    assert (get (&b, 0, 4) == 0xfeedfacf && get (&b, 16, 4) == 2 && get (&b, 20, 4) == 176);
    at = 32 + (size_t)get (&b, 36, 4);
    assert (get (&b, at, 4) == 0x32 && get (&b, at + 4, 4) == 24);
    assert (get (&b, at + 8, 4) == (k < 2 ? 1u : k == 2 ? 2u : 7u));
    assert (get (&b, 104 + 48, 4) >= at + 24);
    save (dir, "macho", k, &b); mulib_buffer_free (&b);
  }
  assert (!mulib_machobuilder_finish_abi (&builder, (mulib_machobuilder_abi_t)-1, 0xe0000, 0, &b));
  mulib_objfilebuilder_free (&builder);
  return 0;
}
