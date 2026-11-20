#define MULIB_IMPL_XSIMD
#define MULIB_IMPL_AUDIO
#define MULIB_IMPL_MINIMPEG
#ifdef MULIB_MINIMPEG_USE_LIBAVCODEC
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_CODEC_CORE
#endif
#include "mulib-minimpeg.h"
#include <assert.h>

#ifdef MULIB_MINIMPEG_USE_LIBAVCODEC
#include <libavutil/log.h>

enum { WIDTH=64, HEIGHT=48, NFRAMES=15, RAW=WIDTH*HEIGHT*3/2 };
static const mulib_minimpeg_video_format_t format={WIDTH,HEIGHT,25,1};
static const size_t test_io_limit=64UL*1024*1024;

static void make_frames(unsigned char *raw, unsigned char *padded,
    mulib_minimpeg_video_frame_t *frames, const mulib_minimpeg_video_format_t *f, size_t n)
{
  size_t i;
  for(i=0;i<n;++i) {
    unsigned p;
    for(p=0;p<3;++p) {
      unsigned w=f->width>>(p!=0), h=f->height>>(p!=0), y,x;
      size_t stride=w+7;
      frames[i].planes[p].data=padded;
      frames[i].planes[p].stride=stride;
      frames[i].planes[p].size=stride*(h-1)+w;
      for(y=0;y<h;++y) for(x=0;x<w;++x) {
        unsigned val=p==0 ? 24+9*(unsigned)i+(x+2*y)%47 :
            p==1 ? 71+((unsigned)i+x)%29 : 167+((unsigned)i+y)%33;
        *raw++=padded[y*stride+x]=(unsigned char)val;
      }
      padded+=stride*h;
    }
  }
}

static int empty(const mulib_minimpeg_video_clip_t *c)
{
  return !c->data && !c->size && !c->frames && !c->frame_size &&
      !c->format.width && !c->format.height && !c->format.fps_num && !c->format.fps_den;
}

static void compare(const unsigned char *a, const unsigned char *b, size_t count)
{
  size_t f;
  for(f=0;f<count;++f) {
    size_t offset=0;
    unsigned p;
    for(p=0;p<3;++p) {
      size_t k, size=p ? WIDTH*HEIGHT/4 : WIDTH*HEIGHT;
      unsigned error=0;
      for(k=0;k<size;++k)
        error+=(unsigned)abs((int)a[f*RAW+offset+k]-(int)b[f*RAW+offset+k]);
      /* Compare each plane of each frame independently: catches swapped
       * chroma, absence of a final picture and B-picture reordering bugs. */
      assert(error<5*size);
      offset+=size;
    }
  }
}

static void test_rates(const mulib_minimpeg_video_frame_t *frame)
{
  static const unsigned rates[][2]={
    {24000,1001},{24,1},{25,1},{30000,1001},{30,1},{50,1},{60000,1001},{60,1}
  };
  size_t i;
  for(i=0;i<sizeof(rates)/sizeof(rates[0]);++i) {
    mulib_minimpeg_video_format_t f={WIDTH,HEIGHT,2*rates[i][0],2*rates[i][1]};
    mulib_minimpeg_video_clip_t c={0};
    mulib_audio_bytes_t b={0};
    assert(mulib_minimpeg_video_encode(&f,frame,1,NULL,&b)==MULIB_MINIMPEG_VIDEO_OK);
    assert(mulib_minimpeg_video_decode(b.data,b.size,NULL,&c)==MULIB_MINIMPEG_VIDEO_OK);
    assert(c.frames==1 && c.size==RAW && c.format.fps_num==rates[i][0] && c.format.fps_den==rates[i][1]);
    mulib_minimpeg_video_free(&c);
    mulib_audio_bytes_free(&b);
  }
}

static void test_geometry(unsigned w, unsigned h)
{
  mulib_minimpeg_video_format_t f={w,h,25,1};
  mulib_minimpeg_video_frame_t view;
  mulib_minimpeg_video_clip_t clip={0};
  mulib_audio_bytes_t b={0};
  size_t raw_size=(size_t)w*h*3/2;
  unsigned char *raw=(unsigned char *)malloc(raw_size);
  unsigned char *padded=(unsigned char *)malloc(raw_size+14*h);
  assert(raw && padded);
  make_frames(raw,padded,&view,&f,1);
  assert(mulib_minimpeg_video_encode(&f,&view,1,NULL,&b)==MULIB_MINIMPEG_VIDEO_OK);
  assert(mulib_minimpeg_video_decode(b.data,b.size,NULL,&clip)==MULIB_MINIMPEG_VIDEO_OK);
  assert(clip.format.width==w && clip.format.height==h && clip.size==raw_size && clip.frames==1);
  assert(abs((int)clip.data[0]-raw[0])<8);
  assert(abs((int)clip.data[w*h]-raw[w*h])<8);
  assert(abs((int)clip.data[w*h*5/4]-raw[w*h*5/4])<8);
  mulib_minimpeg_video_free(&clip);
  mulib_audio_bytes_free(&b);
  free(padded);
  free(raw);
}

static void self_test(const mulib_minimpeg_video_frame_t *views, const unsigned char *raw,
    const mulib_audio_bytes_t *stream)
{
  static const unsigned char seq[]={0,0,1,0xb3}, end[]={0,0,1,0xb7};
  mulib_minimpeg_video_frame_t bad=views[0];
  mulib_minimpeg_video_format_t f=format;
  mulib_minimpeg_video_limits_t l={0,0,0};
  mulib_minimpeg_video_clip_t clip={0};
  mulib_audio_bytes_t b={0};
  unsigned char *copy=(unsigned char *)malloc(stream->size+4);
  size_t i,second=0,last_slice=0;
  assert(copy);
  assert(!memcmp(stream->data,seq,4) && !memcmp(stream->data+stream->size-4,end,4));
  assert(mulib_minimpeg_video_decode(stream->data,stream->size,NULL,&clip)==MULIB_MINIMPEG_VIDEO_OK);
  assert(clip.format.width==WIDTH && clip.format.height==HEIGHT && clip.format.fps_num==25 &&
      clip.format.fps_den==1 && clip.frames==NFRAMES && clip.frame_size==RAW && clip.size==NFRAMES*RAW);
  assert(mulib_minimpeg_video_decode(stream->data,stream->size,NULL,&clip)==MULIB_MINIMPEG_VIDEO_INVALID);
  compare(raw,clip.data,NFRAMES);
  mulib_minimpeg_video_free(&clip);
  mulib_minimpeg_video_free(&clip);
  assert(empty(&clip));
  assert(mulib_minimpeg_video_encode(&format,views,NFRAMES,NULL,&b)==MULIB_MINIMPEG_VIDEO_OK);
  assert(b.size==stream->size && !memcmp(b.data,stream->data,b.size));
  assert(mulib_minimpeg_video_encode(&format,views,NFRAMES,NULL,&b)==MULIB_MINIMPEG_VIDEO_INVALID);
  mulib_audio_bytes_free(&b);
  bad.planes[1].size--;
  assert(mulib_minimpeg_video_encode(&format,&bad,1,NULL,&b)==MULIB_MINIMPEG_VIDEO_INVALID && !b.data);
  bad=views[0];
  bad.planes[0].stride=SIZE_MAX;
  assert(mulib_minimpeg_video_encode(&format,&bad,1,NULL,&b)==MULIB_MINIMPEG_VIDEO_INVALID && !b.data);
  f.width--;
  assert(mulib_minimpeg_video_encode(&f,views,1,NULL,&b)==MULIB_MINIMPEG_VIDEO_UNSUPPORTED && !b.data);
  f.width=770;
  assert(mulib_minimpeg_video_encode(&f,views,1,NULL,&b)==MULIB_MINIMPEG_VIDEO_LIMIT && !b.data);
  f=format;
  f.fps_num=23;
  assert(mulib_minimpeg_video_encode(&f,views,1,NULL,&b)==MULIB_MINIMPEG_VIDEO_UNSUPPORTED && !b.data);
  assert(mulib_minimpeg_video_encode(&format,views,0,NULL,&b)==MULIB_MINIMPEG_VIDEO_INVALID && !b.data);
  l.max_frames=NFRAMES-1;
  assert(mulib_minimpeg_video_encode(&format,views,NFRAMES,&l,&b)==MULIB_MINIMPEG_VIDEO_LIMIT && !b.data);
  assert(mulib_minimpeg_video_decode(stream->data,stream->size,&l,&clip)==MULIB_MINIMPEG_VIDEO_LIMIT && empty(&clip));
  l.max_frames=NFRAMES;
  l.max_raw_bytes=NFRAMES*RAW-1;
  assert(mulib_minimpeg_video_encode(&format,views,NFRAMES,&l,&b)==MULIB_MINIMPEG_VIDEO_LIMIT && !b.data);
  assert(mulib_minimpeg_video_decode(stream->data,stream->size,&l,&clip)==MULIB_MINIMPEG_VIDEO_LIMIT && empty(&clip));
  l.max_raw_bytes=NFRAMES*RAW;
  l.max_stream_bytes=stream->size-1;
  assert(mulib_minimpeg_video_encode(&format,views,NFRAMES,&l,&b)==MULIB_MINIMPEG_VIDEO_LIMIT && !b.data);
  assert(mulib_minimpeg_video_decode(stream->data,stream->size,&l,&clip)==MULIB_MINIMPEG_VIDEO_LIMIT && empty(&clip));
  l.max_stream_bytes=stream->size;
  assert(mulib_minimpeg_video_encode(&format,views,NFRAMES,&l,&b)==MULIB_MINIMPEG_VIDEO_OK);
  assert(b.size==l.max_stream_bytes && b.capacity<=l.max_stream_bytes);
  mulib_audio_bytes_free(&b);
  assert(mulib_minimpeg_video_decode(stream->data,stream->size,&l,&clip)==MULIB_MINIMPEG_VIDEO_OK);
  assert(clip.frames==NFRAMES);
  mulib_minimpeg_video_free(&clip);
  assert(mulib_minimpeg_video_decode(NULL,0,NULL,&clip)==MULIB_MINIMPEG_VIDEO_FORMAT && empty(&clip));
  assert(mulib_minimpeg_video_decode(NULL,1,NULL,&clip)==MULIB_MINIMPEG_VIDEO_INVALID && empty(&clip));
  assert(mulib_minimpeg_video_decode(stream->data,7,NULL,&clip)==MULIB_MINIMPEG_VIDEO_FORMAT && empty(&clip));
  assert(mulib_minimpeg_video_decode(stream->data,stream->size-4,NULL,&clip)==MULIB_MINIMPEG_VIDEO_OK);
  assert(clip.frames==NFRAMES);
  mulib_minimpeg_video_free(&clip);
  for(i=4;i+4<=stream->size;++i) {
    if(!second && !memcmp(stream->data+i,seq,4)) second=i;
    if(!memcmp(stream->data+i,seq,3) && stream->data[i+3]>=1 && stream->data[i+3]<=0xaf)
      last_slice=i;
  }
  assert(second && last_slice);
  memcpy(copy,stream->data,stream->size);
  copy[second+6]^=2; /* change height, still even */
  assert(mulib_minimpeg_video_decode(copy,stream->size,NULL,&clip)==MULIB_MINIMPEG_VIDEO_UNSUPPORTED && empty(&clip));
  memcpy(copy,stream->data,stream->size);
  copy[4]=0xff; /* width > profile cap, rejected before the codec runs */
  assert(mulib_minimpeg_video_decode(copy,stream->size,NULL,&clip)==MULIB_MINIMPEG_VIDEO_LIMIT && empty(&clip));
  memcpy(copy,stream->data,stream->size);
  copy[stream->size-1]=0xb5;
  assert(mulib_minimpeg_video_decode(copy,stream->size,NULL,&clip)==MULIB_MINIMPEG_VIDEO_UNSUPPORTED && empty(&clip));
  memcpy(copy,stream->data,stream->size);
  memcpy(copy+stream->size,seq,4);
  assert(mulib_minimpeg_video_decode(copy,stream->size+4,NULL,&clip)==MULIB_MINIMPEG_VIDEO_FORMAT && empty(&clip));
  copy[last_slice+4]&=7; /* zero slice quantizer */
  assert(mulib_minimpeg_video_decode(copy,stream->size,NULL,&clip)==MULIB_MINIMPEG_VIDEO_FORMAT && empty(&clip));
  assert(mulib_minimpeg_video_decode(stream->data,last_slice+5,NULL,&clip)==MULIB_MINIMPEG_VIDEO_FORMAT && empty(&clip));
  /* The envelope is plausible but entropy data ends inside the last slice. */
  assert(mulib_minimpeg_video_decode(stream->data,last_slice+6,NULL,&clip)==MULIB_MINIMPEG_VIDEO_FORMAT && empty(&clip));
  free(copy);
  test_rates(views);
  test_geometry(2,2);
  test_geometry(62,50);
  test_geometry(768,576);
}
#endif

int main(int argc, char **argv)
{
#ifdef MULIB_MINIMPEG_USE_LIBAVCODEC
  /* Do not print the expected native diagnostic for the truncated slice. */
  av_log_set_level(AV_LOG_QUIET);
  assert(mulib_minimpeg_video_available()==
      (MULIB_MINIMPEG_VIDEO_ENCODER|MULIB_MINIMPEG_VIDEO_DECODER));
  if(argc==4 && !strcmp(argv[1],"encode")) {
    mulib_audio_bytes_t raw={0}, stream={0};
    mulib_minimpeg_video_frame_t *views;
    size_t n,i;
    assert(mulib_audio_read_file(argv[2],test_io_limit,&raw)==MULIB_AUDIO_OK);
    assert(raw.size && !(raw.size%RAW));
    n=raw.size/RAW;
    views=(mulib_minimpeg_video_frame_t *)calloc(n,sizeof(*views));
    assert(views);
    for(i=0;i<n;++i) {
      unsigned p;
      size_t offset=i*RAW;
      for(p=0;p<3;++p) {
        size_t w=WIDTH>>(p!=0), h=HEIGHT>>(p!=0);
        views[i].planes[p].data=raw.data+offset;
        views[i].planes[p].size=w*h;
        views[i].planes[p].stride=w;
        offset+=w*h;
      }
    }
    assert(mulib_minimpeg_video_encode(&format,views,n,NULL,&stream)==MULIB_MINIMPEG_VIDEO_OK);
    assert(mulib_audio_write_file(argv[3],stream.data,stream.size)==MULIB_AUDIO_OK);
    mulib_audio_bytes_free(&stream);
    mulib_audio_bytes_free(&raw);
    free(views);
  } else if(argc==4 && !strcmp(argv[1],"decode")) {
    mulib_audio_bytes_t stream={0};
    mulib_minimpeg_video_clip_t clip={0};
    mulib_minimpeg_video_status_t st;
    assert(mulib_audio_read_file(argv[2],test_io_limit,&stream)==MULIB_AUDIO_OK);
    st=mulib_minimpeg_video_decode(stream.data,stream.size,NULL,&clip);
    if(st) fprintf(stderr,"MPEG-1 decode status %d: %s\n",(int)st,argv[2]);
    assert(st==MULIB_MINIMPEG_VIDEO_OK);
    assert(clip.format.width==WIDTH && clip.format.height==HEIGHT &&
        clip.format.fps_num==25 && clip.format.fps_den==1);
    assert(mulib_audio_write_file(argv[3],clip.data,clip.size)==MULIB_AUDIO_OK);
    mulib_audio_bytes_free(&stream);
    mulib_minimpeg_video_free(&clip);
  } else if(argc==4 && !strcmp(argv[1],"compare")) {
    mulib_audio_bytes_t ref={0}, raw={0};
    assert(mulib_audio_read_file(argv[2],test_io_limit,&ref)==MULIB_AUDIO_OK && ref.size && !(ref.size%RAW));
    assert(mulib_audio_read_file(argv[3],test_io_limit,&raw)==MULIB_AUDIO_OK && raw.size==ref.size);
    compare(ref.data,raw.data,ref.size/RAW);
    mulib_audio_bytes_free(&raw);
    mulib_audio_bytes_free(&ref);
  } else {
    mulib_minimpeg_video_frame_t views[NFRAMES];
    mulib_audio_bytes_t stream={0};
    unsigned char raw[NFRAMES*RAW], padded[NFRAMES*(RAW+14*HEIGHT)];
    assert(argc==1 || (argc==4 && !strcmp(argv[1],"fixture")));
    memset(padded,0x5a,sizeof(padded));
    make_frames(raw,padded,views,&format,NFRAMES);
    assert(mulib_minimpeg_video_encode(&format,views,NFRAMES,NULL,&stream)==MULIB_MINIMPEG_VIDEO_OK);
    if(argc==4) {
      assert(mulib_audio_write_file(argv[2],stream.data,stream.size)==MULIB_AUDIO_OK);
      assert(mulib_audio_write_file(argv[3],raw,sizeof(raw))==MULIB_AUDIO_OK);
    } else self_test(views,raw,&stream);
    mulib_audio_bytes_free(&stream);
  }
#else
  mulib_minimpeg_video_format_t f={16,16,25,1};
  mulib_minimpeg_video_frame_t frame={{{0}}};
  mulib_audio_pcm_t pcm={0}, decoded={0};
  mulib_audio_bytes_t b={0};
  mulib_minimpeg_video_clip_t clip={0};
  (void)argc;
  (void)argv;
  assert(mulib_minimpeg_video_available()==0);
  assert(mulib_minimpeg_video_encode(&f,&frame,1,NULL,&b)==MULIB_MINIMPEG_VIDEO_UNAVAILABLE);
  assert(mulib_minimpeg_video_decode("x",1,NULL,&clip)==MULIB_MINIMPEG_VIDEO_UNAVAILABLE);
  assert(!b.data && !clip.data);
  assert(mulib_audio_pcm_create(&pcm,1,44100,1)==MULIB_AUDIO_OK);
  assert(mulib_minimpeg_encode(&pcm,NULL,&b)==MULIB_AUDIO_OK);
  assert(mulib_minimpeg_decode(b.data,b.size,NULL,&decoded)==MULIB_AUDIO_OK);
  assert(decoded.frames==2304 && decoded.rate==44100 && decoded.channels==1);
  mulib_audio_bytes_free(&b);
  mulib_audio_pcm_free(&decoded);
  mulib_audio_pcm_free(&pcm);
#endif
  return 0;
}
