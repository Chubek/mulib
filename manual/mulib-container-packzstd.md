# mulib-container-packzstd

## NAME AND INTEGRATION

Standard Zstandard raw/RLE packaging for validated container images.
Define `MULIB_IMPL_CONTAINER_PACKZSTD` and the `imgformat` closure. The implemented
`include-mulib-zstd.h` dependency is inline. Example:
`examples/modules/container-packzstd.c`.

## PACKING AND UNPACKING

`pack(image,length,max_image,max_packed,out,info)` validates the image envelope
and produces a standard single-segment Zstandard frame. Raw blocks preserve
arbitrary content; repeated spans use RLE. Output is replaced atomically and can
alias input. Input and output limits are independent.

`unpack(packed,length,max_packed,max_image,out,info)` checks input quota, decodes
within the advertised-size/output quota, validates the image envelope and publishes
bytes only on complete success. It checks image CRC after decompression. Configuration
and archive grammar are fully checked later by image2container.

Optional info reports original/packed bytes, image payload checksum and architecture.
It changes only on success. All owned output is a CT blob, initialized to `{0}`
and released using `mulib_ct_blob_free`.

## FULL CODEC INTEGRATION

The default decoder supports the exact [raw/RLE profile](include-mulib-zstd.md).
Entropy-coded blocks and other unsupported features return ENOTSUP explicitly.
`unpack_with` accepts a caller codec for full Zstandard support. Callback receives
input, output_limit and an initialized empty CT blob. It must allocate through
the same CT allocator, enforce limit, clean or leave valid owned output on failure,
and return zero/negative CT status. The wrapper checks output structure/size and
validates the image before replacing caller output.

The library never silently loads a system zstd library or substitutes another
format. Backend context is borrowed only during the callback. Failure releases
callback-produced output and preserves the previous destination.

## INTEROPERABILITY

Run the example with two output paths to write packed and original image bytes:

```sh
./container-packzstd image.zst image.bin
zstd -d -c image.zst > restored.bin
cmp image.bin restored.bin
```

The focused checker performs this comparison when `zstd` is available. CT errors
distinguish malformed frames/images, unsupported codecs, limits, allocation and
image checksum failures.
