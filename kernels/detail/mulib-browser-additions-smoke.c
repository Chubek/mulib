/* Standalone staging smoke tests; not an upstream build target. */
#include <assert.h>
#include "mulib-bigint.h"
#include "mulib-microjs-object.h"
#include "mulib-microjs-buffer.h"
#include "mulib-microjs-coercion.h"
#include "mulib-microjs-proxy.h"
#include "mulib-wtf8.h"
#include "mulib-unicode-ident.h"
#include "mulib-html-entities.h"
#include "mulib-html-parser.h"
#include "mulib-dom-handle.h"
#include "mulib-webidl-wrappercache.h"
#include "mulib-webidl-convert.h"
#include "mulib-urlsearchparams.h"
#include "mulib-whatwg-url.h"
#include "mulib-punycode.h"
#include "mulib-publicsuffix.h"
#include "mulib-hpack.h"
#include "mulib-fetch-cors.h"
#include "mulib-fetch-headers.h"
#include "mulib-fetch-body.h"
#include "mulib-web-readablestream.h"
#include "mulib-web-abort.h"
#include "mulib-web-structuredclone.h"
#include "mulib-indexeddb-store.h"
#include "mulib-wasm-module.h"
#include "mulib-wasm-memory.h"
#include "mulib-wasm-exec.h"
#include "mulib-css-units.h"
#include "mulib-css-shorthand.h"
#include "mulib-web-canvas2d.h"
#include "mulib-browser-scriptqueue.h"
#include "mulib-browser-history.h"
#include "mulib-browser-journal.h"
#include "mulib-browser-wire.h"
#include "mulib-browser-replay.h"
#include "mulib-browser-stylestream.h"
#include "mulib-browser-subscription.h"

static int span_eq(muba_buf b,const char *s){return muba_eq((muba_span){b.data,b.len},muba_str(s));}
static void test_bigint(void){
    mulib_bigint_t a,b,c;char s[82];
    assert(mulib_bigint_parse(muba_str("18446744073709551616"),&a));
    assert(mulib_bigint_parse(muba_str("-2"),&b));
    assert(mulib_bigint_mul(&a,&b,&c));assert(mulib_bigint_format(&c,s,sizeof s));
    assert(!strcmp(s,"-36893488147419103232"));
    assert(mulib_bigint_add(&a,&b,&c));assert(mulib_bigint_format(&c,s,sizeof s));
    assert(!strcmp(s,"18446744073709551614"));
    assert(mulib_bigint_parse(muba_str("115792089237316195423570985008687907853269984665640564039457584007913129639935"),&a));
    assert(!mulib_bigint_mul(&a,&b,&c));
    assert(!mulib_bigint_parse(muba_str("115792089237316195423570985008687907853269984665640564039457584007913129639936"),&a));
}
static void test_js(void){
    mulib_microjs_object_prop props[2];mulib_microjs_object_obj a={NULL,props,0,2,1},b={&a,NULL,0,0,1};
    mulib_microjs_object_prop p={muba_str("x"),7,0,1,0};
    assert(mulib_microjs_object_define(&a,p));assert(mulib_microjs_object_get(&b,muba_str("x"),4)->value==7);
    p.value=8;assert(!mulib_microjs_object_define(&a,p));assert(!mulib_microjs_object_set_proto(&a,&b,4));
    assert(!mulib_microjs_proxy_get_allowed(&props[0],8));
    assert(mulib_microjs_coercion_int32(4294967295.0)==-1);
    assert(mulib_webidl_convert_clamp_octet(2.5)==2&&mulib_webidl_convert_clamp_octet(3.5)==4);
    mulib_microjs_buffer_t x={0},y={0};uint32_t val;
    assert(mulib_microjs_buffer_init(&x,8));assert(mulib_microjs_buffer_write_u32(&x,4,0xdeadbeef));
    assert(mulib_web_structuredclone_transfer(&x,&y));assert(!mulib_microjs_buffer_read_u32(&x,4,&val));
    assert(mulib_microjs_buffer_read_u32(&y,4,&val)&&val==0xdeadbeef);mulib_microjs_buffer_detach(&y);
}
static void test_unicode(void){
    const uint16_t s[]={0,0xd800,0x61,0xd83d,0xde00};muba_buf a={0},b={0};
    assert(mulib_wtf8_from_utf16(s,5,&a));assert(mulib_wtf8_to_utf16((muba_span){a.data,a.len},&b));
    assert(b.len==sizeof s&&!memcmp(b.data,s,sizeof s));muba_free(&a);muba_free(&b);
    assert(mulib_unicode_ident_start(0x03b1));assert(!mulib_unicode_ident_start(0x0301));assert(mulib_unicode_ident_continue(0x0301));
    assert(mulib_html_entities_decode(muba_str("&amp;&NotEqualTilde;&#x80; &notit;"),&a));
    assert(span_eq(a,"&\xe2\x89\x82\xcc\xb8\xe2\x82\xac \xc2\xacit;"));
    assert(mulib_html_entities_decode_context(muba_str("&notit; &amp;"),1,&a));assert(span_eq(a,"&notit; &"));muba_free(&a);
}
static int emit(void *ctx,mulib_html_parser_kind k,muba_span s){size_t *n=ctx;assert(s.len);assert(k>=mulib_html_parser_TEXT&&k<=mulib_html_parser_END);(*n)++;return *n!=2;}
static void test_html(void){mulib_html_parser_t p={0};p.budget=100;size_t n=0,used;
    assert(mulib_html_parser_feed(&p,muba_str("<a x=\">\">hi</a>"),1,emit,&n,&used)==0);assert(n==2);
    const char *s="<a x=\">\">hi</a>";assert(mulib_html_parser_feed(&p,muba_str(s+used),1,emit,&n,&used)==1);assert(n==3);mulib_html_parser_free(&p);}
static void test_dom(void){mulib_dom_handle_slot slots[1]={{0}};mulib_dom_handle_table t={slots,1};uint64_t a=mulib_dom_handle_insert(&t,7);assert(a&&mulib_dom_handle_get(&t,a)==7);assert(mulib_dom_handle_remove(&t,a));uint64_t b=mulib_dom_handle_insert(&t,8);assert(b!=a&&!mulib_dom_handle_get(&t,a)&&mulib_dom_handle_get(&t,b)==8);
    mulib_webidl_wrappercache_entry v[2];size_t n=0;assert(mulib_webidl_wrappercache_put(v,&n,2,(mulib_webidl_wrappercache_entry){1,2,3}));assert(mulib_webidl_wrappercache_get(v,n,1,2)==3&&!mulib_webidl_wrappercache_get(v,n,2,2));}
static void test_network(void){muba_buf b={0};assert(mulib_urlsearchparams_encode(muba_str("a b&c"),&b));assert(span_eq(b,"a+b%26c"));muba_buf d={0};assert(mulib_urlsearchparams_decode((muba_span){b.data,b.len},&d));assert(span_eq(d,"a b&c"));muba_free(&b);muba_free(&d);
    mulib_whatwg_url_t u;assert(mulib_whatwg_url_parse(muba_str("https://example.com/a?q#f"),&u));assert(muba_eq(u.query,muba_str("q")));assert(!mulib_whatwg_url_parse(muba_str("https://u@h/"),&u));
    const uint32_t cp[]={98,0xfc,99,104,101,114};assert(mulib_punycode_encode(cp,6,&b));assert(span_eq(b,"bcher-kva"));muba_free(&b);
    const char *rules[]={"com","*.ck","!www.ck"};assert(mulib_publicsuffix_count(muba_str("x.a.ck"),rules,3)==2);assert(mulib_publicsuffix_count(muba_str("www.ck"),rules,3)==1);
    assert(mulib_hpack_encode(1337,5,0,&b));uint64_t val;size_t n;assert(mulib_hpack_decode((muba_span){b.data,b.len},5,&val,&n)&&val==1337&&n==b.len);assert(!mulib_hpack_decode((muba_span){b.data,1},5,&val,&n));muba_free(&b);
    assert(mulib_fetch_cors_allow(muba_str("https://a"),muba_str("*"),0,muba_str("")));assert(!mulib_fetch_cors_allow(muba_str("https://a"),muba_str("*"),1,muba_str("true")));
    assert(!mulib_fetch_headers_valid((mulib_fetch_headers_field){muba_str("x"),muba_str("a\r\nb")}));
    mulib_fetch_body_t body={0};assert(mulib_fetch_body_init(&body,muba_str("abc")));assert(mulib_fetch_body_consume(&body,&b)&&span_eq(b,"abc"));assert(!mulib_fetch_body_consume(&body,&b));muba_free(&b);}
static void abort_cb(void *ctx,uintptr_t reason){*(uintptr_t*)ctx=reason;}
static void test_web(void){mulib_web_readablestream_t s={0};s.limit=3;assert(mulib_web_readablestream_push(&s,muba_str("abc")));assert(!mulib_web_readablestream_push(&s,muba_str("d")));unsigned char out[4];size_t n;assert(mulib_web_readablestream_read(&s,out,2,&n)==1&&n==2&&!memcmp(out,"ab",2));mulib_web_readablestream_close(&s);assert(mulib_web_readablestream_read(&s,out,4,&n)==1&&n==1);assert(mulib_web_readablestream_read(&s,out,4,&n)==0);mulib_web_readablestream_free(&s);
    mulib_web_abort_listener v[1];mulib_web_abort_t a={v,0,1,0,0};uintptr_t reason=0;assert(mulib_web_abort_listen(&a,abort_cb,&reason));assert(mulib_web_abort_abort(&a,42)&&reason==42);assert(!mulib_web_abort_abort(&a,43));}
static void test_storage_wasm(void){mulib_indexeddb_store_record rec[2];mulib_indexeddb_store_t s={rec,0,2};mulib_indexeddb_key_t a={mulib_indexeddb_key_NUMBER,2,{0}},b={mulib_indexeddb_key_NUMBER,1,{0}};assert(mulib_indexeddb_store_put(&s,a,42,0));assert(mulib_indexeddb_store_put(&s,b,41,0));assert(s.v[0].value==41&&!mulib_indexeddb_store_put(&s,a,1,0));
    unsigned char wasm[]={0,97,115,109,1,0,0,0,1,1,0};mulib_wasm_module_t mod;assert(mulib_wasm_module_read((muba_span){wasm,sizeof wasm},&mod));assert(mod.present==2);assert(!mulib_wasm_module_read((muba_span){wasm,sizeof wasm-1},&mod));
    mulib_wasm_memory_t m={0};m.max=2;uint32_t old;assert(mulib_wasm_memory_grow(&m,1,&old)&&old==0);unsigned char x=1;assert(mulib_wasm_memory_read(&m,65535,&x,1)&&x==0);assert(!mulib_wasm_memory_write(&m,65536,&x,1));assert(!mulib_wasm_memory_grow(&m,2,&old));mulib_wasm_memory_free(&m);int32_t q;assert(!mulib_wasm_exec_div_s(INT32_MIN,-1,&q));}
static int replay_cb(void *ctx,const mulib_browser_journal_event *e){uint64_t *last=ctx;assert(e->seq>*last);*last=e->seq;return 1;}
static void test_browser(void){double x;assert(mulib_css_units_px(2.54,muba_str("cm"),0,0,0,0,0,&x)&&fabs(x-96)<1e-10);muba_span v[]={muba_str("a"),muba_str("b")},o[4];assert(mulib_css_shorthand_sides(v,2,o)&&muba_eq(o[3],v[1]));
    unsigned char pixels[16]={0},color[]={1,2,3,4},out[4];mulib_web_canvas2d_t canvas={pixels,2,2,8,16};assert(mulib_web_canvas2d_fill(&canvas,1,0,1,2,color));assert(mulib_web_canvas2d_pixel(&canvas,1,1,out)&&!memcmp(color,out,4));assert(!mulib_web_canvas2d_fill(&canvas,1,0,2,2,color));
    mulib_browser_history_entry h[3];mulib_browser_history_t history={h,0,3,0};assert(mulib_browser_history_push(&history,(mulib_browser_history_entry){1,0,muba_str("a")}));assert(mulib_browser_history_push(&history,(mulib_browser_history_entry){2,0,muba_str("b")}));assert(mulib_browser_history_go(&history,-1));assert(mulib_browser_history_push(&history,(mulib_browser_history_entry){3,0,muba_str("c")}));assert(history.len==2&&history.v[1].document==3);
    mulib_browser_journal_event events[2];mulib_browser_journal_t j={events,0,2,0,8,0};assert(mulib_browser_stylestream_emit(&j,1,1,2,muba_str("abc")));assert(mulib_browser_journal_append(&j,7,2,1,2,muba_str("def")));assert(!mulib_browser_journal_append(&j,7,2,1,2,muba_str("x")));uint64_t cursor=0,last=0;assert(mulib_browser_replay_run(&j,&cursor,replay_cb,&last)&&cursor==2&&last==2);
    mulib_browser_subscription_t sub={0,UINT64_C(1)<<6};last=0;assert(mulib_browser_subscription_poll(&sub,&j,replay_cb,&last)&&sub.after==2&&last==1);
    mulib_browser_wire_event e={1,1,2,6,1,muba_str("abc")},d;muba_buf b={0};size_t used;assert(mulib_browser_wire_encode(e,&b));assert(mulib_browser_wire_decode((muba_span){b.data,b.len},8,&d,&used)&&d.node==2&&muba_eq(d.payload,e.payload)&&used==b.len);assert(!mulib_browser_wire_decode((muba_span){b.data,b.len},2,&d,&used));muba_free(&b);mulib_browser_journal_clear(&j);}
int main(void){test_bigint();test_js();test_unicode();test_html();test_dom();test_network();test_web();test_storage_wasm();test_browser();puts("MuLIB browser staging smoke: 9 groups passed");return 0;}
