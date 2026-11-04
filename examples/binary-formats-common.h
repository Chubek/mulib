/* Dependency-complete implementation fixture for the binary format examples.
 * Applications should enable only the modules they actually use. */
#ifndef MULIB_EXAMPLE_BINARY_FORMATS_COMMON_H
#define MULIB_EXAMPLE_BINARY_FORMATS_COMMON_H
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_OBJFILEBUILDER
#define MULIB_IMPL_ELFBUILDER
#define MULIB_IMPL_COFFBUILDER
#define MULIB_IMPL_PEBUILDER
#define MULIB_IMPL_MACHOBUILDER
#define MULIB_IMPL_ELF
#define MULIB_IMPL_COFF
#define MULIB_IMPL_PE
#define MULIB_IMPL_MACHO
#define MULIB_IMPL_COFFUTILS
#define MULIB_IMPL_PEUTILS
#define MULIB_IMPL_MACHOUTILS
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_OBJDUMP
#define MULIB_IMPL_ELF_OBJDUMP
#define MULIB_IMPL_COFF_OBJDUMP
#define MULIB_IMPL_PE_OBJDUMP
#define MULIB_IMPL_MACHO_OBJDUMP
#define MULIB_IMPL_CONTAINER
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_IMGFORMAT
#define MULIB_IMPL_CONTAINER_PACKCOFF
#define MULIB_IMPL_CONTAINER_PACKPE
#define MULIB_IMPL_CONTAINER_PACKMACHO
#include "mulib-elfbuilder.h"
#include "mulib-coffbuilder.h"
#include "mulib-pebuilder.h"
#include "mulib-machobuilder.h"
#include "mulib-coffutils.h"
#include "mulib-peutils.h"
#include "mulib-machoutils.h"
#include "mulib-elf-objdump.h"
#include "mulib-coff-objdump.h"
#include "mulib-pe-objdump.h"
#include "mulib-macho-objdump.h"
#include "mulib-container-packcoff.h"
#include "mulib-container-packpe.h"
#include "mulib-container-packmacho.h"
#include <assert.h>
static inline void example_binary_image (mulib_ct_blob *image)
{
  mulib_container_spec spec;
  assert (!mulib_container_spec_init (&spec, "binary-example"));
  assert (!mulib_imgformat_pack (&spec, MULIB_IMGFORMAT_PORTABLE, "", 0, image, 65536));
  mulib_container_spec_free (&spec);
}
static inline void example_binary_write (int argc, char **argv, const void *data, size_t n)
{
  if (argc == 2) {
    FILE *f = fopen (argv[1], "wb");
    assert (f && fwrite (data, 1, n, f) == n); assert (!fclose (f));
  }
}
#endif
