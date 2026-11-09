#ifndef MULIB_COMPRESSION_TEST_SETUP_H
#define MULIB_COMPRESSION_TEST_SETUP_H
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_CODEC_CORE
#define MULIB_IMPL_DEFLATE
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_GZIP
#define MULIB_IMPL_LZF
#define MULIB_IMPL_LZMA
#define MULIB_IMPL_LZIP
#define MULIB_IMPL_BZIP2
#define MULIB_IMPL_LZO
#define MULIB_IMPL_LZOP
#define MULIB_IMPL_ADLER32
#define MULIB_IMPL_XZ
#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_ZSTD
#define MULIB_IMPL_BCJ
#define MULIB_IMPL_ZIP
#define MULIB_IMPL_UTF8
#include "mulib-gzip.h"
#include "mulib-lzf.h"
#include "mulib-lzip.h"
#include "mulib-bzip2.h"
#include "mulib-lzop.h"
#include "mulib-xz.h"
#include "mulib-zstd.h"
#include "mulib-zip.h"
typedef int (*encoder_t) (const void *,size_t,mulib_buffer_t *);
typedef int (*decoder_t) (const void *,size_t,size_t,mulib_buffer_t *);
static const struct codec { const char *name; encoder_t encode; decoder_t decode; } codecs[]={
  {"gzip",mulib_gzip_encode,mulib_gzip_decode},
  {"lzf",mulib_lzf_encode,mulib_lzf_decode},
  {"lzma",mulib_lzma_encode,mulib_lzma_decode},
  {"lzip",mulib_lzip_encode,mulib_lzip_decode},
  {"bzip2",mulib_bzip2_encode,mulib_bzip2_decode},
  {"lzo",mulib_lzo1x_encode,mulib_lzo1x_decode},
  {"lzop",mulib_lzop_encode,mulib_lzop_decode},
  {"xz",mulib_xz_encode,mulib_xz_decode},
  {"zstd",mulib_zstd_encode,mulib_zstd_decode}
};
#endif
