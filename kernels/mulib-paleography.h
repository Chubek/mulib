/** @file mulib-paleography.h
 * @brief Native/Lua script profiles, vector glyphs, SVG fonts, CSS and hybrid
 * LaTeX packages/encodings for historical or invented scripts.
 * Define MULIB_IMPL_PALEOGRAPHY plus MUNE, JSON, SVG, BASE64, TEX and dependencies.
 */
#ifndef MULIB_PALEOGRAPHY_H
#define MULIB_PALEOGRAPHY_H
#include "mulib-mune.h"
#include "mulib-json.h"
#include "mulib-svg.h"
#include "mulib-base64.h"
#include "mulib-tex.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_paleography mulib_paleography_t;
typedef struct mulib_paleography_error {size_t offset;char message[256];} mulib_paleography_error_t;
/** Explicit host font-outline service. Append an absolute M/L/Q/C/Z path in
 * y-up font units and set advance; return nonzero. Borrowed arguments/builder.
 * No font engine, file access or third-party runtime is implicitly loaded. */
typedef int (*mulib_paleography_native_fn)(void *,const char *font,uint32_t scalar,mulib_strbuf_t *path,double *advance);
typedef struct mulib_paleography_options {
    const char *id,*family,*native_font,*web_font_url,*web_font_format;
    unsigned units_per_em; double ascent,descent;
    mulib_paleography_native_fn native;void *user;
    mulib_mune_runtime_options_t lua;
} mulib_paleography_options_t;
typedef struct mulib_paleography_glyph {const char *name,*path;uint32_t scalar;double advance;} mulib_paleography_glyph_t;
typedef struct mulib_paleography_layout {double font_size,line_height;const char *direction;} mulib_paleography_layout_t;
/** @brief Copy options/create independent state. lpaleo auto-loads; defaults are id
 * Paleo, family Paleo, 1000 units/em, ascent800/descent200. No global mutation. */
mulib_paleography_t *mulib_paleography_create(const mulib_paleography_options_t *,mulib_paleography_error_t *);
/** @brief Destroy state/glyphs; NULL safe, no-op during an active operation. */
void mulib_paleography_destroy(mulib_paleography_t *);
/** @brief Copy a unique glyph/name/scalar. Scalar zero allocates a free BMP PUA scalar.
 * Empty/NULL path means native Unicode-font fallback. <=4096 glyphs, 64 KiB
 * path, <=4096 explicit absolute M/L/Q/C/Z commands, finite coordinates.
 * 1/0, atomic; advance is in [0,4*units_per_em]. */
int mulib_paleography_define(mulib_paleography_t *,const mulib_paleography_glyph_t *,mulib_paleography_error_t *);
/** @brief Import through the explicitly configured native callback; same ownership. */
int mulib_paleography_import(mulib_paleography_t *,const char *name,uint32_t scalar,const char *font,mulib_paleography_error_t *);
/** @brief Number of registered glyphs; zero for NULL. */
size_t mulib_paleography_count(const mulib_paleography_t *);
/** @brief Borrow immutable glyph descriptor; stable until context destruction. */
const mulib_paleography_glyph_t *mulib_paleography_glyph(const mulib_paleography_t *,const char *name);
/** @brief Run lpaleo.script/define/native extensions (<=1 MiB). 1/0; preceding Lua
 * and glyph-registration side effects persist on error. No reentrancy. */
int mulib_paleography_extend(mulib_paleography_t *,const char *,size_t,mulib_paleography_error_t *);
/** @brief Borrow state for explicit host bindings; NULL during an operation. */
mulib_mune_state_t *mulib_paleography_lua(mulib_paleography_t *);
/** @brief Longest-match Lua transcription (identity/default, elder-futhark, greek or
 * custom script). Owned SVG with paths or native font text fallback. Direction
 * ltr/rtl/ttb/boustrophedon; this is not OpenType shaping or Unicode bidi. */
mulib_xml_node_t *mulib_paleography_svg(mulib_paleography_t *,const char *,size_t,const char *script,const mulib_paleography_layout_t *,mulib_paleography_error_t *);
/** @brief Owned SVG font resource containing vector-defined glyphs. SVG font format
 * is legacy; use svg() for modern path output or supply a WOFF/OTF URL for CSS. */
mulib_xml_node_t *mulib_paleography_font(const mulib_paleography_t *,mulib_paleography_error_t *);
/** @brief malloc-owned CSS @font-face (provided URL or embedded SVG font). */
char *mulib_paleography_css(const mulib_paleography_t *,size_t *,mulib_paleography_error_t *);
/** @brief malloc-owned LaTeX .sty hybrid: PGF outlines plus optional fontspec. */
char *mulib_paleography_package(const mulib_paleography_t *,size_t *,mulib_paleography_error_t *);
/** @brief malloc-owned 8-bit PostScript .enc; <=255 glyphs, no font compiler. */
char *mulib_paleography_encoding(const mulib_paleography_t *,size_t *,mulib_paleography_error_t *);
/** @brief Transcribe to package glyph commands/native Unicode text. Unknown glyphs
 * require a configured native_font, rather than silently inventing an outline. */
char *mulib_paleography_latex(mulib_paleography_t *,const char *,size_t,const char *script,size_t *,mulib_paleography_error_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PALEOGRAPHY) && !defined(MULIB_PALEOGRAPHY_IMPLEMENTED)
#define MULIB_PALEOGRAPHY_IMPLEMENTED
#include "mulib-mune-markup.inc"
#include "mulib-paleography-data.inc"
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_paleography {mulib_paleography_options_t options;char *id,*family,*font,*url,*format;mulib_paleography_glyph_t **glyphs;size_t count,glyph_bytes;mulib_mune_state_t *state;int busy,destroying;};
static int mulib_paleography_error_(mulib_paleography_error_t *e,size_t at,const char *m){if(e){e->offset=at;(void)snprintf(e->message,sizeof e->message,"%s",m?m:"");}return 0;}
static char *mulib_paleography_copy_(const char *s){size_t n=strlen(s);char *p=(char *)malloc(n+1);if(p)memcpy(p,s,n+1);return p;}
static int mulib_paleography_name_(const char *s,int letters){size_t i;if(!s||!*s||strlen(s)>63)return 0;for(i=0;s[i];++i)if(!((s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(!letters&&i&&((s[i]>='0'&&s[i]<='9')||s[i]=='_'))))return 0;return 1;}
typedef struct mulib_paleography_path_ {const char *s;size_t at,length,count;double x,y;int open;} mulib_paleography_path_;
static void mulib_paleography_skip_(mulib_paleography_path_ *p){while(p->at<p->length&&(p->s[p->at]==' '||p->s[p->at]=='\t'||p->s[p->at]=='\r'||p->s[p->at]=='\n'||p->s[p->at]==','))++p->at;}
static int mulib_paleography_decimal_(mulib_paleography_path_ *p,double *out){double v=0,scale=0.1;int negative=0,digits=0;mulib_paleography_skip_(p);if(p->at<p->length&&(p->s[p->at]=='-'||p->s[p->at]=='+'))negative=p->s[p->at++]=='-';while(p->at<p->length&&p->s[p->at]>='0'&&p->s[p->at]<='9'){digits=1;v=v*10+(p->s[p->at++]-'0');if(v>1000000)return 0;}if(p->at<p->length&&p->s[p->at]=='.'){++p->at;while(p->at<p->length&&p->s[p->at]>='0'&&p->s[p->at]<='9'){digits=1;v+=(p->s[p->at++]-'0')*scale;scale*=0.1;}}if(!digits||!isfinite(v)||v>1000000)return 0;*out=negative?-v:v;return 1;}
/** One absolute command; 1 command, 0 end, -1 invalid. */
static int mulib_paleography_path_next_(mulib_paleography_path_ *p,char *command,double values[6]){
    size_t i,n;mulib_paleography_skip_(p);if(p->at==p->length)return 0;
    *command=p->s[p->at++];n=*command=='M'||*command=='L'?2:*command=='Q'?4:*command=='C'?6:*command=='Z'?0:99;
    if(n==99||++p->count>4096||(*command!='M'&&!p->open))return -1;
    for(i=0;i<n;++i)if(!mulib_paleography_decimal_(p,&values[i]))return -1;
    if(*command=='Z')p->open=0;else{p->open=1;p->x=values[n-2];p->y=values[n-1];}return 1;
}
static int mulib_paleography_path_valid_(const char *s){mulib_paleography_path_ p;char cmd;double values[6];int next;memset(&p,0,sizeof p);p.s=s;p.length=strlen(s);if(p.length>65536)return 0;while((next=mulib_paleography_path_next_(&p,&cmd,values))==1){}return next==0;}
size_t mulib_paleography_count(const mulib_paleography_t *c){return c?c->count:0;}
const mulib_paleography_glyph_t *mulib_paleography_glyph(const mulib_paleography_t *c,const char *name){size_t i;if(!c||!name)return NULL;for(i=0;i<c->count;++i)if(!strcmp(name,c->glyphs[i]->name))return c->glyphs[i];return NULL;}
static const mulib_paleography_glyph_t *mulib_paleography_scalar_(const mulib_paleography_t *c,uint32_t scalar){size_t i;for(i=0;i<c->count;++i)if(c->glyphs[i]->scalar==scalar)return c->glyphs[i];return NULL;}
static int mulib_paleography_define_(mulib_paleography_t *c,const mulib_paleography_glyph_t *g,mulib_paleography_error_t *e){
    mulib_paleography_glyph_t *copy,**items;uint32_t scalar;unsigned char utf8[4];size_t n;
    if(!c||!g||c->destroying||c->count==4096||!mulib_paleography_name_(g->name,0)||mulib_paleography_glyph(c,g->name)||!isfinite(g->advance)||g->advance<0||g->advance>c->options.units_per_em*4.0||
        (g->path&&!mulib_paleography_path_valid_(g->path)))return mulib_paleography_error_(e,0,"invalid/duplicate/bounded paleography glyph/path");
    if(strlen(g->name)+(g->path?strlen(g->path):0)>8*1024*1024-c->glyph_bytes)return mulib_paleography_error_(e,0,"paleography total glyph byte limit");
    scalar=g->scalar;if(!scalar){scalar=0xe000;while(scalar<=0xf8ff&&mulib_paleography_scalar_(c,scalar))++scalar;}
    n=mulib_utf8_encode(scalar,utf8,sizeof utf8);if(!n||scalar<32||(scalar>=0x7f&&scalar<=0x9f)||!mulib_xml_valid_text((const char *)utf8,n)||mulib_paleography_scalar_(c,scalar))return mulib_paleography_error_(e,0,"invalid/duplicate glyph Unicode scalar");
    copy=(mulib_paleography_glyph_t *)calloc(1,sizeof *copy);if(!copy)goto memory;copy->name=mulib_paleography_copy_(g->name);copy->path=mulib_paleography_copy_(g->path?g->path:"");copy->scalar=scalar;copy->advance=g->advance;
    if(!copy->name||!copy->path)goto fail;
    items=(mulib_paleography_glyph_t **)realloc(c->glyphs,(c->count+1)*sizeof *items);if(!items)goto fail;c->glyphs=items;c->glyphs[c->count++]=copy;c->glyph_bytes+=strlen(copy->name)+strlen(copy->path);return 1;
fail:free((void *)copy->name);free((void *)copy->path);free(copy);
memory:return mulib_paleography_error_(e,0,"paleography glyph allocation failure");
}
int mulib_paleography_define(mulib_paleography_t *c,const mulib_paleography_glyph_t *g,mulib_paleography_error_t *e){(void)mulib_paleography_error_(e,0,NULL);return c&&!c->busy?mulib_paleography_define_(c,g,e):mulib_paleography_error_(e,0,"invalid/reentrant paleography registration");}
static int mulib_paleography_import_(mulib_paleography_t *c,const char *name,uint32_t scalar,const char *font,mulib_paleography_error_t *e){mulib_strbuf_t path={0};mulib_paleography_glyph_t glyph;int ok;
    if(c->destroying||!c->options.native||!font)return mulib_paleography_error_(e,0,"no active native font provider configured");
    memset(&glyph,0,sizeof glyph);glyph.name=name;glyph.scalar=scalar;
    if(!c->options.native(c->options.user,font,scalar,&path,&glyph.advance)){mulib_strbuf_free(&path);return mulib_paleography_error_(e,0,"native font provider failed");}
    glyph.path=path.data;ok=mulib_paleography_define_(c,&glyph,e);mulib_strbuf_free(&path);return ok;
}
int mulib_paleography_import(mulib_paleography_t *c,const char *name,uint32_t scalar,const char *font,mulib_paleography_error_t *e){(void)mulib_paleography_error_(e,0,NULL);if(!c||c->busy)return mulib_paleography_error_(e,0,"invalid/reentrant native import");c->busy=1;{int ok=mulib_paleography_import_(c,name,scalar,font,e);c->busy=0;return ok;}}
static mulib_mune_status_t mulib_paleography_native_define_(mulib_mune_state_t *s,const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *r,void *user){
    mulib_paleography_glyph_t g;mulib_mune_value_t v;mulib_paleography_t *c=(mulib_paleography_t *)user;memset(&g,0,sizeof g);
    if(n!=1||!mulib_munexml_get_(s,a[0],"name",&v)||!(g.name=mulib_munexml_string_(v)))return MULIB_MUNE_INVALID;
    if(!mulib_munexml_get_(s,a[0],"path",&v))return MULIB_MUNE_INVALID;
    g.path=v.type==MULIB_MUNE_NIL?NULL:mulib_munexml_string_(v);if(v.type!=MULIB_MUNE_NIL&&!g.path)return MULIB_MUNE_INVALID;
    if(!mulib_munexml_get_(s,a[0],"codepoint",&v))return MULIB_MUNE_INVALID;
    if(v.type!=MULIB_MUNE_NIL){if(v.type!=MULIB_MUNE_INTEGER||v.as.integer<0||v.as.integer>0x10ffff)return MULIB_MUNE_INVALID;g.scalar=(uint32_t)v.as.integer;}
    if(!mulib_munexml_get_(s,a[0],"advance",&v))return MULIB_MUNE_INVALID;
    g.advance=v.type==MULIB_MUNE_NIL?c->options.units_per_em:v.type==MULIB_MUNE_INTEGER?(double)v.as.integer:v.type==MULIB_MUNE_NUMBER?v.as.number:-1;
    if(!mulib_paleography_define_(c,&g,NULL))return MULIB_MUNE_INVALID;
    return mulib_mune_result_push(r,mulib_mune_integer((int64_t)c->glyphs[c->count-1]->scalar))?MULIB_MUNE_OK:MULIB_MUNE_MEMORY;
}
static mulib_mune_status_t mulib_paleography_native_import_(mulib_mune_state_t *s,const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *r,void *user){
    const char *name=n==3?mulib_munexml_string_(a[0]):NULL,*font=n==3?mulib_munexml_string_(a[2]):NULL;
    if(!name||!font||a[1].type!=MULIB_MUNE_INTEGER||a[1].as.integer<0||a[1].as.integer>0x10ffff||!mulib_paleography_import_((mulib_paleography_t *)user,name,(uint32_t)a[1].as.integer,font,NULL))return MULIB_MUNE_INVALID;
    (void)s;return mulib_mune_result_push(r,mulib_mune_boolean(1))?MULIB_MUNE_OK:MULIB_MUNE_MEMORY;
}
mulib_paleography_t *mulib_paleography_create(const mulib_paleography_options_t *options,mulib_paleography_error_t *e){
    mulib_paleography_options_t o={0};mulib_paleography_t *c=NULL;mulib_mune_error_t me={0};(void)mulib_paleography_error_(e,0,NULL);if(options)o=*options;
    if(!o.id)o.id="Paleo";
    if(!o.family)o.family=o.id;
    if(!o.units_per_em)o.units_per_em=1000;
    if(!o.ascent&&!o.descent){o.ascent=o.units_per_em*.8;o.descent=o.units_per_em*.2;}
    if(!mulib_paleography_name_(o.id,1)||strlen(o.family)>127||!mulib_xml_valid_text(o.family,strlen(o.family))||o.units_per_em<16||o.units_per_em>16384||!isfinite(o.ascent)||!isfinite(o.descent)||o.ascent<0||o.descent<0||o.ascent+o.descent<=0||o.ascent+o.descent>4.0*o.units_per_em||
        (o.native_font&&(strlen(o.native_font)>255||strpbrk(o.native_font,"\\{}[]%#$\r\n")||!mulib_xml_valid_text(o.native_font,strlen(o.native_font))))||
        (o.web_font_url&&(strlen(o.web_font_url)>4096||!mulib_xml_valid_text(o.web_font_url,strlen(o.web_font_url))||strpbrk(o.web_font_url,"\r\n")))||
        (o.web_font_format&&strcmp(o.web_font_format,"woff2")&&strcmp(o.web_font_format,"woff")&&strcmp(o.web_font_format,"opentype")&&strcmp(o.web_font_format,"truetype")&&strcmp(o.web_font_format,"svg"))){(void)mulib_paleography_error_(e,0,"invalid paleography/font options");return NULL;}
    c=(mulib_paleography_t *)calloc(1,sizeof *c);if(!c)goto memory;c->options=o;
    c->id=mulib_paleography_copy_(o.id);c->family=mulib_paleography_copy_(o.family);c->font=mulib_paleography_copy_(o.native_font?o.native_font:"");c->url=mulib_paleography_copy_(o.web_font_url?o.web_font_url:"");c->format=mulib_paleography_copy_(o.web_font_format?o.web_font_format:"woff2");
    if(!c->id||!c->family||!c->font||!c->url||!c->format)goto memory;
    c->options.id=c->id;c->options.family=c->family;c->options.native_font=c->font;c->options.web_font_url=c->url;c->options.web_font_format=c->format;
    c->state=mulib_munexml_state_(&o.lua);if(!c->state)goto memory;
    if(mulib_mune_bind(c->state,"__mulib_paleo_define",mulib_paleography_native_define_,c)||mulib_mune_bind(c->state,"__mulib_paleo_native",mulib_paleography_native_import_,c))goto memory;
    if(!mulib_munexml_bootstrap_(c->state,mulib_paleography_source_,sizeof mulib_paleography_source_/sizeof mulib_paleography_source_[0],"lpaleo",&me)){(void)mulib_paleography_error_(e,me.offset,me.message?me.message:"lpaleo bootstrap failed");mulib_paleography_destroy(c);return NULL;}return c;
memory:(void)mulib_paleography_error_(e,0,"paleography/Mune allocation failure");mulib_paleography_destroy(c);return NULL;
}
void mulib_paleography_destroy(mulib_paleography_t *c){size_t i;if(!c||c->busy)return;c->destroying=1;c->busy=1;mulib_mune_state_destroy(c->state);for(i=0;i<c->count;++i){free((void *)c->glyphs[i]->name);free((void *)c->glyphs[i]->path);free(c->glyphs[i]);}free(c->glyphs);free(c->id);free(c->family);free(c->font);free(c->url);free(c->format);free(c);}
mulib_mune_state_t *mulib_paleography_lua(mulib_paleography_t *c){return c&&!c->busy?c->state:NULL;}
int mulib_paleography_extend(mulib_paleography_t *c,const char *source,size_t length,mulib_paleography_error_t *e){mulib_mune_result_t *r=NULL;mulib_mune_error_t me;int ok;(void)mulib_paleography_error_(e,0,NULL);if(!c||c->busy||length>1024*1024||!mulib_xml_valid_text(source,length))return mulib_paleography_error_(e,0,"invalid/reentrant/oversized paleography extension");c->busy=1;ok=!mulib_mune_run_source(c->state,source,length,&r,&me);if(!ok)(void)mulib_paleography_error_(e,me.offset,me.message);mulib_mune_result_destroy(r);c->busy=0;return ok;}
static int mulib_paleography_numattr_(mulib_xml_node_t *n,const char *key,double v){char b[128];return mulib_munexml_number_(b,v)&&mulib_xml_set_attr(n,key,b);}
static int mulib_paleography_append_(mulib_xml_node_t *p,mulib_xml_node_t *n){if(n&&mulib_xml_append(p,n))return 1;mulib_xml_destroy(n);return 0;}
mulib_xml_node_t *mulib_paleography_font(const mulib_paleography_t *c,mulib_paleography_error_t *e){
    mulib_xml_node_t *doc=NULL,*root,*defs,*font,*face,*glyph;size_t i;(void)mulib_paleography_error_(e,0,NULL);if(!c)goto fail;
    doc=mulib_svg_document(c->options.units_per_em,c->options.units_per_em);if(!doc)goto fail;root=mulib_xml_root(doc);
    defs=mulib_xml_new(MULIB_XML_ELEMENT,"defs");if(!mulib_paleography_append_(root,defs))goto fail;
    font=mulib_xml_new(MULIB_XML_ELEMENT,"font");if(!mulib_paleography_append_(defs,font))goto fail;
    if(!mulib_xml_set_attr(font,"id",c->id)||!mulib_paleography_numattr_(font,"horiz-adv-x",c->options.units_per_em))goto fail;
    face=mulib_xml_new(MULIB_XML_ELEMENT,"font-face");if(!mulib_paleography_append_(font,face)||!mulib_xml_set_attr(face,"font-family",c->family)||!mulib_paleography_numattr_(face,"units-per-em",c->options.units_per_em)||!mulib_paleography_numattr_(face,"ascent",c->options.ascent)||!mulib_paleography_numattr_(face,"descent",-c->options.descent))goto fail;
    for(i=0;i<c->count;++i){unsigned char bytes[5];size_t n;if(!*c->glyphs[i]->path)continue;n=mulib_utf8_encode(c->glyphs[i]->scalar,bytes,4);bytes[n]=0;
        glyph=mulib_xml_new(MULIB_XML_ELEMENT,"glyph");if(!mulib_paleography_append_(font,glyph)||!mulib_xml_set_attr(glyph,"glyph-name",c->glyphs[i]->name)||!mulib_xml_set_attr(glyph,"unicode",(const char *)bytes)||!mulib_xml_set_attr(glyph,"d",c->glyphs[i]->path)||!mulib_paleography_numattr_(glyph,"horiz-adv-x",c->glyphs[i]->advance))goto fail;}
    root=mulib_xml_detach(root);mulib_xml_destroy(doc);return root;
fail:mulib_xml_destroy(doc);(void)mulib_paleography_error_(e,0,"SVG font allocation/argument failure");return NULL;
}
static mulib_mune_result_t *mulib_paleography_shape_(mulib_paleography_t *c,const char *text,size_t length,const char *script,mulib_mune_value_t *list,const char **direction,size_t *count,mulib_paleography_error_t *e){
    mulib_mune_result_t *roots=NULL,*r=NULL;mulib_mune_value_t module,fn,args[2],v;mulib_mune_error_t me;
    if(length>1024*1024||!mulib_xml_valid_text(text,length)||!script||strlen(script)>63){(void)mulib_paleography_error_(e,0,"invalid/oversized paleography text/script");return NULL;}
    roots=mulib_mune_result_create(c->state);if(!roots||mulib_mune_make_string(c->state,text?text:"",length,&args[0])||!mulib_mune_result_push(roots,args[0])||mulib_mune_make_string(c->state,script,strlen(script),&args[1])||!mulib_mune_result_push(roots,args[1])||mulib_mune_get_global(c->state,"lpaleo",&module)||!mulib_munexml_get_(c->state,module,"shape",&fn))goto fail;
    if(mulib_mune_call(c->state,fn,args,2,&r,&me)){(void)mulib_paleography_error_(e,me.offset,me.message);goto fail;}
    v=mulib_mune_result_get(r,0);if(!mulib_munexml_get_(c->state,v,"glyphs",list)||!mulib_munexml_array_(c->state,*list,count,16384)||!mulib_munexml_get_(c->state,v,"direction",&v)||!(*direction=mulib_munexml_string_(v)))goto fail;
    mulib_mune_result_destroy(roots);return r;
fail:if(e&&!e->message[0])(void)mulib_paleography_error_(e,0,"invalid paleography shape result/bridge allocation failure");mulib_mune_result_destroy(roots);mulib_mune_result_destroy(r);return NULL;
}
static const mulib_paleography_glyph_t *mulib_paleography_lookup_(const mulib_paleography_t *c,const char *token,uint32_t *scalar){const mulib_paleography_glyph_t *g=mulib_paleography_glyph(c,token);size_t used;if(g){*scalar=g->scalar;return g;}*scalar=0;if(mulib_utf8_decode((const unsigned char *)token,strlen(token),scalar,&used)&&used==strlen(token))return mulib_paleography_scalar_(c,*scalar);*scalar=0;return NULL;}
mulib_xml_node_t *mulib_paleography_svg(mulib_paleography_t *c,const char *text,size_t length,const char *script,const mulib_paleography_layout_t *layout,mulib_paleography_error_t *e){
    mulib_mune_result_t *result=NULL;mulib_mune_value_t list,v;mulib_xml_node_t *doc=NULL,*root,*group,*linegroup,*shape;const char *direction,*token;double size=layout&&layout->font_size?layout->font_size:32,lineheight=layout&&layout->line_height?layout->line_height:1.4;
    size_t count,i,line=0;double x=0,y=0,maxx=0,maxy=0;int rtl=0,vertical=0,boustro=0;char transform[1024],a[128],b[128],scale[128];
    (void)mulib_paleography_error_(e,0,NULL);if(!c||c->busy||!isfinite(size)||size<=0||size>1000||!isfinite(lineheight)||lineheight<.1||lineheight>10){(void)mulib_paleography_error_(e,0,"invalid/reentrant paleography layout");return NULL;}c->busy=1;
    result=mulib_paleography_shape_(c,text,length,script?script:"identity",&list,&direction,&count,e);if(!result)goto fail;if(layout&&layout->direction)direction=layout->direction;
    rtl=!strcmp(direction,"rtl");vertical=!strcmp(direction,"ttb");boustro=!strcmp(direction,"boustrophedon");if(!rtl&&!vertical&&!boustro&&strcmp(direction,"ltr"))goto fail;
    doc=mulib_svg_document(1,1);if(!doc)goto fail;root=mulib_xml_root(doc);if(!mulib_xml_set_attr(root,"role","img"))goto fail;
    group=mulib_svg_group(root,NULL);if(!group||!mulib_xml_set_attr(group,"font-family",c->family))goto fail;
    linegroup=mulib_svg_group(group,NULL);if(!linegroup)goto fail;
    for(i=1;i<=count;++i){const mulib_paleography_glyph_t *g;uint32_t scalar;double advance,sx,sy;int backwards=rtl||(boustro&&(line&1));
        if(mulib_mune_raw_get(c->state,list,mulib_mune_integer((int64_t)i),&v)||(token=mulib_munexml_string_(v))==NULL)goto fail;
        if(!strcmp(token,"\r"))continue;
        if(!strcmp(token,"\n")){x=0;y+=size*lineheight;++line;linegroup=mulib_svg_group(group,NULL);if(!linegroup)goto fail;continue;}
        g=mulib_paleography_lookup_(c,token,&scalar);if(!g&&!scalar)goto fail;advance=(g?g->advance/c->options.units_per_em:scalar==' '?0.35:0.65)*size;
        sx=backwards?-x-advance:x;sy=y+size*c->options.ascent/c->options.units_per_em;
        if(g&&*g->path){shape=mulib_svg_path(linegroup,g->path,"currentColor",NULL);if(!shape||!mulib_munexml_number_(a,sx)||!mulib_munexml_number_(b,sy)||!mulib_munexml_number_(scale,size/c->options.units_per_em))goto fail;(void)snprintf(transform,sizeof transform,"translate(%s %s) scale(%s -%s)",a,b,scale,scale);if(!mulib_xml_set_attr(shape,"transform",transform))goto fail;}
        else{unsigned char bytes[5];size_t n=mulib_utf8_encode(scalar,bytes,4);bytes[n]=0;shape=mulib_svg_text(linegroup,sx,sy,(const char *)bytes,"currentColor");if(!shape||!mulib_paleography_numattr_(shape,"font-size",size))goto fail;}
        maxy=fmax(maxy,y+size*(c->options.ascent+c->options.descent)/c->options.units_per_em);
        if(vertical){y+=size*lineheight;maxx=fmax(maxx,advance);}else{x+=advance;maxx=fmax(maxx,x);}
    }
    if(maxx<1)maxx=size;
    if(maxy<1)maxy=size;
    if(rtl||boustro){if(!mulib_munexml_number_(a,maxx))goto fail;(void)snprintf(transform,sizeof transform,"translate(%s 0)",a);line=0;for(linegroup=mulib_xml_first(group);linegroup;linegroup=mulib_xml_next(linegroup),++line)if((rtl||(line&1))&&!mulib_xml_set_attr(linegroup,"transform",transform))goto fail;}
    {char w[128],h[128],view[270];if(!mulib_munexml_number_(w,maxx)||!mulib_munexml_number_(h,maxy))goto fail;(void)snprintf(view,sizeof view,"0 0 %s %s",w,h);if(!mulib_xml_set_attr(root,"width",w)||!mulib_xml_set_attr(root,"height",h)||!mulib_xml_set_attr(root,"viewBox",view))goto fail;}
    root=mulib_xml_detach(root);mulib_xml_destroy(doc);mulib_mune_result_destroy(result);c->busy=0;return root;
fail:mulib_xml_destroy(doc);mulib_mune_result_destroy(result);c->busy=0;if(e&&!e->message[0])(void)mulib_paleography_error_(e,0,"unknown glyph/direction or SVG allocation failure");return NULL;
}
static int mulib_paleography_quote_(mulib_strbuf_t *b,const char *text){size_t i;if(!mulib_strbuf_putc(b,'"'))return 0;for(i=0;text[i];++i){if((unsigned char)text[i]<32)return 0;if((text[i]=='"'||text[i]=='\\')&&!mulib_strbuf_putc(b,'\\'))return 0;if(!mulib_strbuf_putc(b,text[i]))return 0;}return mulib_strbuf_putc(b,'"');}
char *mulib_paleography_css(const mulib_paleography_t *c,size_t *length,mulib_paleography_error_t *e){mulib_strbuf_t b={0};mulib_xml_node_t *font=NULL;char *svg=NULL,*base64=NULL;size_t n,capacity;(void)mulib_paleography_error_(e,0,NULL);if(length)*length=0;if(!c)goto fail;
    if(!mulib_strbuf_append(&b,"@font-face{font-family:")||!mulib_paleography_quote_(&b,c->family)||!mulib_strbuf_append(&b,";src:url("))goto fail;
    if(*c->url){if(!mulib_paleography_quote_(&b,c->url))goto fail;}
    else{font=mulib_paleography_font(c,e);if(!font)goto fail;svg=mulib_xml_serialize(font,&n);if(!svg||n>(SIZE_MAX-4)/4*3)goto fail;capacity=(n+2)/3*4+1;base64=(char *)malloc(capacity);if(!base64||!mulib_base64_encode(svg,n,base64,capacity))goto fail;if(!mulib_strbuf_append(&b,"\"data:image/svg+xml;base64,")||!mulib_strbuf_append(&b,base64)||!mulib_strbuf_putc(&b,'#')||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_putc(&b,'"'))goto fail;}
    if(!mulib_strbuf_append(&b,") format(")||!mulib_paleography_quote_(&b,*c->url?c->format:"svg")||!mulib_strbuf_append(&b,");font-style:normal;font-weight:normal;}\n"))goto fail;
    free(svg);free(base64);mulib_xml_destroy(font);if(length)*length=b.length;return b.data;
fail:free(svg);free(base64);mulib_xml_destroy(font);mulib_strbuf_free(&b);if(e&&!e->message[0])(void)mulib_paleography_error_(e,0,"CSS font allocation/argument failure");return NULL;
}
/* TeX dimensions do not accept scientific notation. Coordinates are bounded
 * by path validation; fixed decimal rounding is finer than TeX's own units. */
static int mulib_paleography_dimension_(char out[128],double value) {
    int n;char *at;const char *decimal;
    if(!isfinite(value)||fabs(value)>1000000)return 0;
    n=snprintf(out,128,"%.17f",value);if(n<0||n>=128)return 0;
    decimal=localeconv()->decimal_point;at=decimal&&*decimal?strstr(out,decimal):NULL;
    if(at&&strcmp(decimal,".")){size_t length=strlen(decimal);*at='.';memmove(at+1,at+length,strlen(at+length)+1);}
    at=strchr(out,'.');
    if(at){size_t end=strlen(out);while(end&&(out[end-1]=='0'))out[--end]=0;if(end&&out[end-1]=='.')out[--end]=0;}
    if(!strcmp(out,"-0")){out[0]='0';out[1]=0;}return 1;
}
static int mulib_paleography_point_(mulib_strbuf_t *b,double x,double y,double units){char a[128],v[128];return mulib_paleography_dimension_(a,x/units)&&mulib_paleography_dimension_(v,y/units)&&mulib_strbuf_append(b,"{\\pgfpoint{")&&mulib_strbuf_append(b,a)&&mulib_strbuf_append(b,"em}{")&&mulib_strbuf_append(b,v)&&mulib_strbuf_append(b,"em}}");}
char *mulib_paleography_package(const mulib_paleography_t *c,size_t *length,mulib_paleography_error_t *e){
    mulib_strbuf_t b={0};size_t i;(void)mulib_paleography_error_(e,0,NULL);if(length)*length=0;if(!c)goto fail;
    if(!mulib_strbuf_append(&b,"\\NeedsTeXFormat{LaTeX2e}\n\\ProvidesPackage{")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"}[MuLIB vector and native script font]\n\\RequirePackage{pgf}\n\\RequirePackage{iftex}\n"))goto fail;
    if(*c->font){if(!mulib_strbuf_append(&b,"\\ifPDFTeX\\else\n\\RequirePackage{fontspec}\n\\expandafter\\newfontfamily\\csname ")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"font\\endcsname{")||!mulib_strbuf_append(&b,c->font)||!mulib_strbuf_append(&b,"}\n\\fi\n\\expandafter\\def\\csname ")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"text\\endcsname#1{\\ifPDFTeX\\PackageError{")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"}{Native Unicode font requires LuaLaTeX or XeLaTeX}{}\\else{\\csname ")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"font\\endcsname #1}\\fi}\n"))goto fail;}
    if(!mulib_strbuf_append(&b,"\\expandafter\\def\\csname ")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"glyph\\endcsname#1{\\csname ")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"@#1\\endcsname}\n"))goto fail;
    for(i=0;i<c->count;++i){const mulib_paleography_glyph_t *g=c->glyphs[i];char hex[16];
        if(!mulib_strbuf_append(&b,"\\expandafter\\def\\csname ")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_putc(&b,'@')||!mulib_strbuf_append(&b,g->name)||!mulib_strbuf_append(&b,"\\endcsname{"))goto fail;
        if(*g->path){mulib_paleography_path_ p;char cmd;double v[6],px=0,py=0;int next;memset(&p,0,sizeof p);p.s=g->path;p.length=strlen(p.s);
            if(!mulib_strbuf_append(&b,"\\begin{pgfpicture}\\pgfsetbaseline{0pt}\\pgfpathrectangle")||!mulib_paleography_point_(&b,0,-c->options.descent,c->options.units_per_em)||!mulib_paleography_point_(&b,g->advance,c->options.ascent+c->options.descent,c->options.units_per_em)||!mulib_strbuf_append(&b,"\\pgfusepath{use as bounding box}"))goto fail;
            while((next=mulib_paleography_path_next_(&p,&cmd,v))==1){
                if(cmd=='Z'){if(!mulib_strbuf_append(&b,"\\pgfpathclose"))goto fail;continue;}
                if(!mulib_strbuf_append(&b,cmd=='M'?"\\pgfpathmoveto":cmd=='L'?"\\pgfpathlineto":"\\pgfpathcurveto"))goto fail;
                if(cmd=='Q'){if(!mulib_paleography_point_(&b,px+2*(v[0]-px)/3,py+2*(v[1]-py)/3,c->options.units_per_em)||!mulib_paleography_point_(&b,v[2]+2*(v[0]-v[2])/3,v[3]+2*(v[1]-v[3])/3,c->options.units_per_em)||!mulib_paleography_point_(&b,v[2],v[3],c->options.units_per_em))goto fail;px=v[2];py=v[3];}
                else{size_t j,n=cmd=='C'?6:2;for(j=0;j<n;j+=2)if(!mulib_paleography_point_(&b,v[j],v[j+1],c->options.units_per_em))goto fail;px=v[n-2];py=v[n-1];}
            }
            if(next<0||!mulib_strbuf_append(&b,"\\pgfusepath{fill}\\end{pgfpicture}"))goto fail;
        }else{unsigned char bytes[5];size_t n=mulib_utf8_encode(g->scalar,bytes,4);bytes[n]=0;if(!*c->font){if(!mulib_strbuf_append(&b,"\\PackageError{")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"}{Glyph has no outline or native font}{}"))goto fail;}else if(!mulib_strbuf_putc(&b,'\\')||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"text{")||!mulib_tex_escape(&b,(const char *)bytes,n)||!mulib_strbuf_putc(&b,'}'))goto fail;}
        if(!mulib_strbuf_append(&b,"}\n"))goto fail;
        (void)snprintf(hex,sizeof hex,"%X",g->scalar);
        if(!mulib_strbuf_append(&b,"\\ifdefined\\DeclareUnicodeCharacter\\DeclareUnicodeCharacter{")||!mulib_strbuf_append(&b,hex)||!mulib_strbuf_append(&b,"}{\\")||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"glyph{")||!mulib_strbuf_append(&b,g->name)||!mulib_strbuf_append(&b,"}}\\fi\n"))goto fail;
    }
    if(length)*length=b.length;
    return b.data;
fail:mulib_strbuf_free(&b);(void)mulib_paleography_error_(e,0,"LaTeX package allocation/argument failure");return NULL;
}
char *mulib_paleography_encoding(const mulib_paleography_t *c,size_t *length,mulib_paleography_error_t *e){mulib_strbuf_t b={0};size_t i;(void)mulib_paleography_error_(e,0,NULL);if(length)*length=0;if(!c||c->count>255)goto fail;if(!mulib_strbuf_putc(&b,'/')||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"Encoding [\n/.notdef\n"))goto fail;for(i=0;i<255;++i)if(!mulib_strbuf_putc(&b,'/')||!mulib_strbuf_append(&b,i<c->count?c->glyphs[i]->name:".notdef")||!mulib_strbuf_putc(&b,'\n'))goto fail;if(!mulib_strbuf_append(&b,"] def\n"))goto fail;if(length)*length=b.length;return b.data;
fail:mulib_strbuf_free(&b);(void)mulib_paleography_error_(e,0,"encoding requires <=255 glyphs, valid context and storage");return NULL;}
char *mulib_paleography_latex(mulib_paleography_t *c,const char *text,size_t length,const char *script,size_t *out_length,mulib_paleography_error_t *e){
    mulib_mune_result_t *r=NULL;mulib_mune_value_t list,v;const char *direction,*token;size_t count,i;mulib_strbuf_t b={0};(void)mulib_paleography_error_(e,0,NULL);if(out_length)*out_length=0;if(!c||c->busy){(void)mulib_paleography_error_(e,0,"invalid/reentrant paleography context");return NULL;}c->busy=1;
    r=mulib_paleography_shape_(c,text,length,script?script:"identity",&list,&direction,&count,e);if(!r||!mulib_strbuf_append(&b,""))goto fail;(void)direction;
    for(i=1;i<=count;++i){const mulib_paleography_glyph_t *g;uint32_t scalar;if(mulib_mune_raw_get(c->state,list,mulib_mune_integer((int64_t)i),&v)||(token=mulib_munexml_string_(v))==NULL)goto fail;
        if(!strcmp(token,"\n")){if(!mulib_strbuf_append(&b,"\\par\n"))goto fail;continue;}if(!strcmp(token,"\r"))continue;if(!strcmp(token," ")){if(!mulib_strbuf_putc(&b,' '))goto fail;continue;}
        g=mulib_paleography_lookup_(c,token,&scalar);if(g){if((!*g->path&&!*c->font)||!mulib_strbuf_putc(&b,'\\')||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"glyph{")||!mulib_strbuf_append(&b,g->name)||!mulib_strbuf_putc(&b,'}'))goto fail;}
        else{if(!scalar||!*c->font)goto fail;if(!mulib_strbuf_putc(&b,'\\')||!mulib_strbuf_append(&b,c->id)||!mulib_strbuf_append(&b,"text{")||!mulib_tex_escape(&b,token,strlen(token))||!mulib_strbuf_putc(&b,'}'))goto fail;}
    }
    mulib_mune_result_destroy(r);c->busy=0;if(out_length)*out_length=b.length;return b.data;
fail:mulib_mune_result_destroy(r);mulib_strbuf_free(&b);c->busy=0;if(e&&!e->message[0])(void)mulib_paleography_error_(e,0,"unknown glyph/native font or LaTeX allocation failure");return NULL;
}
#ifdef __cplusplus
}
#endif
#endif
