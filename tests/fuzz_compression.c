/* Dependency-free codec hostile-input target. No filesystem or codec backends. */
#include "compression_test_setup.h"
int LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  size_t i,used=0; mulib_buffer_t out={0}; mulib_zip_reader_t zip={0};
  if (size>1048576) return 0;
  for (i=0;i<sizeof (codecs)/sizeof (codecs[0]);++i) {
    (void)codecs[i].decode (data,size,65536,&out); mulib_buffer_free (&out);
  }
  (void)mulib_lzma2_decode (data,size,8388608,65536,&used,&out); mulib_buffer_free (&out);
  (void)mulib_lzf_raw_decode (data,size,65536,&out); mulib_buffer_free (&out);
  (void)mulib_zstd_decode_dict (data,size,data,size,65536,&out); mulib_buffer_free (&out);
  {
    static const unsigned char frame[9]={0x28,0xb5,0x2f,0xfd,0x20,0,1,0,0};
    (void)mulib_zstd_decode_dict (frame,sizeof (frame),data,size,65536,&out); mulib_buffer_free (&out);
  }
  if (!mulib_zip_open_limited (&zip,data,size,32,65536)) {
    for (i=0;i<zip.count;++i) {
      (void)mulib_zip_extract (&zip,i,65536,&out); mulib_buffer_free (&out);
    }
  }
  mulib_zip_close (&zip); return 0;
}
