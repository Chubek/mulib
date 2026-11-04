#include "../examples/binary-formats-common.h"

static void test_coff (void)
{
  unsigned char p[147] = { 0 };
  mulib_coff_t v, before;
  mulib_coff_symbol_t sym;
  mulib_coff_section_t sec;
  const char *name;
  const unsigned char *bytes;
  size_t len, index = 99;
  p[0] = 0x64; p[1] = 0x86; p[2] = 1;
  mulib_endian_store_le32 (p + 8, 64); mulib_endian_store_le32 (p + 12, 3);
  memcpy (p + 20, "/4", 2); mulib_endian_store_le32 (p + 36, 4);
  mulib_endian_store_le32 (p + 40, 60); mulib_endian_store_le32 (p + 56, 0x40000040);
  memcpy (p + 60, "data", 4); memcpy (p + 64, "short", 5); p[76] = 1; p[81] = 1;
  mulib_endian_store_le32 (p + 104, 17); p[112] = 1;
  mulib_endian_store_le32 (p + 118, 29); memcpy (p + 122, ".longsection\0long_symbol", 25);
  assert (!mulib_coff_open (&v, p, sizeof (p)));
  assert (!mulib_coff_section_name (&v, 0, &name, &len) && len == 12 && !memcmp (name, ".longsection", len));
  assert (!mulib_coffutils_section (&v, ".longsection", &index, &sec) && index == 0);
  assert (!mulib_coffutils_symbol (&v, "long_symbol", &sym) && sym.section_index == 1);
  assert (mulib_coff_symbol (&v, 1, &sym) == MULIB_COFF_EFORMAT);
  assert (mulib_coffutils_symbol (&v, "missing", &sym) == MULIB_COFF_ENOTFOUND);
  assert (!mulib_coff_bytes (&v, sizeof (p), 0, &bytes) && bytes == p + sizeof (p));
  assert (mulib_coff_bytes (&v, UINT64_MAX, 1, &bytes));
  before = v; assert (mulib_coff_open (&v, p, sizeof (p) - 1)); assert (!memcmp (&v, &before, sizeof (v)));
  p[81] = 3; assert (mulib_coff_open (&v, p, sizeof (p))); p[81] = 1;
  p[146] = 'x'; assert (!mulib_coff_open (&v, p, sizeof (p)));
  assert (mulib_coff_symbol_name (&v, 2, &name, &len) == MULIB_COFF_EFORMAT);
  p[146] = 0; p[59] |= 1; assert (mulib_coff_open (&v, p, sizeof (p)) == MULIB_COFF_EUNSUPPORTED);
  assert (mulib_coff_open (NULL, p, sizeof (p)) == MULIB_COFF_EINVAL);
}

static void test_pe (void)
{
  mulib_buffer_t b = { 0 };
  mulib_pe_t v, saved;
  mulib_pe_section_t sec;
  mulib_pe_directory_t d;
  const unsigned char *bytes;
  unsigned char pe32[512] = { 0 };
  assert (mulib_pebuilder_text (&b, "\xc3", 1, 0));
  assert (!mulib_pe_open (&v, b.data, b.length));
  assert (!mulib_peutils_section (&v, ".text", NULL, &sec));
  assert (!mulib_pe_rva (&v, v.entry, 1, &bytes) && bytes[0] == 0xc3);
  assert (!mulib_pe_rva (&v, 0, 2, &bytes) && bytes[0] == 'M');
  assert (mulib_pe_rva (&v, 4096, SIZE_MAX, &bytes));
  assert (mulib_pe_rva (&v, 0x2000, 1, &bytes));
  mulib_endian_store_le32 (b.data + 400, 1024);
  assert (!mulib_pe_open (&v, b.data, b.length));
  assert (mulib_pe_rva (&v, 4096 + 512, 1, &bytes)); /* virtual zero-fill */
  assert (!mulib_pe_directory (&v, 4, &d) && !d.size);
  saved = v; assert (mulib_pe_open (&v, b.data, 511)); assert (!memcmp (&v, &saved, sizeof (v)));
  mulib_endian_store_le32 (b.data + 260, 17); assert (mulib_pe_open (&v, b.data, b.length));
  /* Empty PE32 with no sections/directories, independent of PE32+ builder. */
  memcpy (pe32, "MZ", 2); mulib_endian_store_le32 (pe32 + 60, 64); memcpy (pe32 + 64, "PE\0\0", 4);
  pe32[68] = 0x4c; pe32[69] = 1; pe32[84] = 96; pe32[88] = 0x0b; pe32[89] = 1;
  mulib_endian_store_le32 (pe32 + 116, 0x400000); mulib_endian_store_le32 (pe32 + 120, 4096);
  mulib_endian_store_le32 (pe32 + 124, 512); mulib_endian_store_le32 (pe32 + 144, 4096);
  mulib_endian_store_le32 (pe32 + 148, 512);
  assert (!mulib_pe_open (&v, pe32, sizeof (pe32)) && v.bits == 32 && v.image_base == 0x400000);
  mulib_buffer_free (&b);
}

static void test_macho (void)
{
  mulib_buffer_t b = { 0 };
  mulib_macho_t v;
  mulib_macho_section_t s;
  mulib_macho_symbol_t sym;
  const char *name; size_t len;
  unsigned char p[156] = { 0 }, symbols[72] = { 0 };
  assert (mulib_machobuilder_text (&b, "\xc3", 1));
  assert (!mulib_macho_open (&v, b.data, b.length) && v.bits == 64 && v.little_endian);
  assert (!mulib_machoutils_section (&v, "__TEXT", "__text", NULL, &s));
  assert (mulib_machoutils_section (&v, "__DATA", "__text", NULL, &s) == MULIB_MACHO_ENOTFOUND);
  mulib_endian_store_le32 (b.data + 36, 0); assert (mulib_macho_open (&v, b.data, b.length));
  /* Big-endian 32-bit segment/section. */
  mulib_endian_store_be32 (p, 0xfeedface); mulib_endian_store_be32 (p + 4, 18);
  mulib_endian_store_be32 (p + 12, 1); mulib_endian_store_be32 (p + 16, 1);
  mulib_endian_store_be32 (p + 20, 124); mulib_endian_store_be32 (p + 28, 1);
  mulib_endian_store_be32 (p + 32, 124); mulib_endian_store_be32 (p + 56, 4);
  mulib_endian_store_be32 (p + 60, 152); mulib_endian_store_be32 (p + 64, 4);
  mulib_endian_store_be32 (p + 76, 1); memcpy (p + 84, "1234567890123456", 16);
  memcpy (p + 100, "__DATA", 6); mulib_endian_store_be32 (p + 120, 4);
  mulib_endian_store_be32 (p + 124, 152); memcpy (p + 152, "abcd", 4);
  assert (!mulib_macho_open (&v, p, sizeof (p)) && v.bits == 32 && !v.little_endian);
  assert (!mulib_macho_section_name (&v, 0, &name, &len) && len == 16);
  assert (!mulib_macho_section (&v, 0, &s) && s.size == 4 && s.name[16] == 0);
  mulib_endian_store_be32 (p + 120, UINT32_MAX); assert (mulib_macho_open (&v, p, sizeof (p)));
  /* Little-endian 32-bit nlist with one absolute symbol. */
  mulib_endian_store_le32 (symbols, 0xfeedface); mulib_endian_store_le32 (symbols + 12, 1);
  mulib_endian_store_le32 (symbols + 16, 1); mulib_endian_store_le32 (symbols + 20, 24);
  mulib_endian_store_le32 (symbols + 28, 2); mulib_endian_store_le32 (symbols + 32, 24);
  mulib_endian_store_le32 (symbols + 36, 52); mulib_endian_store_le32 (symbols + 40, 1);
  mulib_endian_store_le32 (symbols + 44, 64); mulib_endian_store_le32 (symbols + 48, 8);
  mulib_endian_store_le32 (symbols + 52, 1); symbols[56] = 3;
  mulib_endian_store_le32 (symbols + 60, 42); memcpy (symbols + 65, "answer", 7);
  assert (!mulib_macho_open (&v, symbols, sizeof (symbols)));
  assert (!mulib_machoutils_symbol (&v, "answer", &sym) && sym.value == 42);
  symbols[71] = 'x'; assert (mulib_macho_symbol_name (&v, 0, &name, &len));
  mulib_buffer_free (&b);
}

static void test_builders_and_dumps (void)
{
  mulib_coffbuilder_t coff = { 0 }; mulib_pebuilder_t pe = { 0 }; mulib_machobuilder_t macho = { 0 };
  mulib_elfbuilder_t elf = { 0 };
  mulib_buffer_t b = { 0 }; mulib_strbuf_t text = { 0 };
  mulib_objdump_options_t o;
  mulib_coff_t cv; mulib_pe_t pv; mulib_macho_t mv;
  size_t i;
  assert (mulib_coffbuilder_add (&coff, ".text", "\xc3", 1, 16, 6));
  assert (!mulib_coffbuilder_add (&coff, ".text", "", 0, 1, 0));
  assert (!mulib_coffbuilder_add (&coff, "too-long-name", "", 0, 1, 0));
  assert (!mulib_coffbuilder_add (&coff, ".bad", "", 0, 3, 0));
  assert (mulib_coffbuilder_add (&coff, ".data", "ab\033", 3, 8, 3));
  assert (mulib_coffbuilder_finish (&coff, 0x8664, &b));
  assert (!mulib_coff_open (&cv, b.data, b.length) && cv.section_count == 2);
  mulib_objdump_options_init (&o); o.flags = MULIB_OBJDUMP_ALL; o.max_bytes = 1;
  assert (mulib_coff_objdump (b.data, b.length, &o, &text));
  assert (strstr (text.data, "COFF") && strstr (text.data, "truncated") && !strchr (text.data, '\033'));
  mulib_strbuf_free (&text); assert (mulib_strbuf_append (&text, "unchanged"));
  o.max_records = 1; assert (!mulib_coff_objdump (b.data, b.length, &o, &text) && !strcmp (text.data, "unchanged"));
  o.max_records = 4096;
  /* Input may alias an output being grown. */
  assert (mulib_coffbuilder_text (&b, b.data, b.length));
  mulib_buffer_free (&b);
  assert (mulib_pebuilder_add (&pe, ".data", "data", 4, 8, 3));
  assert (mulib_pebuilder_add (&pe, ".empty", NULL, 0, 1, 0));
  assert (mulib_pebuilder_finish (&pe, 0x8664, &b));
  assert (!mulib_pe_open (&pv, b.data, b.length) && pv.coff.section_count == 2);
  assert (mulib_pe_objdump (b.data, b.length, &o, &text)); mulib_buffer_free (&b);
  assert (mulib_machobuilder_add (&macho, "__data", "abcd", 4, 32, 3));
  assert (mulib_machobuilder_add (&macho, "__empty", NULL, 0, 16, 0));
  assert (mulib_machobuilder_finish (&macho, 0x0100000c, &b));
  assert (!mulib_macho_open (&mv, b.data, b.length) && mv.section_count == 2);
  assert (mulib_macho_objdump (b.data, b.length, &o, &text));
  for (i = 0; i < b.length; ++i) {
    unsigned char old = b.data[i];
    b.data[i] ^= 0xff;
    (void)mulib_macho_objdump (b.data, b.length, &o, &text);
    b.data[i] = old; mulib_strbuf_free (&text);
  }
  mulib_buffer_free (&b);
  assert (mulib_elfbuilder_add (&elf, ".bad\033name", "\xc3", 1, 16, 6));
  assert (mulib_elfbuilder_finish (&elf, 62, &b));
  assert (mulib_elf_objdump (b.data, b.length, &o, &text));
  assert (strstr (text.data, ".bad\\x1bname") && !strchr (text.data, '\033'));
  mulib_buffer_free (&b); mulib_strbuf_free (&text);
  assert (mulib_objdump_hex (&text, NULL, 0, UINT64_MAX, 0));
  assert (!mulib_objdump_hex (&text, "a", 1, UINT64_MAX, 1));
  mulib_strbuf_free (&text);
  mulib_coffbuilder_free (&coff); mulib_pebuilder_free (&pe);
  mulib_machobuilder_free (&macho); mulib_elfbuilder_free (&elf);
}

typedef int (*pack_object_fn) (const void *, size_t, uint16_t, mulib_ct_blob *, size_t);
typedef int (*pack_append_fn) (const void *, size_t, const void *, size_t, mulib_ct_blob *, size_t);
typedef int (*pack_open_fn) (const void *, size_t, size_t, const unsigned char **, size_t *, mulib_imgformat_view *);
static int pack_macho (const void *p, size_t n, uint16_t ignored, mulib_ct_blob *out, size_t limit)
{ (void)ignored; return mulib_container_packmacho_object (p, n, 0x01000007, out, limit); }
static void test_pack (pack_object_fn object, pack_append_fn append, pack_open_fn open)
{
  mulib_ct_blob image = { 0 }, file = { 0 }, overlay = { 0 };
  mulib_imgformat_view v, saved;
  const unsigned char *p = NULL; size_t n = 99, i;
  example_binary_image (&image);
  assert (!object (image.data, image.length, 0x8664, &file, 65536));
  assert (!open (file.data, file.length, image.length, &p, &n, &v));
  assert (n == image.length && !memcmp (p, image.data, n));
  saved = v; n = 99; p = NULL;
  assert (open (file.data, file.length, image.length - 1, &p, &n, &v) == MULIB_CT_ELIMIT);
  assert (p == NULL && n == 99 && !memcmp (&v, &saved, sizeof (v)));
  assert (object (image.data, image.length, 0x8664, &file, 1) == MULIB_CT_ELIMIT);
  assert (!append (file.data, file.length, image.data, image.length, &overlay, 65536));
  assert (!memcmp (overlay.data, file.data, file.length));
  assert (!open (overlay.data, overlay.length, image.length, &p, &n, &v) && n == image.length);
  overlay.data[overlay.length - 1] ^= 1;
  assert (open (overlay.data, overlay.length, 65536, &p, &n, &v) == MULIB_CT_ECHECKSUM);
  overlay.data[overlay.length - 1] ^= 1; overlay.data[file.length] ^= 1;
  assert (open (overlay.data, overlay.length, 65536, &p, &n, &v) == MULIB_CT_ECHECKSUM);
  overlay.data[file.length] ^= 1;
  /* Replacement is safe with file and image spans aliasing the old output. */
  assert (!append (overlay.data, overlay.length, overlay.data + file.length, image.length, &overlay, 65536));
  assert (!open (overlay.data, overlay.length, 65536, &p, &n, &v));
  for (i = 0; i < file.length; ++i) (void)open (file.data, i, 65536, &p, &n, &v);
  mulib_ct_blob_free (&image); mulib_ct_blob_free (&file); mulib_ct_blob_free (&overlay);
}

static void test_termstyle (void)
{
  mulib_terminfo_t info = { 0 };
  mulib_termstyle_t style;
  mulib_termstyle_spec_t spec = { MULIB_TERMSTYLE_BOLD | MULIB_TERMSTYLE_UNDERLINE, 2, 4 };
  mulib_strbuf_t text = { 0 };
  mulib_termstyle_init (&style, &info, 0);
  assert (mulib_termstyle_text (&style, &spec, "plain", 5, &text) && !strcmp (text.data, "plain"));
  mulib_termstyle_init (&style, &info, 1);
  assert (!mulib_termstyle_apply (&style, &spec, &text) && !strcmp (text.data, "plain"));
  mulib_terminfo_ansi (&info);
  mulib_strbuf_free (&text);
  assert (mulib_termstyle_text (&style, &spec, "ok", 2, &text));
  assert (!strcmp (text.data, "\033[0m\033[1m\033[4m\033[38;5;2m\033[48;5;4mok\033[0m"));
  spec.foreground = 256;
  { size_t n = text.length; assert (!mulib_termstyle_apply (&style, &spec, &text) && text.length == n); }
  spec.foreground = -1; spec.background = -1;
  assert (mulib_termstyle_text (&style, &spec, text.data, text.length, &text));
  mulib_strbuf_free (&text); mulib_terminfo_free (&info);
  assert (!mulib_coff_objdump (NULL, 0, NULL, &text));
  assert (!mulib_pe_objdump (NULL, 0, NULL, &text));
  assert (!mulib_macho_objdump (NULL, 0, NULL, &text));
  assert (!mulib_elf_objdump (NULL, 0, NULL, &text));
}
static void test_compiled_style (void)
{
  unsigned char record[782] = { 0 };
  mulib_terminfo_t info = { 0 };
  mulib_termstyle_t style;
  mulib_termstyle_spec_t spec = { MULIB_TERMSTYLE_BOLD, 2, 3 };
  mulib_strbuf_t out = { 0 };
  size_t i;
  record[0] = 0x1a; record[1] = 1; record[2] = 2; record[6] = 14;
  record[8] = 0x69; record[9] = 1; record[10] = 18; record[12] = 't';
  record[14 + 13 * 2] = 8;
  for (i = 0; i < 361; ++i) { record[42 + i * 2] = 255; record[43 + i * 2] = 255; }
  record[42 + 39 * 2] = 0; record[43 + 39 * 2] = 0;
  record[42 + 27 * 2] = 2; record[43 + 27 * 2] = 0;
  record[42 + 359 * 2] = 4; record[43 + 359 * 2] = 0;
  record[42 + 360 * 2] = 11; record[43 + 360 * 2] = 0;
  memcpy (record + 764, "R\0B\0F%p1%d\0G%p1%d", 18);
  assert (mulib_terminfo_load (&info, record, sizeof (record)));
  mulib_termstyle_init (&style, &info, 1);
  assert (mulib_termstyle_text (&style, &spec, "x", 1, &out));
  assert (!strcmp (out.data, "RBF2G3xR")); /* actual record, not hardcoded ANSI */
  spec.attributes |= MULIB_TERMSTYLE_ITALIC;
  assert (!mulib_termstyle_apply (&style, &spec, &out) && !strcmp (out.data, "RBF2G3xR"));
  mulib_strbuf_free (&out); mulib_terminfo_free (&info);
}
static void test_signed_overlays (void)
{
  mulib_ct_blob image = { 0 }, object = { 0 }, out = { 0 };
  example_binary_image (&image);
  assert (!mulib_container_packpe_object (image.data, image.length, 0x8664, &object, 65536));
  mulib_ct_store32 (object.data + 264 + 4 * 8, 512);
  mulib_ct_store32 (object.data + 268 + 4 * 8, 8);
  assert (mulib_container_packpe_append (object.data, object.length, image.data, image.length, &out, 65536) == MULIB_CT_ENOTSUP);
  mulib_ct_blob_free (&object);
  assert (!mulib_container_packmacho_object (image.data, image.length, 0x01000007, &object, 65536));
  /* Replace the segment with one opaque aligned code-signature command. */
  mulib_ct_store32 (object.data + 32, 0x1d);
  assert (mulib_container_packmacho_append (object.data, object.length, image.data, image.length, &out, 65536) == MULIB_CT_ENOTSUP);
  mulib_ct_blob_free (&image); mulib_ct_blob_free (&object); mulib_ct_blob_free (&out);
}
int main (void)
{
  test_coff (); test_pe (); test_macho (); test_builders_and_dumps (); test_termstyle ();
  test_compiled_style (); test_signed_overlays ();
  test_pack (mulib_container_packcoff_object, mulib_container_packcoff_append, mulib_container_packcoff_open);
  test_pack (mulib_container_packpe_object, mulib_container_packpe_append, mulib_container_packpe_open);
  test_pack (pack_macho, mulib_container_packmacho_append, mulib_container_packmacho_open);
  return 0;
}
