#include <assert.h>
#include "compression_test_setup.h"
static void roundtrips (void)
{
  unsigned char input[70000]; size_t c,k,i,n; unsigned state=12345;
  for (c=0;c<sizeof (codecs)/sizeof (codecs[0]);++c) {
    for (k=0;k<7;++k) {
      mulib_buffer_t packed={0},decoded={0};
      n=k<4?k:k==4?256:k==5?70000:4096;
      for (i=0;i<n;++i) { state=state*1664525u+1013904223u; input[i]=k==5?(unsigned char)(i%17):k==6?'x':(unsigned char)(state>>24); }
      assert (!codecs[c].encode (input,n,&packed));
      if (k>=5) assert (packed.length<n/2);
      { int rc=codecs[c].decode (packed.data,packed.length,n,&decoded);
        if (rc) fprintf (stderr,"%s decode n=%zu rc=%d size=%zu\n",codecs[c].name,n,rc,packed.length);
        assert (!rc); }
      assert (decoded.length==n && (!n || !memcmp (input,decoded.data,n)));
      if (n) {
        size_t before=decoded.length;
        assert (codecs[c].decode (packed.data,packed.length,n-1,&decoded)==MULIB_CODEC_ELIMIT);
        assert (decoded.length==before && !memcmp (input,decoded.data,n));
      }
      assert (!codecs[c].decode (packed.data,packed.length,n,&packed));
      assert (!n || !memcmp (packed.data+packed.length-n,input,n));
      mulib_buffer_free (&packed); mulib_buffer_free (&decoded);
      assert (mulib_buffer_append (&packed,input,n));
      assert (!codecs[c].encode (packed.data,n,&packed));
      assert (!n || !memcmp (packed.data,input,n));
      assert (!codecs[c].decode (packed.data+n,packed.length-n,n,&decoded));
      assert (decoded.length==n && (!n || !memcmp (decoded.data,input,n)));
      mulib_buffer_free (&packed); mulib_buffer_free (&decoded);
    }
  }
}
static void hostile (void)
{
  size_t c,i,j; unsigned char input[96];
  for (i=0;i<sizeof (input);++i) input[i]=(unsigned char)(i%7);
  for (c=0;c<sizeof (codecs)/sizeof (codecs[0]);++c) {
    mulib_buffer_t packed={0},out={0};
    assert (!codecs[c].encode (input,sizeof (input),&packed));
    assert (mulib_buffer_append (&out,"prefix",6));
    for (i=0;i<packed.length;++i) {
      assert (codecs[c].decode (packed.data,i,256,&out)<0);
      assert (out.length==6 && !memcmp (out.data,"prefix",6));
      for (j=0;j<8;++j) {
        int rc; packed.data[i]^=(unsigned char)(1u<<j);
        rc=codecs[c].decode (packed.data,packed.length,256,&out);
        if (rc) assert (out.length==6 && !memcmp (out.data,"prefix",6));
        else { assert (out.length<=262); out.length=6; }
        packed.data[i]^=(unsigned char)(1u<<j);
      }
    }
    assert (codecs[c].encode (NULL,1,&out)==MULIB_CODEC_EINVAL);
    assert (codecs[c].decode (NULL,1,256,&out)==MULIB_CODEC_EINVAL);
    assert (codecs[c].decode (NULL,0,256,&out)==MULIB_CODEC_EFORMAT);
    mulib_buffer_free (&out); mulib_buffer_free (&packed);
  }
}
static void framing (void)
{
  mulib_buffer_t packed={0},out={0}; size_t used=123,at; unsigned char h[32]={31,139,8,30,0,0,0,0,0,255,3,0,'x','y','z','n',0,'c',0};
  uint32_t crc=mulib_crc32 (h,19);
  h[19]=(unsigned char)crc; h[20]=(unsigned char)(crc>>8);
  assert (mulib_buffer_append (&packed,h,21));
  assert (!mulib_deflate_encode ("abc",3,&packed));
  mulib_codec_put32 (h,mulib_crc32 ("abc",3)); mulib_codec_put32 (h+4,3);
  assert (mulib_buffer_append (&packed,h,8));
  assert (!mulib_gzip_decode (packed.data,packed.length,3,&out));
  assert (out.length==3 && !memcmp (out.data,"abc",3));
  packed.data[19]^=1;
  assert (mulib_gzip_decode (packed.data,packed.length,3,&out)==MULIB_CODEC_ECHECKSUM && out.length==3);
  mulib_buffer_free (&packed); mulib_buffer_free (&out);
  assert (!mulib_lzma2_encode ("abc",3,&packed));
  for (at=0;at<packed.length;++at) {
    assert (mulib_lzma2_decode (packed.data,at,4096,3,&used,&out)<0 && used==123 && !out.length);
  }
  assert (!mulib_lzma2_decode (packed.data,packed.length,4096,3,&used,&out) && used==packed.length);
  mulib_buffer_free (&packed); mulib_buffer_free (&out);
  {
    unsigned char input[4096],alone[13]={93,0,0,128,0}; mulib_buffer_t stream={0}; size_t n;
    memset (input,'a',sizeof (input)); assert (!mulib_lzma2_encode (input,sizeof (input),&packed));
    assert (packed.data[0]>=0xe0); n=((size_t)packed.data[3]<<8|packed.data[4])+1;
    mulib_codec_put64 (alone+5,sizeof (input));
    assert (mulib_buffer_append (&stream,alone,13) && mulib_buffer_append (&stream,packed.data+6,n));
    assert (!mulib_lzma_decode (stream.data,stream.length,sizeof (input),&out));
    assert (out.length==sizeof (input) && !memcmp (out.data,input,sizeof (input)));
    mulib_buffer_free (&stream); mulib_buffer_free (&packed); mulib_buffer_free (&out);
  }
  assert (mulib_codec_crc64 ("123456789",9)==UINT64_C(0x995dc9bbdf1939fa));
  assert (mulib_codec_xxh64 (NULL,0)==UINT64_C(0xef46db3751d8e999));
  used=123;
  assert (!mulib_deflate_encode ("abc",3,&packed)); at=packed.length;
  assert (mulib_buffer_append (&packed,"tail",4));
  assert (mulib_deflate_decode (packed.data,packed.length,3,&out)==MULIB_DEFLATE_EFORMAT && !out.length);
  assert (!mulib_deflate_decode_prefix (packed.data,packed.length,3,&used,&out) && used==at);
  assert (out.length==3 && !memcmp (out.data,"abc",3)); used=123;
  assert (mulib_deflate_decode_prefix (packed.data,at-1,3,&used,&out)==MULIB_DEFLATE_EFORMAT && used==123 && out.length==3);
  mulib_buffer_free (&packed); mulib_buffer_free (&out);
}
static void zip_budgets (void)
{
  mulib_zip_writer_t w={0}; mulib_zip_reader_t r={0}; unsigned char fake[22]={0}; size_t end;
  assert (!mulib_zip_add (&w,"file","hello",5,8) && !mulib_zip_finish (&w));
  assert (mulib_zip_open_limited (&r,w.bytes.data,w.bytes.length,0,10)==MULIB_ZIP_ELIMIT && !r.data);
  assert (mulib_zip_open_limited (&r,w.bytes.data,w.bytes.length,1,4)==MULIB_ZIP_ELIMIT && !r.entries);
  assert (!mulib_zip_open_limited (&r,w.bytes.data,w.bytes.length,1,5)); mulib_zip_close (&r);
  end=w.bytes.length-22; w.bytes.data[end+20]=22;
  mulib_codec_put32 (fake,UINT32_C(0x06054b50)); mulib_codec_put32 (fake+16,(uint32_t)w.bytes.length);
  assert (mulib_buffer_append (&w.bytes,fake,22));
  assert (!mulib_zip_open (&r,w.bytes.data,w.bytes.length) && r.count==1);
  mulib_zip_close (&r); mulib_zip_writer_free (&w);
}
int main (int argc, char **argv)
{
  size_t i;
  if (argc==5 || argc==6) {
    FILE *f; unsigned char bytes[4096]; size_t n;
    mulib_buffer_t input={0},output={0},dictionary={0}; int rc=MULIB_CODEC_EINVAL;
    f=fopen (argv[3],"rb"); if (!f) return 2;
    while ((n=fread (bytes,1,sizeof (bytes),f))!=0) assert (mulib_buffer_append (&input,bytes,n));
    assert (!ferror (f)); fclose (f);
    if (argc==6) {
      f=fopen (argv[5],"rb"); if (!f) { mulib_buffer_free (&input); return 2; }
      while ((n=fread (bytes,1,sizeof (bytes),f))!=0) assert (mulib_buffer_append (&dictionary,bytes,n));
      assert (!ferror (f)); fclose (f);
    }
    for (i=0;i<sizeof (codecs)/sizeof (codecs[0]);++i) if (!strcmp (argv[1],codecs[i].name)) {
      if (argc==6 && !strcmp (argv[1],"zstd"))
        rc=mulib_zstd_decode_dict (input.data,input.length,dictionary.data,dictionary.length,16777216,&output);
      else rc=!strcmp (argv[2],"encode")?codecs[i].encode (input.data,input.length,&output):codecs[i].decode (input.data,input.length,16777216,&output);
      break;
    }
    if (!rc) { f=fopen (argv[4],"wb"); assert (f); assert (!output.length || fwrite (output.data,1,output.length,f)==output.length); assert (!fclose (f)); }
    else fprintf (stderr,"%s: %d\n",argv[1],rc);
    mulib_buffer_free (&input); mulib_buffer_free (&output); mulib_buffer_free (&dictionary); return rc?1:0;
  }
  roundtrips (); hostile (); framing (); zip_budgets (); return 0;
}
