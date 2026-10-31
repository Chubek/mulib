/* Deterministic wire mutations, truncations and streaming/roundtrip invariants.
 * Define MULIB_CONTAINER_FUZZ to use LLVMFuzzerTestOneInput with libFuzzer. */
#include "../examples/container-stack-common.h"
#include <stdio.h>

int LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  mulib_container_ipc_message message, streamed;
  mulib_container_ipc_reader reader;
  mulib_container_spec spec;
  mulib_imgformat_view view;
  mulib_ct_blob decoded = { 0 }, packed = { 0 }, roundtrip = { 0 };
  mulib_rootfs_t root;
  const unsigned char *embedded;
  size_t used, first, second = 0, split, length;
  int direct, incremental, rc;
  if (size > 16384) return 0;
  split = size ? data[0] % (size + 1) : 0;
  direct = mulib_container_ipc_decode (data, size, 16384, &message, &used);
  assert (!mulib_container_ipc_reader_init (&reader, 16384));
  incremental = mulib_container_ipc_feed (&reader, data, split, &first);
  assert (first <= split);
  if (incremental == MULIB_CT_AGAIN)
    {
      incremental = mulib_container_ipc_feed (&reader, data + split, size - split, &second);
      assert (second <= size - split);
    }
  if (!direct)
    {
      assert (incremental == MULIB_CT_DONE && first + second == used);
      assert (!mulib_container_ipc_peek (&reader, &streamed));
      assert (streamed.opcode == message.opcode && streamed.flags == message.flags
          && streamed.request_id == message.request_id && streamed.status == message.status
          && streamed.length == message.length);
      assert (!memcmp (streamed.data, message.data, message.length));
    }
  else assert (direct == incremental);
  mulib_container_ipc_reader_free (&reader);

  assert (!mulib_ct_blob_assign (&decoded, "keep", 4, 4));
  rc = mulib_zstd_raw_decode (data, size, &decoded, 32768);
  if (rc) assert (decoded.length == 4 && !memcmp (decoded.data, "keep", 4));
  else
    {
      assert (decoded.length <= 32768);
      assert (!mulib_zstd_raw_encode (decoded.data, decoded.length, &packed, 65536));
      assert (!mulib_zstd_raw_decode (packed.data, packed.length, &roundtrip, 32768));
      assert (roundtrip.length == decoded.length);
      assert (!decoded.length || !memcmp (roundtrip.data, decoded.data, decoded.length));
      /* In-place decode must not invalidate source bytes before publication. */
      assert (!mulib_zstd_raw_decode (packed.data, packed.length, &packed, 32768));
      assert (packed.length == decoded.length);
      assert (!packed.length || !memcmp (packed.data, decoded.data, packed.length));
    }
  mulib_ct_blob_free (&decoded); mulib_ct_blob_free (&packed); mulib_ct_blob_free (&roundtrip);

  if (!mulib_imgformat_spec_decode (data, size, &spec))
    {
      assert (!mulib_container_spec_validate (&spec));
      assert (!mulib_imgformat_spec_encode (&spec, &packed, 32768));
      mulib_container_spec_free (&spec); mulib_ct_blob_free (&packed);
    }
  if (!mulib_imgformat_open (data, size, 16384, &view))
    {
      mulib_image2container image;
      mulib_image2container_limits limits = mulib_image2container_default_limits ();
      limits.image_bytes = 16384; limits.nodes = 32;
      limits.file_bytes = limits.total_file_bytes = 32768;
      if (!mulib_image2container_open (data, size, NULL, &limits, &image, NULL))
        {
          assert (!mulib_container_spec_validate (&image.container.spec));
          mulib_image2container_free (&image);
        }
    }
  (void)mulib_container_packelf_open (data, size, 16384, &embedded, &length, &view);
  if (!mulib_rootfs_import_limited (data, size, 32, 32768, 32768, &root))
    mulib_rootfs_free (&root);
  return 0;
}

#ifndef MULIB_CONTAINER_FUZZ
static void mutations (const unsigned char *data, size_t length)
{
  unsigned char *copy = (unsigned char *)malloc (length ? length : 1);
  uint32_t random = UINT32_C(0x91e10da5);
  size_t i;
  assert (copy);
  for (i = 0; i <= length; ++i) LLVMFuzzerTestOneInput (data, i);
  for (i = 0; i < 1024 && length; ++i)
    {
      size_t at;
      memcpy (copy, data, length);
      random = random * UINT32_C(1664525) + UINT32_C(1013904223);
      at = random % length; copy[at] ^= (unsigned char)(1U << ((random >> 16) & 7));
      /* Preserve checksums on alternate mutations to reach nested parsers. */
      if ((i & 1) && length >= 64 && !memcmp (copy, "MULIMG1\0", 8))
        {
          mulib_ct_store32 (copy + 32, mulib_crc32 (copy + 64, length - 64));
          mulib_ct_store32 (copy + 36, mulib_crc32_update (mulib_crc32 (copy, 36), copy + 40, 24));
        }
      if ((i & 1) && length >= 32 && !memcmp (copy, "MCIP", 4))
        {
          mulib_ct_store32 (copy + 24, mulib_crc32 (copy + 32, length - 32));
          mulib_ct_store32 (copy + 28, mulib_crc32 (copy, 28));
        }
      LLVMFuzzerTestOneInput (copy, length);
    }
  free (copy);
}

int main (void)
{
  mulib_ct_blob image = { 0 }, packed = { 0 }, elf = { 0 }, wire = { 0 };
  mulib_imgformat_view view;
  mulib_container_ipc_message message = { 7, 0, 42, 0, (const unsigned char *)"abc", 3 };
  unsigned char bytes[256];
  uint32_t random = UINT32_C(0xabad1dea);
  size_t i, j;
  assert (!example_container_image (&image));
  assert (!mulib_imgformat_open (image.data, image.length, 16384, &view));
  assert (!mulib_zstd_raw_encode (image.data, image.length, &packed, 16384));
  assert (!mulib_container_packelf_object (image.data, image.length, 62, &elf, 16384));
  assert (!mulib_container_ipc_encode (&message, &wire, 16));
  mutations (image.data, image.length); mutations (view.config, view.config_length);
  mutations (view.rootfs, view.rootfs_length); mutations (packed.data, packed.length);
  mutations (elf.data, elf.length); mutations (wire.data, wire.length);
  for (i = 0; i < 2048; ++i)
    {
      size_t n = i % sizeof (bytes);
      for (j = 0; j < n; ++j)
        {
          random = random * UINT32_C(1664525) + UINT32_C(1013904223);
          bytes[j] = (unsigned char)(random >> 24);
        }
      LLVMFuzzerTestOneInput (bytes, n);
    }
  mulib_ct_blob_free (&image); mulib_ct_blob_free (&packed);
  mulib_ct_blob_free (&elf); mulib_ct_blob_free (&wire);
  puts ("container wire: truncations, mutations, bounded imports and roundtrips passed");
  return 0;
}
#endif
