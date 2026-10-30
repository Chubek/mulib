#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ELF
#define MULIB_IMPL_ELFUTILS
#define MULIB_IMPL_BINFILE
#define MULIB_IMPL_BINFILESCHEMA
#define MULIB_IMPL_BINFILEREADER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BINFILEBUILDER
#define MULIB_IMPL_OBJFILE
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_OBJFILEBUILDER
#define MULIB_IMPL_OBJFILEINJECTOR
#define MULIB_IMPL_OBJFILEPACKER
#define MULIB_IMPL_OBJFILELIBSEARCH
#define MULIB_IMPL_OBJFILERELOC
#define MULIB_IMPL_OBJFILEPOSINDEP
#define MULIB_IMPL_OBJFILESTUB
#define MULIB_IMPL_OBJFILESYMLOC
#define MULIB_IMPL_OBJFILELEADER
#define MULIB_IMPL_FLATBUFFERS
#include "mulib-binfilebuilder.h"
#include "mulib-binfilereader.h"
#include "mulib-flatbuffers.h"
#include "mulib-objfile.h"
#include "mulib-objfilebuilder.h"
#include "mulib-objfileinjector.h"
#include "mulib-objfileleader.h"
#include "mulib-objfilelibsearch.h"
#include "mulib-objfilepacker.h"
#include "mulib-objfilereloc.h"
#include "mulib-objfilestub.h"
#include "mulib-objfilesymloc.h"
#include <assert.h>

static void
symbol_and_load (void)
{
  mulib_buffer_t b = { 0 };
  unsigned char symbols[48] = { 0 }, executable[124] = { 0 }, memory[8];
  mulib_objfilewriter_section_t sections[]
      = { { ".data", "xyz", 3, 1, 3 },
          { ".strtab", "\0name\0", 6, 1, 0 },
          { ".symtab", symbols, sizeof (symbols), 8, 0 } };
  mulib_elf_t elf;
  mulib_elf_section_t symtab;
  mulib_binfile_t span;
  uint64_t table;
  /* Construct a real SYMTAB over writer-produced sections, using wire fields.
   */
  assert (mulib_bycutils_store (symbols, 48, 24, 4, 1));
  symbols[28] = 0x10;
  assert (mulib_bycutils_store (symbols, 48, 30, 2, 1));
  assert (mulib_bycutils_store (symbols, 48, 32, 8, 1));
  assert (mulib_bycutils_store (symbols, 48, 40, 8, 2));
  assert (mulib_objfilewriter_write (&b, 62, sections, 3));
  assert (mulib_bycutils_load (b.data, b.length, 40, 8, &table));
  assert (mulib_bycutils_store (b.data, b.length, (size_t)table + 2 * 64 + 4,
                                4, 3));
  assert (mulib_bycutils_store (b.data, b.length, (size_t)table + 3 * 64 + 4,
                                4, 2));
  assert (mulib_bycutils_store (b.data, b.length, (size_t)table + 3 * 64 + 40,
                                4, 2));
  assert (mulib_bycutils_store (b.data, b.length, (size_t)table + 3 * 64 + 44,
                                4, 1));
  assert (mulib_bycutils_store (b.data, b.length, (size_t)table + 3 * 64 + 56,
                                8, 24));
  assert (!mulib_elf_open (&elf, b.data, b.length));
  assert (!mulib_objfilesymloc_find (&elf, "name", &span) && span.size == 2
          && !memcmp (span.data, "yz", 2));
  assert (!mulib_elf_section (&elf, 3, &symtab));
  assert (mulib_bycutils_store (b.data, b.length, (size_t)symtab.offset + 40,
                                8, 4));
  assert (mulib_objfilesymloc_find (&elf, "name", &span) == MULIB_ELF_EBOUNDS);
  mulib_buffer_free (&b);

  memcpy (executable, "\177ELF\2\1\1", 7);
  assert (mulib_bycutils_store (executable, 124, 16, 2, 2));
  assert (mulib_bycutils_store (executable, 124, 18, 2, 62));
  assert (mulib_bycutils_store (executable, 124, 20, 4, 1));
  assert (mulib_bycutils_store (executable, 124, 32, 8, 64));
  assert (mulib_bycutils_store (executable, 124, 52, 2, 64));
  assert (mulib_bycutils_store (executable, 124, 54, 2, 56));
  assert (mulib_bycutils_store (executable, 124, 56, 2, 1));
  assert (mulib_bycutils_store (executable, 124, 64, 4, 1));
  assert (mulib_bycutils_store (executable, 124, 68, 4, 6));
  assert (mulib_bycutils_store (executable, 124, 72, 8, 120));
  assert (mulib_bycutils_store (executable, 124, 80, 8, 4096));
  assert (mulib_bycutils_store (executable, 124, 96, 8, 4));
  assert (mulib_bycutils_store (executable, 124, 104, 8, 8));
  assert (mulib_bycutils_store (executable, 124, 112, 8, 1));
  memcpy (executable + 120, "DATA", 4);
  assert (!mulib_elf_open (&elf, executable, 124));
  memset (memory, 0xaa, sizeof (memory));
  assert (mulib_objfileleader_load (&elf, 4096, memory, 7) == MULIB_ELF_EBOUNDS
          && memory[0] == 0xaa);
  assert (!mulib_objfileleader_load (&elf, 4096, memory, 8));
  assert (!memcmp (memory, "DATA\0\0\0\0", 8));
}

int
main (void)
{
  symbol_and_load ();
  const mulib_binfileschema_field_t fields[]
      = { { "a", 0, 2 }, { "b", 4, 4 } };
  mulib_binfileschema_t schema = { fields, 2, 8 };
  uint64_t values[] = { 65535, 123 }, v = 0;
  mulib_buffer_t b = { 0 }, obj = { 0 }, ar = { 0 }, stub = { 0 };
  mulib_binfilereader_t r;
  mulib_binfile_t span;
  mulib_objfilebuilder_t builder = { 0 };
  mulib_objfile_t view;
  mulib_elf_section_t section;
  mulib_elf_symbol_t symbol;
  size_t i, index;
  int32_t delta;
  unsigned char fb[36] = { 0 };
  mulib_flatbuffers_table_t table;
  assert (mulib_binfileschema_validate (&schema));
  assert (mulib_binfilebuilder_record (&b, &schema, values, 2)
          && b.length == 8);
  values[0] = 65536;
  assert (!mulib_binfilebuilder_record (&b, &schema, values, 2)
          && b.length == 8);
  assert (mulib_binfileschema_get (&schema, b.data, b.length, "b", &v)
          && v == 123);
  assert (mulib_binfilereader_init (&r, b.data, b.length));
  assert (mulib_binfilereader_uint (&r, 2, &v) && v == 65535);
  assert (!mulib_binfilereader_uint (&r, 8, &v) && r.position == 2);
  assert (mulib_binfilereader_seek (&r, 8)
          && mulib_binfilereader_bytes (&r, 0, &span));
  assert (mulib_objfilebuilder_add (&builder, ".text", "abc", 3, 16, 6));
  assert (!mulib_objfilebuilder_add (&builder, ".text", "", 0, 1, 0));
  assert (mulib_objfilebuilder_finish (&builder, 62, &obj));
  assert (!mulib_objfile_open (&view, obj.data, obj.length));
  assert (view.elf.bits == 64 && view.elf.shnum == 3);
  assert (!mulib_objfile_section (&view, ".text", &span) && span.size == 3
          && !memcmp (span.data, "abc", 3));
  assert (!mulib_elfutils_section (&view.elf, ".text", &index, &section)
          && index == 1 && section.alignment == 16);
  assert (mulib_elfutils_symbol (&view.elf, "missing", &symbol)
          == MULIB_ELF_ENOTFOUND);
  assert (mulib_objfilesymloc_find (&view.elf, "missing", &span)
          == MULIB_ELF_ENOTFOUND);
  assert (mulib_objfileleader_load (&view.elf, 0, b.data, b.length)
          == MULIB_ELF_ETYPE);
  assert (
      mulib_objfileinjector_replace (obj.data, obj.length, ".text", "xy", 2)
      < 0);
  assert (!mulib_objfileinjector_replace (obj.data, obj.length, ".text", "xyz",
                                          3));
  assert (!mulib_objfile_section (&view, ".text", &span)
          && !memcmp (span.data, "xyz", 3));
  {
    mulib_elf_section_t strings;
    uint64_t old_offset = section.offset;
    size_t field = (size_t)view.elf.shoff + view.elf.shentsize + 24;
    unsigned char before[3];
    assert (!mulib_elf_section (&view.elf, view.elf.shstrndx, &strings));
    memcpy (before, obj.data + strings.offset, 3);
    /* A valid ELF range may still alias metadata belonging to another section.
     */
    assert (
        mulib_bycutils_store (obj.data, obj.length, field, 8, strings.offset));
    assert (
        mulib_objfileinjector_replace (obj.data, obj.length, ".text", "BAD", 3)
        == MULIB_ELF_EFORMAT);
    assert (!memcmp (before, obj.data + strings.offset, 3));
    assert (mulib_bycutils_store (obj.data, obj.length, field, 8, old_offset));
    assert (
        mulib_bycutils_store (obj.data, obj.length, 52, 2, old_offset + 3));
    assert (
        mulib_objfileinjector_replace (obj.data, obj.length, ".text", "BAD", 3)
        == MULIB_ELF_EFORMAT);
    assert (!memcmp (obj.data + old_offset, "xyz", 3));
    assert (mulib_bycutils_store (obj.data, obj.length, 52, 2, 64));
  }
  for (i = 0; i < 64; ++i)
    {
      mulib_objfile_t bad;
      assert (mulib_objfile_open (&bad, obj.data, i) < 0);
    }
  assert (mulib_objfilepacker_add (&ar, "one.o", obj.data, obj.length));
  assert (mulib_objfilepacker_add (&ar, "two.o", "a", 1));
  assert (!mulib_objfilelibsearch_find (ar.data, ar.length, "one.o", &span)
          && span.size == obj.length);
  assert (mulib_objfilelibsearch_find (ar.data, ar.length - 1, "one.o", &span)
          < 0);
  assert (mulib_objfileposindep_rel32 (0, 0, 2147483648ULL, &delta)
          && delta == INT32_MIN);
  assert (!mulib_objfileposindep_rel32 (UINT64_MAX, 1, 0, &delta));
  assert (mulib_objfilereloc_apply (b.data, b.length, 0,
                                    MULIB_OBJFILERELOC_PC32, 100, 0, 104));
  assert (!mulib_objfilereloc_apply (
      b.data, b.length, 0, MULIB_OBJFILERELOC_ABS32, UINT64_MAX, 0, 0));
  assert (mulib_objfilestub_x64_jump (&stub, 0x1234) && stub.length == 14
          && stub.data[6] == 0x34);
  /* Root=12, vtable=4: two fields, uint32 at +4, string uoffset at +8. */
  mulib_bycutils_store (fb, sizeof (fb), 0, 4, 12);
  mulib_bycutils_store (fb, sizeof (fb), 4, 2, 8);
  mulib_bycutils_store (fb, sizeof (fb), 6, 2, 12);
  mulib_bycutils_store (fb, sizeof (fb), 8, 2, 4);
  mulib_bycutils_store (fb, sizeof (fb), 10, 2, 8);
  mulib_bycutils_store (fb, sizeof (fb), 12, 4, 8);
  mulib_bycutils_store (fb, sizeof (fb), 16, 4, 42);
  mulib_bycutils_store (fb, sizeof (fb), 20, 4, 4);
  mulib_bycutils_store (fb, sizeof (fb), 24, 4, 3);
  memcpy (fb + 28, "abc", 3);
  assert (mulib_flatbuffers_root (fb, sizeof (fb), &table));
  assert (mulib_flatbuffers_uint (&table, 0, 4, &v) && v == 42);
  assert (mulib_flatbuffers_string (&table, 1, &span) && span.size == 3);
  assert (!mulib_flatbuffers_uint (&table, 99, 4, &v));
  {
    mulib_flatbuffers_table_t forged = table;
    forged.table = SIZE_MAX - 3;
    assert (!mulib_flatbuffers_uint (&forged, 0, 4, &v));
    forged = table;
    forged.vtable = SIZE_MAX - 3;
    assert (!mulib_flatbuffers_string (&forged, 1, &span));
    /* A forward offset into its own four-byte field is malformed. */
    assert (mulib_bycutils_store (fb, sizeof (fb), 20, 4, 1));
    assert (mulib_bycutils_store (fb, sizeof (fb), 24, 4, 0));
    assert (!mulib_flatbuffers_string (&table, 1, &span));
    assert (mulib_bycutils_store (fb, sizeof (fb), 20, 4, 4));
    assert (mulib_bycutils_store (fb, sizeof (fb), 24, 4, 3));
    /* Vtable must not overlap the object, even if both ranges fit the buffer.
     */
    assert (mulib_bycutils_store (fb, sizeof (fb), 4, 2, 10));
    assert (!mulib_flatbuffers_root (fb, sizeof (fb), &forged));
    assert (mulib_bycutils_store (fb, sizeof (fb), 4, 2, 8));
    /* Shared vtables after the object are valid (negative signed distance). */
    {
      unsigned char forward[20] = { 4, 0, 0, 0, 248, 255, 255, 255, 42, 0,
                                    0, 0, 6, 0, 8,   0,   4,   0,   0,  0 };
      assert (mulib_flatbuffers_root (forward, sizeof (forward), &forged));
      assert (mulib_flatbuffers_uint (&forged, 0, 4, &v) && v == 42);
    }
  }
  fb[31] = 1;
  assert (!mulib_flatbuffers_string (&table, 1, &span));
  for (i = 0; i < 24; ++i)
    assert (!mulib_flatbuffers_root (fb, i, &table));
  mulib_objfilebuilder_free (&builder);
  mulib_buffer_free (&b);
  mulib_buffer_free (&obj);
  mulib_buffer_free (&ar);
  mulib_buffer_free (&stub);
  return 0;
}
