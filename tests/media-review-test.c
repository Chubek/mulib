/* Bundle regression tests: caller-backed C11 mechanisms, not production ABI
 * admission, real codec coverage, durable storage, or archive extraction. */
#include "mulib-media-timebase.h"
#include "mulib-media-convert.h"
#include "mulib-media-scale.h"
#include "mulib-media-proxy.h"
#include "mulib-media-clock.h"
#include "mulib-media-bufferpool.h"
#include "mulib-media-codec.h"
#include "mulib-media-export.h"
#include "mulib-media-stream.h"
#include "mulib-media-isobmff.h"
#include "mulib-media-matroska.h"
#include "mulib-media-av1.h"
#include "mulib-media-h265.h"
#include "mulib-media-vp9.h"
#include "mulib-media-mpegts.h"
#include "mulib-nle-edit.h"
#include "mulib-nle-ripple.h"
#include "mulib-nle-snap.h"
#include "mulib-nle-otio.h"
#include "mulib-asset-cache.h"
#include "mulib-project-history.h"
#include "mulib-project-journal.h"
#include "mulib-project-migrate.h"
#include "mulib-project-package.h"
#include "mulib-project-store.h"
#include <assert.h>

static uint32_t random_state = 0x138fad21u;
static uint32_t
random_u32(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void
test_time(void)
{
    int64_t t = 123;
    mulib_fraction_t canonical = {1, 48000};
    mulib_media_rational base = canonical; /* Canonical type interoperability. */
    assert(mulib_media_timebase_rescale(30, (mulib_media_rational){1,30}, base, &t));
    assert(t == 48000);
    assert(mulib_media_timebase_rescale(INT64_MIN, base, base, &t) && t == INT64_MIN);
    assert(mulib_media_timebase_rescale(-5, (mulib_media_rational){1,2},
                                      (mulib_media_rational){1,1}, &t) && t == -2);
    t = 123;
    assert(!mulib_media_timebase_rescale(INT64_MAX, (mulib_media_rational){2,1},
                                       (mulib_media_rational){1,1}, &t) && t == 123);
    assert(!mulib_media_timebase_rescale(0, (mulib_media_rational){0,1}, base, &t));
    for (unsigned i=0; i<10000; i++) {
        int64_t value = (int64_t)random_u32() - INT64_C(2147483648);
        int64_t a = 1 + random_u32()%1000, b = 1 + random_u32()%1000;
        int64_t c = 1 + random_u32()%1000, d = 1 + random_u32()%1000;
        /* Independent small-integer oracle: product cannot overflow int64_t. */
        int64_t expected = (value*a*d)/(b*c);
        assert(mulib_media_timebase_rescale(value, (mulib_media_rational){a,b},
                                           (mulib_media_rational){c,d}, &t));
        assert(t == expected);
    }
    mulib_media_clock clock = {INT64_MAX, 0, 1, true};
    assert(mulib_media_clock_position(&clock, 1, &t) && t == INT64_MAX);
    clock.paused = false;
    t = 123;
    assert(!mulib_media_clock_position(&clock, INT64_MAX, &t) && t == 123);
    clock.rate = NAN;
    assert(!mulib_media_clock_position(&clock, 0, &t));
    assert(mulib_nle_otio_time_to_ticks((mulib_nle_otio_time){-1,2},
                                       (mulib_media_rational){1,1}, &t) && t == -1);
    t = 123;
    assert(!mulib_nle_otio_time_to_ticks((mulib_nle_otio_time){DBL_MAX,DBL_MIN},
                                        base, &t) && t == 123);
    assert(!mulib_nle_otio_time_to_ticks((mulib_nle_otio_time){NAN,1}, base, &t));
}

static void
test_frames(void)
{
    uint8_t bytes[64], saved[64];
    for (size_t i=0; i<sizeof bytes; i++) bytes[i]=(uint8_t)i;
    memcpy(saved, bytes, sizeof bytes);
    mulib_media_frame a = {0}, b = {0};
    a.data[0]=bytes; a.stride[0]=8; a.capacity[0]=sizeof bytes;
    a.width=1; a.height=3; a.format=MULIB_MEDIA_RGBA8;
    b=a; b.data[0]=bytes+8; b.capacity[0]-=8;
    assert(mulib_media_frame_valid(&a) && mulib_media_frame_valid(&b));
    assert(!mulib_media_frame_copy(&b, &a)); /* memmove per row was insufficient. */
    assert(!memcmp(bytes, saved, sizeof bytes));
    assert(mulib_media_frame_copy(&a, &a));
    b=a; b.width=2; b.stride[0]=8;
    assert(!mulib_media_scale_rgba8(&b, &a));
    assert(!memcmp(bytes, saved, sizeof bytes));
    b=a; b.format=MULIB_MEDIA_RGB8;
    assert(!mulib_media_convert_rgba8(&a, &b));
    assert(!memcmp(bytes, saved, sizeof bytes));
    a.capacity[0]=19;
    assert(!mulib_media_frame_valid(&a));
    a.capacity[0]=20;
    assert(mulib_media_frame_valid(&a)); /* No final-row padding required. */
    a.width=SIZE_MAX;
    assert(!mulib_media_frame_valid(&a));

    for (size_t w=1; w<=9; w++) for (size_t h=1; h<=9; h++) {
        mulib_media_frame source={0}, target={0}, small={0}, yuv={0};
        assert(mulib_media_frame_alloc(&source,w,h,MULIB_MEDIA_RGBA8));
        assert(mulib_media_frame_alloc(&target,w,h,MULIB_MEDIA_RGBA8));
        for (size_t i=0; i<source.capacity[0]; i++) source.data[0][i]=(uint8_t)random_u32();
        source.pts=42; source.timebase=(mulib_media_rational){1001,30000};
        assert(mulib_media_frame_copy(&target,&source));
        assert(target.pts==42 && target.timebase.den==30000);
        assert(!memcmp(target.data[0],source.data[0],source.capacity[0]));
        assert(mulib_media_proxy_make(&small,&source,3,3));
        assert(small.width<=3 && small.height<=3 && mulib_media_frame_valid(&small));
        assert(mulib_media_frame_alloc(&yuv,w,h,MULIB_MEDIA_YUV420P8));
        assert(mulib_media_frame_valid(&yuv));
        assert(mulib_media_convert_rgba8(&target,&yuv));
        uint8_t *original=yuv.data[1];
        yuv.data[1]=yuv.data[0]; /* Cross-plane overlap is not a valid view. */
        assert(!mulib_media_frame_valid(&yuv));
        yuv.data[1]=original;
        mulib_media_frame_release(&source); mulib_media_frame_release(&target);
        mulib_media_frame_release(&small); mulib_media_frame_release(&yuv);
    }
    mulib_media_bufferpool pool={0};
    assert(mulib_media_bufferpool_init(&pool,2,2,2,MULIB_MEDIA_RGBA8));
    mulib_media_frame *first=mulib_media_bufferpool_acquire(&pool);
    mulib_media_frame *second=mulib_media_bufferpool_acquire(&pool);
    assert(first && second && first!=second && !mulib_media_bufferpool_acquire(&pool));
    assert(mulib_media_bufferpool_return(&pool,first));
    assert(!mulib_media_bufferpool_return(&pool,first));
    assert(mulib_media_bufferpool_return(&pool,second));
    mulib_media_bufferpool_destroy(&pool);
}

typedef struct provider_state {
    unsigned reads,writes,finishes;
    mulib_media_status next_status,write_status,finish_status;
} provider_state;
static unsigned releases;
static mulib_media_status
provider_next(void *ctx,mulib_media_packet *p)
{
    provider_state *s=ctx;
    uint8_t *data=malloc(1);
    assert(data);
    *data=42;
    p->bytes=(mulib_media_bytes){data,1};
    s->reads++;
    return s->next_status;
}
static mulib_media_status
provider_write(void *ctx,const mulib_media_packet *p)
{
    provider_state *s=ctx;
    assert(p->bytes.size==1 && p->bytes.data[0]==42);
    s->writes++;
    return s->write_status;
}
static mulib_media_status
provider_sequence_next(void *ctx,mulib_media_packet *p)
{
    provider_state *s=ctx;
    if(s->reads==2)s->next_status=MULIB_MEDIA_EOF;
    return provider_next(ctx,p);
}
static bool
provider_cancel(void *ctx,size_t packets)
{
    (void)ctx; (void)packets;
    return false;
}
static mulib_media_status
provider_bad_frame(void *ctx,mulib_media_frame *f)
{
    (void)ctx; (void)f;
    return MULIB_MEDIA_OK; /* Empty output is not a decoded frame. */
}
static mulib_media_status
provider_bad_packet(void *ctx,mulib_media_packet *p)
{
    (void)ctx;
    p->bytes.size=1;
    return MULIB_MEDIA_OK;
}
static mulib_media_status
provider_finish(void *ctx)
{
    provider_state *s=ctx;
    s->finishes++;
    return s->finish_status;
}
static void
provider_release(mulib_media_packet *p)
{
    releases++;
    mulib_media_packet_release(p);
}
static void
test_providers(void)
{
    provider_state state={0,0,0,MULIB_MEDIA_IO,MULIB_MEDIA_OK,MULIB_MEDIA_IO};
    mulib_media_demux demux={&state,provider_next,NULL,NULL};
    mulib_media_mux mux={&state,provider_write,provider_finish,NULL,false};
    releases=0;
    assert(mulib_media_export_remux(&demux,&mux,NULL,NULL,provider_release)==MULIB_MEDIA_IO);
    assert(releases==1 && state.reads==1 && state.finishes==0);
    state.next_status=MULIB_MEDIA_OK; state.write_status=MULIB_MEDIA_IO;
    assert(mulib_media_export_remux(&demux,&mux,NULL,NULL,provider_release)==MULIB_MEDIA_IO);
    assert(releases==2 && state.writes==1);
    state.next_status=MULIB_MEDIA_EOF;
    assert(mulib_media_export_remux(&demux,&mux,NULL,NULL,provider_release)==MULIB_MEDIA_IO);
    assert(releases==3 && state.finishes==1 && mux.finished);
    assert(mulib_media_mux_finish(&mux)==MULIB_MEDIA_INVALID && state.finishes==1);
    assert(mulib_media_export_remux(&demux,&mux,NULL,NULL,provider_release)==MULIB_MEDIA_INVALID);
    assert(state.reads==3);
    mulib_media_packet invalid={0}, source={0};
    invalid.bytes.size=1;
    assert(!mulib_media_packet_clone(&invalid,&source));
    assert(mulib_media_demux_next(&demux,&invalid)==MULIB_MEDIA_INVALID);
    mulib_media_codec codec={0};
    assert(mulib_media_codec_encode_receive(&codec,&invalid)==MULIB_MEDIA_INVALID);
    mulib_media_frame frame={0};
    codec.receive_frame=provider_bad_frame;
    codec.receive_packet=provider_bad_packet;
    assert(mulib_media_codec_decode_receive(&codec,&frame)==MULIB_MEDIA_IO);
    invalid.bytes.size=0;
    assert(mulib_media_codec_encode_receive(&codec,&invalid)==MULIB_MEDIA_IO);
    state=(provider_state){0,0,0,MULIB_MEDIA_OK,MULIB_MEDIA_OK,MULIB_MEDIA_OK};
    mux.finished=false; demux.next=provider_sequence_next; releases=0;
    assert(mulib_media_export_remux(&demux,&mux,provider_cancel,NULL,provider_release)==MULIB_MEDIA_CANCELLED);
    assert(state.reads==0 && state.finishes==0 && releases==0);
    assert(mulib_media_export_remux(&demux,&mux,NULL,NULL,provider_release)==MULIB_MEDIA_OK);
    assert(state.reads==3 && state.writes==2 && state.finishes==1 && releases==3);
}

static void
test_timeline(void)
{
    mulib_nle_item storage[4];
    mulib_nle_timeline t={storage,0,4,{1,48000}};
    assert(mulib_nle_timeline_insert(&t,(mulib_nle_item){1,2,3,10,20,5}));
    assert(mulib_nle_edit_split(&t,1,20,4));
    assert(storage[0].length==10 && storage[1].source==15);
    mulib_nle_item before[2]; memcpy(before,storage,sizeof before);
    assert(!mulib_nle_ripple_shift(&t,3,0,INT64_MAX));
    assert(!memcmp(before,storage,sizeof before));
    storage[1].start=INT64_MAX; storage[1].length=1;
    assert(!mulib_nle_timeline_valid(&t));
    assert(!mulib_nle_ripple_shift(&t,3,0,1));
    memcpy(storage,before,sizeof before);
    storage[1].id=1;
    assert(!mulib_nle_timeline_valid(&t));
    memcpy(storage,before,sizeof before);
    int64_t snap=777;
    assert(!mulib_nle_snap_nearest(&t,0,INT64_MIN,INT64_MAX,&snap) && snap==777);
    assert(mulib_nle_snap_nearest(&t,0,15,5,&snap) && snap==10);
    storage[0]=(mulib_nle_item){1,2,3,INT64_MAX-10,5,0};
    t.count=1;
    assert(mulib_nle_snap_nearest(&t,0,INT64_MAX-7,3,&snap) && snap==INT64_MAX-5);
    assert(mulib_nle_timeline_remove(&t,1) && t.count==0);
    t.capacity=SIZE_MAX;
    assert(!mulib_nle_timeline_valid(&t));
}

static bool
migrate_step(void *ctx)
{
    (*(unsigned *)ctx)++;
    return true;
}
static void
test_projects(void)
{
    assert(mulib_project_package_path("assets/a.png"));
    const char bounded[]={'a','/','b'};
    assert(mulib_project_package_path_n(bounded,sizeof bounded));
    assert(!mulib_project_package_path_n("a\0b",3));
    const char *bad[]={"../x","a/../b","a//b","a\\b","a:stream","/a","a/",
                       "CON","a/nul.txt","aux","COM1.wav","Lpt9","CONIN$","conout$.txt",
                       "COM\xc2\xb9","a/CON .txt","a/CON           .txt","a. ","."};
    for(size_t i=0;i<sizeof bad/sizeof bad[0];i++)assert(!mulib_project_package_path(bad[i]));
    assert(mulib_project_package_path("company/console.txt"));
    assert(mulib_project_package_path("COM10.txt"));
    uint8_t journal[128]={0};
    const uint8_t message[]={1,2,3};
    size_t used=0,off=0;
    mulib_media_bytes payload={0};
    assert(mulib_project_journal_append((mulib_media_mutbytes){journal,sizeof journal},
                                       &used,(mulib_media_bytes){message,sizeof message}));
    assert(mulib_project_journal_next((mulib_media_bytes){journal,used},&off,&payload));
    assert(off==used && payload.size==sizeof message && !memcmp(payload.data,message,sizeof message));
    for(size_t cut=0;cut<used;cut++){
        off=0;
        assert(!mulib_project_journal_next((mulib_media_bytes){journal,cut},&off,&payload));
        assert(off==0);
    }
    journal[used-1]^=1; off=0;
    assert(!mulib_project_journal_next((mulib_media_bytes){journal,used},&off,&payload) && off==0);
    unsigned calls=0; uint32_t version=1;
    mulib_project_migrate_step route[]={{1,2,migrate_step},{2,3,migrate_step},{2,4,migrate_step}};
    assert(!mulib_project_migrate(route,3,&version,4,&calls));
    assert(version==1 && calls==0); /* No partial migration on a bad route. */
    assert(mulib_project_migrate(route,2,&version,3,&calls));
    assert(version==3 && calls==2);
    mulib_project_store store={0};
    store.active=true;
    mulib_project_store_rollback(&store); /* Invalid provider cannot crash. */
    assert(store.active);
    mulib_project_history history={NULL,0,0,SIZE_MAX};
    assert(!mulib_project_history_valid(&history));
    mulib_asset_cache_entry cache[2]={0}, value={0};
    value.digest[0]=42; value.variant=1; value.value=1;
    assert(mulib_asset_cache_put(cache,2,value));
    value.value=2;
    assert(mulib_asset_cache_put(cache,2,value));
    uintptr_t found=0;
    assert(mulib_asset_cache_find(cache,2,value.digest,1,&found) && found==2);
    assert(!cache[1].occupied);
}

static void
test_parsers(void)
{
    uint8_t transport[188]={0};
    mulib_media_mpegts_packet ts;
    transport[0]=0x47; transport[3]=0x20; transport[4]=182;
    assert(!mulib_media_mpegts_parse(transport,sizeof transport,&ts));
    transport[4]=183;
    assert(mulib_media_mpegts_parse(transport,sizeof transport,&ts) && ts.payload.size==0);
    transport[3]=0x30;
    assert(!mulib_media_mpegts_parse(transport,sizeof transport,&ts));
    transport[4]=182;
    assert(mulib_media_mpegts_parse(transport,sizeof transport,&ts) && ts.payload.size==1);
    uint8_t rbsp[]={0,0,3,1,0,0,3,0};
    size_t written=99;
    assert(mulib_media_h264_rbsp((mulib_media_bytes){rbsp,sizeof rbsp},
                                (mulib_media_mutbytes){rbsp,sizeof rbsp},&written));
    const uint8_t expected[]={0,0,1,0,0,0};
    assert(written==sizeof expected && !memcmp(rbsp,expected,sizeof expected));
    assert(!mulib_media_h264_rbsp((mulib_media_bytes){rbsp,sizeof rbsp},
                                 (mulib_media_mutbytes){rbsp+1,sizeof rbsp-1},&written));
    uint8_t byte=0xff;
    unsigned bits=123; size_t pos=7;
    assert(!mulib_media_vp9_bits((mulib_media_bytes){&byte,1},&pos,2,&bits));
    assert(pos==7 && bits==123);
    pos=SIZE_MAX;
    assert(!mulib_media_vp9_bits((mulib_media_bytes){&byte,1},&pos,1,&bits));
    assert(pos==SIZE_MAX && bits==123);
    /* Deterministic independent small-buffer fuzz: successful framing must
     * produce a nonwrapping in-bounds payload and advance the byte cursor. */
    uint8_t data[64];
    for(unsigned trial=0;trial<12000;trial++){
        size_t n=random_u32()%(sizeof data+1);
        for(size_t i=0;i<n;i++)data[i]=(uint8_t)random_u32();
        mulib_media_bytes input={data,n}, nal={0};
        mulib_media_isobmff_box box;
        mulib_media_matroska_element elem;
        mulib_media_av1_obu obu;
        unsigned type=0,layer=0,temporal=0;
        size_t off=0;
        if(mulib_media_isobmff_next(input,&off,&box)){
            assert(off>0 && off<=n && box.header<=off);
            assert(box.payload.data==data+box.header && box.payload.size==off-box.header);
        }else assert(off==0);
        off=0;
        if(mulib_media_matroska_next(input,&off,&elem)){
            assert(off>0 && off<=n && elem.payload.size<=off);
            assert(elem.payload.data==data+off-elem.payload.size);
        }else assert(off==0);
        off=0;
        if(mulib_media_av1_next(input,&off,&obu)){
            assert(off>0 && off<=n && obu.payload.size<=off);
            assert(obu.payload.data==data+off-obu.payload.size);
        }else assert(off==0);
        off=0;
        if(mulib_media_h264_nal(input,&off,&nal,&type)){
            assert(off>0 && off<=n && nal.size>0 && nal.size<=off);
            assert(nal.data>=data && nal.data+nal.size<=data+off);
        }
        off=0;
        if(mulib_media_h265_nal(input,&off,&nal,&type,&layer,&temporal)){
            assert(off>0 && off<=n && nal.size>=2 && nal.size<=off);
            assert(nal.data>=data && nal.data+nal.size<=data+off);
        }
        mulib_media_vp9_header vp9;
        (void)mulib_media_vp9_parse(input,&vp9);
    }
}

int
main(void)
{
    test_time();
    test_frames();
    test_providers();
    test_timeline();
    test_projects();
    test_parsers();
    puts("Media/NLE/asset/project review tests passed");
    return 0;
}
