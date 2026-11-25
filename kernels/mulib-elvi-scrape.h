/** @file mulib-elvi-scrape.h
 * @brief Compile Elvi profiles and extract bounded legacy search results.
 * ISO C11; no third-party libraries. MuLIB dependencies: mulib-fetch,
 * mulib-url, mulib-https, mulib-json, and detail/mulib-webscan.
 * Define MULIB_IMPL_ELVI_SCRAPE in one TU.
 *
 * The HTTPS adapter composes MuLIB HTTPS/TSL with application-owned TLS
 * connection and authenticated peer inspection callbacks.
 * See manual/mulib-elvi-scrape.md and manual/elvi-script-grammar.md.
 */
#ifndef MULIB_ELVI_SCRAPE_H
#define MULIB_ELVI_SCRAPE_H
#include "mulib-fetch.h"
#include "mulib-url.h"
#include "mulib-https.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_elvi_anchor_mode {
  MULIB_ELVI_ANCHOR_CHILD = 1,  /* <a><h3>title</h3></a> */
  MULIB_ELVI_ANCHOR_PARENT = 2, /* <h2><a>title</a></h2> */
  MULIB_ELVI_ANCHOR_ANY = 3
} mulib_elvi_anchor_mode_t;
typedef enum mulib_elvi_rewrite {
  MULIB_ELVI_REWRITE_DIRECT = 0,
  MULIB_ELVI_REWRITE_GOOGLE = 1,
  MULIB_ELVI_REWRITE_RELATIVE = 2
} mulib_elvi_rewrite_t;
typedef enum mulib_elvi_format {
  MULIB_ELVI_FORMAT_HTML = 0,
  MULIB_ELVI_FORMAT_JSON = 1
} mulib_elvi_format_t;
typedef enum mulib_elvi_mode {
  MULIB_ELVI_MODE_SEARCH = 0,
  MULIB_ELVI_MODE_SCRAPE
} mulib_elvi_mode_t;
typedef enum mulib_elvi_stage_kind {
  MULIB_ELVI_STAGE_NONE = 0,
  MULIB_ELVI_STAGE_COOKIE_FILE,
  MULIB_ELVI_STAGE_XPATH,
  MULIB_ELVI_STAGE_DISCARD,
  MULIB_ELVI_STAGE_LUA,
  MULIB_ELVI_STAGE_IPC
} mulib_elvi_stage_kind_t;
typedef struct mulib_elvi_stage {
  mulib_elvi_stage_kind_t kind;
  char argument[1024];            /* decoded text, with HOME still unexpanded */
  size_t line;                    /* one-based opening directive line; 0 if absent */
} mulib_elvi_stage_t;
typedef struct mulib_elvi_scrape_program {
  char name[64];
  char host[256];                 /* exact HTTPS request host */
  char home[1024];
  char search_template[4096];
  char anchor_tag[32];           /* child/parent context tag */
  char excluded_hosts[8][256];   /* exact host or subdomain suffix */
  size_t excluded_count;
  unsigned max_results;          /* 1..64 */
  mulib_elvi_anchor_mode_t anchor_mode;
  mulib_elvi_rewrite_t rewrite;
  mulib_elvi_format_t format;
  char anchor_class[128];        /* optional case-sensitive HTML class token */
  char json_results[256], json_title[256], json_url[256]; /* selector paths */
  mulib_elvi_mode_t mode;
  mulib_elvi_stage_t cookies, xpath, artifacts, logic, forward;
} mulib_elvi_scrape_program_t;
typedef struct mulib_elvi_scrape_result {
  char title[512];
  char url[2048];
} mulib_elvi_scrape_result_t;
typedef struct mulib_elvi_scrape_error {
  size_t line;
  char message[160];
} mulib_elvi_scrape_error_t;

/* Parse a NUL-free buffer (<=64 KiB) without I/O; *out is unchanged on failure. */
int mulib_elvi_scrape_compile(const char *src, size_t len,
                             mulib_elvi_scrape_program_t *out,
                             mulib_elvi_scrape_error_t *error);
/* Load a bounded file (<=64 KiB) with ISO C stdio, then compile. */
int mulib_elvi_scrape_load(const char *path,
                          mulib_elvi_scrape_program_t *out,
                          mulib_elvi_scrape_error_t *error);
/* Resolve a nonempty stage argument as text. Leading $HOME or ${HOME}, as a
 * whole path component, uses only the supplied absolute home. A literal
 * argument needs no home. Strings must not overlap out; out is unchanged
 * on failure. This neither canonicalizes paths nor authorizes I/O. */
int mulib_elvi_script_resolve(const char *argument, const char *home,
                             char *out, size_t capacity);
/* Both modes: public HTTPS, exact profile host, no fragment; no I/O. */
int mulib_elvi_script_url_allowed(const mulib_elvi_scrape_program_t *p,
                                 const char *url);
/* Percent-encode a nonempty query with mulib-url; "" selects the provider
 * home URL. In both cases limit is 1..max_results. Output is unchanged on
 * failure. This and the legacy parse/search APIs require search mode. */
int mulib_elvi_scrape_url(const mulib_elvi_scrape_program_t *p,
                         const char *query, unsigned limit,
                         char *out, size_t capacity);
/* Parse HTML or JSON (<=2 MiB) without fetching, with bounded work. Returns
 * success even for zero hits. Results/count remain unchanged on failure. */
int mulib_elvi_scrape_parse(const mulib_elvi_scrape_program_t *p,
                            const char *body, size_t len,
                           mulib_elvi_scrape_result_t *results, size_t cap,
                           size_t *count);
/* Build URL, fetch via supplied MuLIB fetch client, parse if status=200 and
 * media type matches the profile. Transport MUST use verified TLS for HTTPS. */
int mulib_elvi_scrape_search(const mulib_elvi_scrape_program_t *p,
                            const mulib_fetch_client_t *client,
                            const char *query, unsigned limit,
                            mulib_elvi_scrape_result_t *results, size_t cap,
                            size_t *count, unsigned *http_status);
/* Inspect the negotiated session AFTER its verified handshake. The application
 * must report the authenticated peer, never configuration/advertised versions.
 * Both callbacks/context are borrowed for the synchronous call. */
typedef struct mulib_elvi_scrape_peer {
  char hostname[256];
  unsigned tls_version; /* 0x0303 (TLS 1.2) or 0x0304 (TLS 1.3) */
  int certificate_verified, hostname_verified;
  char alpn[16]; /* "http/1.1"; empty means no ALPN, HTTP/1.1 default */
} mulib_elvi_scrape_peer_t;
typedef struct mulib_elvi_scrape_https {
  mulib_https_connect_fn connect;
  int (*peer)(void *, const mulib_tsl_t *, mulib_elvi_scrape_peer_t *);
  void *context;
} mulib_elvi_scrape_https_t;
/* A mulib_https_connect_fn. ctx points to a borrowed mulib_elvi_scrape_https_t.
 * Use with a validated endpoint and mulib_https_request, which owns cleanup
 * of the session even if peer validation fails here. This is not a URL gate. */
int mulib_elvi_scrape_https_connect_verified(void *ctx, const mulib_uri_t *endpoint,
                                             mulib_tsl_t *session);
/* A mulib_fetch_transport_fn. ctx points to mulib_elvi_scrape_https_t.
 * Enforces public HTTPS, verified peer identity, TLS >=1.2 and HTTP/1.1.
 * Never follows redirects or retries rate limits. Body must be initialized;
 * outputs remain unchanged on failure. Callbacks must not mutate the adapter. */
int mulib_elvi_scrape_https_transport(void *ctx, const char *url,
    mulib_bounded_text_buffer_t *body, unsigned *status, char *type, size_t cap);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ELVI_SCRAPE) && !defined(MULIB_ELVI_SCRAPE_IMPLEMENTED)
#define MULIB_ELVI_SCRAPE_IMPLEMENTED
#include "detail/mulib-webscan.h"
#include "mulib-json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

static int melvi_copy_(char *dst, size_t cap, const char *src) {
  size_t n = strlen(src);
  if (n >= cap) return 0;
  memcpy(dst, src, n+1); return 1;
}
static void melvi_error_(mulib_elvi_scrape_error_t *e, size_t line,
                          const char *message) {
  if (!e) return;
  e->line = line;
  (void)melvi_copy_(e->message, sizeof(e->message), message);
}
static int melvi_eq_(const char *a, const char *b) { return strcmp(a,b)==0; }
static void melvi_lower_(char *s) {
  for(;*s;s++) *s=(char)mulib_webscan_lower_((unsigned char)*s);
}
static int melvi_hostname_(const char *s) {
  size_t n=strlen(s), label=0;
  if(!n || n>253) return 0;
  for(size_t i=0;i<n;i++) {
    unsigned char c=(unsigned char)s[i];
    if(c=='.') {if(!label || s[i-1]=='-')return 0;label=0;}
    else {
      if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
           (c>='0'&&c<='9')||c=='-') || (!label&&c=='-') || ++label>63)return 0;
    }
  }
  return label && s[n-1]!='-';
}
static int melvi_url_bytes_(const char *url) {
  unsigned char decoded[8192];size_t n=0,len=0;
  while(len<sizeof decoded && url[len])len++;
  if(len==sizeof decoded)return 0;
  /* Validate every escape, including direct URLs; reject decoded controls and
   * backslashes without interpreting escaped delimiters as URL structure. */
  if(!mulib_url_decode(url,len,decoded,sizeof decoded,&n))return 0;
  for(size_t i=0;i<n;i++)if(decoded[i]<32 || decoded[i]==127 || decoded[i]=='\\')return 0;
  for(size_t i=0;i<len;i++)if((unsigned char)url[i]<=32 || url[i]=='\\')return 0;
  return 1;
}
static int melvi_host_(const char *url, char *host, size_t cap) {
  const char *p, *end;size_t n;
  mulib_fetch_policy_t policy={8192,2097152,0,1,0};
  if(!melvi_url_bytes_(url) || strncmp(url,"https://",8))return 0;
  if(!mulib_fetch_policy_check(&policy,url))return 0;
  p=url+8;end=p;
  while(*end && *end!='/' && *end!='?' && *end!='#')end++;
  n=(size_t)(end-p);
  if(!n || n>=cap)return 0;
  for(size_t i=0;i<n;i++) {
    unsigned char c=(unsigned char)p[i];
    host[i]=(char)((c>='A'&&c<='Z')?c+32:c);
  }
  host[n]=0;return melvi_hostname_(host);
}
static int melvi_class_token_(const char *s) {
  if(!*s)return 0;
  for(;*s;s++)if((unsigned char)*s<=32 || (unsigned char)*s==127)return 0;
  return 1;
}
static int melvi_pointer_syntax_(const char *s,size_t cap) {
  if(!memchr(s,0,cap) || *s!='/')return 0;
  for(;*s;s++)if(*s=='~' && (*++s!='0' && *s!='1'))return 0;
  return 1;
}
/* Public structs are caller-owned: validate extents before string operations. */
static int melvi_program_(const mulib_elvi_scrape_program_t *p) {
  if(!p || p->mode!=MULIB_ELVI_MODE_SEARCH ||
     p->excluded_count>8 || p->max_results<1 || p->max_results>64 ||
     (p->rewrite!=MULIB_ELVI_REWRITE_DIRECT && p->rewrite!=MULIB_ELVI_REWRITE_GOOGLE &&
      p->rewrite!=MULIB_ELVI_REWRITE_RELATIVE) ||
     !memchr(p->name,0,sizeof p->name) || !memchr(p->host,0,sizeof p->host) ||
     !memchr(p->home,0,sizeof p->home) ||
     !memchr(p->search_template,0,sizeof p->search_template))return 0;
  if(p->format==MULIB_ELVI_FORMAT_HTML) {
    if(p->anchor_mode<MULIB_ELVI_ANCHOR_CHILD || p->anchor_mode>MULIB_ELVI_ANCHOR_ANY ||
       !memchr(p->anchor_tag,0,sizeof p->anchor_tag) || !p->anchor_tag[0] ||
       !memchr(p->anchor_class,0,sizeof p->anchor_class) ||
       (p->anchor_class[0] && !melvi_class_token_(p->anchor_class)))return 0;
  } else if(p->format==MULIB_ELVI_FORMAT_JSON) {
    if(p->rewrite==MULIB_ELVI_REWRITE_GOOGLE ||
       !melvi_pointer_syntax_(p->json_results,sizeof p->json_results) ||
       !melvi_pointer_syntax_(p->json_title,sizeof p->json_title) ||
       !melvi_pointer_syntax_(p->json_url,sizeof p->json_url))return 0;
  } else return 0;
  for(size_t i=0;i<p->excluded_count;i++)
    if(!memchr(p->excluded_hosts[i],0,256) || !melvi_hostname_(p->excluded_hosts[i]))return 0;
  return melvi_hostname_(p->host);
}
static int melvi_same_host_(const char *url, const char *host) {
  char parsed[256]; return melvi_host_(url,parsed,sizeof parsed) && !strcmp(parsed,host);
}
int mulib_elvi_script_url_allowed(const mulib_elvi_scrape_program_t *p,
                                 const char *url) {
  return p && url && memchr(p->host,0,sizeof p->host) &&
    melvi_same_host_(url,p->host) && !strchr(url,'#');
}
/* Match the literal template authority, independently of any expansion. */
static int melvi_template_host_(const mulib_elvi_scrape_program_t *p) {
  const char *s=p->search_template;
  if(strncmp(s,"https://",8)) return 0;
  s+=8;
  return mulib_webscan_equal_(s,strcspn(s,"/?#"),p->host);
}
static int melvi_excluded_(const mulib_elvi_scrape_program_t *p, const char *url) {
  char h[256];
  if (!melvi_host_(url,h,sizeof h)) return 1;
  for(size_t i=0;i<p->excluded_count;i++) {
    const char *block=p->excluded_hosts[i];
    size_t n=strlen(h), m=strlen(block);
    if ((n==m && !strcmp(h,block)) ||
        (n>m && h[n-m-1]=='.' && !strcmp(h+n-m,block))) return 1;
  }
  return 0;
}
/* Line tokens: unquoted no-whitespace strings, or double-quoted strings
 * with escapes for backslash and double-quote. '#' starts a comment. */
static int melvi_token_(const char **cursor, const char *end,
                         char *out, size_t cap) {
  const char *p=*cursor; size_t n=0; int quoted=0, closed=0;
  while (p<end && (*p==' '||*p=='\t'||*p=='\r')) p++;
  if (p==end || *p=='#') { *cursor=end; return 0; }
  if (*p=='"') {quoted=1; p++;}
  while(p<end) {
    char c=*p;
    if(quoted) {
      if(c=='"') { p++;closed=1;break; }
      if(c=='\\') { p++;if(p==end || (*p!='\\'&&*p!='"')) return -1;c=*p; }
    } else if(c==' '||c=='\t'||c=='\r'||c=='#') break;
    if ((unsigned char)c<32 || n+1>=cap) return -1;
    out[n++]=c; p++;
  }
  if(quoted && !closed) return -1;
  if(quoted && p<end && *p!=' ' && *p!='\t' && *p!='\r' && *p!='#') return -1;
  out[n]=0;
  *cursor=p;
  return n?1:-1;
}
/* Provider expressions have their own grammar; legacy tokens (notably search
 * placeholders and JSON paths) are not interpreted as provider expressions. */
static int melvi_hspace_(char c) { return c==' ' || c=='\t' || c=='\r'; }
static int melvi_provider_(const char **cursor,const char *line_end,const char *end,
                            const char *provider,char *out,size_t cap,size_t *line) {
  const char *p=*cursor;size_t prefix=strlen(provider),n=0;
  unsigned depth=1;char quote=0;int space=0;
  while(p<line_end && melvi_hspace_(*p))p++;
  if((size_t)(line_end-p)<=prefix || memcmp(p,provider,prefix) || p[prefix]!='{')return 0;
  p+=prefix+1;
  while(p<end) {
    char c=*p++;
    if(!quote && (melvi_hspace_(c) || c=='\n' || c=='#')) {
      if(c=='#')while(p<end && *p!='\n')p++;
      if(c=='\n')(*line)++;
      if(n)space=1;
      continue;
    }
    if(c=='\\') {
      if(p==end || !strchr("\\\"'{}# ",*p))return 0;
      c=*p++;
    } else if(quote) {
      if(c==quote) {quote=0;continue;}
    } else if(c=='"' || c=='\'') {
      quote=c;continue;
    } else if(c=='{') {
      if(++depth>32)return 0;
    } else if(c=='}' && --depth==0) {
      while(p<end && melvi_hspace_(*p))p++;
      if(p<end && *p=='#')while(p<end && *p!='\n')p++;
      if(!n || (p<end && *p!='\n'))return 0;
      out[n]=0;*cursor=p;return 1;
    }
    if((unsigned char)c<32 || (unsigned char)c==127)return 0;
    if(space) {
      if(n+1>=cap)return 0;
      out[n++]=' ';space=0;
    }
    if(n+1>=cap)return 0;
    out[n++]=c;
  }
  return 0;
}
static int melvi_argument_(const char *argument,size_t *prefix) {
  const char *p=argument;
  if(!p || !*p)return 0;
  *prefix=0;
  if(*p=='$') {
    if(!strncmp(p,"$HOME",5)) *prefix=5;
    else if(!strncmp(p,"${HOME}",7)) *prefix=7;
    else return 0;
    p+=*prefix;
    if(*p && *p!='/')return 0;
  }
  for(;*p;p++)
    if((unsigned char)*p<32 || (unsigned char)*p==127 || *p=='$')return 0;
  return 1;
}
int mulib_elvi_script_resolve(const char *argument, const char *home,
                             char *out, size_t capacity) {
  size_t prefix,n,base=0;
  if(!out || !capacity || !melvi_argument_(argument,&prefix))return 0;
  if(prefix) {
    if(!home || home[0]!='/')return 0;
    while(home[base]) {
      unsigned char c=(unsigned char)home[base];
      if(c<32 || c==127 || base==capacity-1)return 0;
      base++;
    }
  }
  argument+=prefix;n=strlen(argument);
  /* Elide the boundary separator if the supplied base already ends in one. */
  if(base && home[base-1]=='/' && *argument=='/') {argument++;n--;}
  if(n>=capacity-base)return 0;
  if(base)memcpy(out,home,base);
  memcpy(out+base,argument,n+1);return 1;
}
static int melvi_uint_(const char *s, unsigned *out) {
  unsigned x=0;
  if(!*s) return 0;
  for(;*s;s++) { if(*s<'0'||*s>'9'||x>10000) return 0;
    x=x*10u+(unsigned)(*s-'0'); }
  *out=x;return 1;
}
int mulib_elvi_scrape_compile(const char *src, size_t len,
                              mulib_elvi_scrape_program_t *out,
                              mulib_elvi_scrape_error_t *error) {
  enum { M_NAME, M_HOST, M_HOME, M_SEARCH, M_ANCHOR, M_REWRITE, M_EXCLUDE,
         M_MAX_RESULTS, M_FORMAT, M_CLASS, M_RESULTS, M_TITLE, M_URL,
         M_MODE, M_COOKIES, M_XPATH, M_ARTIFACTS, M_LOGIC, M_FORWARD, M_DIRECTIVES };
  static const char *const keys[M_DIRECTIVES]={
    "elvi","host","home","search","anchor","rewrite","exclude_host","max_results",
    "format","anchor_class","results","title","url","mode","cookies","xpath",
    "artifacts","logic","forward"
  };
  mulib_elvi_scrape_program_t v;
  size_t at=0,line=0,lines[M_DIRECTIVES]={0};
  if(!out||(!src&&len)||len>65536 || (len && memchr(src,0,len))) {
    melvi_error_(error,0,"invalid DSL input"); return 0;
  }
  memset(&v,0,sizeof v);v.max_results=20;
  while(at<len) {
    size_t start=at;const char *cur,*end;mulib_elvi_stage_t *stage=NULL;
    mulib_elvi_stage_kind_t stage_kind=MULIB_ELVI_STAGE_NONE;
    const char *provider=NULL,*stage_error=NULL;
    char k[64],a[4096],b[256];int nk,na,nb,extra,kind;
    while(at<len && src[at]!='\n') at++;
    end=src+at;cur=src+start;line++;
    if(at<len) at++;
    nk=melvi_token_(&cur,end,k,sizeof k);
    if(nk==0) continue;
    if(nk<0) goto syntax;
    for(kind=0;kind<M_DIRECTIVES && !melvi_eq_(k,keys[kind]);kind++) {}
    if(kind==M_DIRECTIVES) goto syntax;
    if(kind!=M_EXCLUDE && lines[kind]) {
      melvi_error_(error,line,"duplicate directive");return 0;
    }
    lines[kind]=line;
    switch(kind) {
    case M_COOKIES:
      stage=&v.cookies;stage_kind=MULIB_ELVI_STAGE_COOKIE_FILE;provider="provide";
      stage_error="expected cookies provide{PATH}";break;
    case M_XPATH:
      stage=&v.xpath;stage_kind=MULIB_ELVI_STAGE_XPATH;provider="provide";
      stage_error="expected xpath provide{mulib-xpath}";break;
    case M_LOGIC:
      stage=&v.logic;stage_kind=MULIB_ELVI_STAGE_LUA;provider="lua";
      stage_error="expected logic lua{PATH}";break;
    case M_FORWARD:
      stage=&v.forward;stage_kind=MULIB_ELVI_STAGE_IPC;provider="ipc";
      stage_error="expected forward ipc{PATH}";break;
    default: break;
    }
    if(stage) {
      size_t prefix;
      if(!melvi_provider_(&cur,end,src+len,provider,stage->argument,sizeof stage->argument,&line)) {
        melvi_error_(error,line,stage_error);return 0;
      }
      if(kind==M_XPATH) {
        if(!melvi_eq_(stage->argument,"mulib-xpath")) {
          melvi_error_(error,lines[kind],stage_error);return 0;
        }
      } else if(!melvi_argument_(stage->argument,&prefix)) {
        melvi_error_(error,lines[kind],"invalid path or unsupported variable; only leading HOME is allowed");
        return 0;
      }
      stage->kind=stage_kind;stage->line=lines[kind];
      at=(size_t)(cur-src);
      if(at<len)at++;
      continue;
    }
    na=melvi_token_(&cur,end,a,sizeof a);
    nb=melvi_token_(&cur,end,b,sizeof b);
    {char unused[2];extra=melvi_token_(&cur,end,unused,sizeof unused);}
    if(na!=1||nb<0||extra!=0||(kind!=M_ANCHOR && nb!=0))goto syntax;
    switch(kind) {
    case M_NAME:
      if(!melvi_copy_(v.name,sizeof v.name,a)) goto syntax;
      break;
    case M_HOST:
      if(!melvi_copy_(v.host,sizeof v.host,a)) goto syntax;
      melvi_lower_(v.host);
      if(!melvi_hostname_(v.host)) goto syntax;
      break;
    case M_HOME:
      if(!melvi_copy_(v.home,sizeof v.home,a)) goto syntax;
      break;
    case M_MODE:
      if(melvi_eq_(a,"search"))v.mode=MULIB_ELVI_MODE_SEARCH;
      else if(melvi_eq_(a,"scrape"))v.mode=MULIB_ELVI_MODE_SCRAPE;
      else {melvi_error_(error,line,"expected mode search or mode scrape");return 0;}
      break;
    case M_ARTIFACTS:
      if(!melvi_eq_(a,"discard")) {
        melvi_error_(error,line,"expected artifacts discard");return 0;
      }
      v.artifacts.kind=MULIB_ELVI_STAGE_DISCARD;v.artifacts.line=line;break;
    case M_SEARCH:
      if(!melvi_copy_(v.search_template,sizeof v.search_template,a)) goto syntax;
      break;
    case M_ANCHOR:
      v.anchor_mode=melvi_eq_(a,"child")?MULIB_ELVI_ANCHOR_CHILD:
                    melvi_eq_(a,"parent")?MULIB_ELVI_ANCHOR_PARENT:
                    melvi_eq_(a,"any")?MULIB_ELVI_ANCHOR_ANY:(mulib_elvi_anchor_mode_t)0;
      if(!v.anchor_mode || (!nb && v.anchor_mode!=MULIB_ELVI_ANCHOR_ANY) ||
         !melvi_copy_(v.anchor_tag,sizeof v.anchor_tag,nb?b:"a")) goto syntax;
      melvi_lower_(v.anchor_tag);
      for(char *q=v.anchor_tag;*q;q++) {
        if(!((*q>='a'&&*q<='z') ||
             (q!=v.anchor_tag && ((*q>='0'&&*q<='9') || *q=='-' || *q==':'))))
          goto syntax;
      }
      break;
    case M_REWRITE:
      if(melvi_eq_(a,"direct")) v.rewrite=MULIB_ELVI_REWRITE_DIRECT;
      else if(melvi_eq_(a,"google")) v.rewrite=MULIB_ELVI_REWRITE_GOOGLE;
      else if(melvi_eq_(a,"relative")) v.rewrite=MULIB_ELVI_REWRITE_RELATIVE;
      else goto syntax;
      break;
    case M_EXCLUDE:
      if(v.excluded_count==8 || !melvi_copy_(v.excluded_hosts[v.excluded_count],256,a)) goto syntax;
      melvi_lower_(v.excluded_hosts[v.excluded_count]);
      if(!melvi_hostname_(v.excluded_hosts[v.excluded_count])) goto syntax;
      v.excluded_count++;
      break;
    case M_MAX_RESULTS:
      if(!melvi_uint_(a,&v.max_results) || v.max_results<1||v.max_results>64) goto syntax;
      break;
    case M_FORMAT:
      if(melvi_eq_(a,"html"))v.format=MULIB_ELVI_FORMAT_HTML;
      else if(melvi_eq_(a,"json"))v.format=MULIB_ELVI_FORMAT_JSON;
      else goto syntax;
      break;
    case M_CLASS:
      if(!melvi_class_token_(a) || !melvi_copy_(v.anchor_class,sizeof v.anchor_class,a))goto syntax;
      break;
    case M_RESULTS: case M_TITLE: case M_URL: {
      char *path=kind==M_RESULTS?v.json_results:kind==M_TITLE?v.json_title:v.json_url;
      if(!melvi_copy_(path,sizeof v.json_results,a) ||
         !melvi_pointer_syntax_(path,sizeof v.json_results))goto syntax;
      break;
    }
    default:
      goto syntax;
    }
    continue;
  syntax: melvi_error_(error,line,"unknown directive or invalid arguments");return 0;
  }
  if(!lines[M_NAME] || !lines[M_HOST] || !lines[M_HOME]) {
    melvi_error_(error,line,"required elvi, host or home directive missing");return 0;
  }
  if(v.mode==MULIB_ELVI_MODE_SCRAPE) {
    for(int kind=M_SEARCH;kind<=M_URL;kind++)if(lines[kind]) {
      melvi_error_(error,lines[kind],"search directive not supported in scrape mode");return 0;
    }
    if(!lines[M_XPATH] || !lines[M_ARTIFACTS] || !lines[M_LOGIC] || !lines[M_FORWARD]) {
      melvi_error_(error,line,"scrape mode requires xpath, artifacts, logic and forward");return 0;
    }
  } else {
    for(int kind=M_COOKIES;kind<=M_FORWARD;kind++)if(lines[kind]) {
      melvi_error_(error,lines[kind],"stage directive requires mode scrape");return 0;
    }
    if(!lines[M_SEARCH] ||
       (v.format==MULIB_ELVI_FORMAT_HTML && !lines[M_ANCHOR]) ||
       (v.format==MULIB_ELVI_FORMAT_JSON && (!lines[M_RESULTS] || !lines[M_TITLE] || !lines[M_URL]))) {
      melvi_error_(error,line,"required directives missing");return 0;
    }
    if(v.format==MULIB_ELVI_FORMAT_HTML) {
      for(int kind=M_RESULTS;kind<=M_URL;kind++)if(lines[kind]) {
        melvi_error_(error,lines[kind],"JSON selector requires format json");return 0;
      }
    } else if(lines[M_ANCHOR] || lines[M_CLASS] || v.rewrite==MULIB_ELVI_REWRITE_GOOGLE) {
      melvi_error_(error,lines[M_ANCHOR]?lines[M_ANCHOR]:lines[M_CLASS]?lines[M_CLASS]:lines[M_REWRITE],
                   "HTML selector or rewrite not supported for JSON");return 0;
    }
  }
  if(!mulib_elvi_script_url_allowed(&v,v.home)) {
    melvi_error_(error,lines[M_HOME],"invalid HTTPS home URL or host mismatch");return 0;
  }
  if(v.mode==MULIB_ELVI_MODE_SEARCH) {
    /* Render with a nonempty probe to validate placeholders and URL syntax. */
    char url[8192];
    if(!strstr(v.search_template,"{query}") || !mulib_elvi_scrape_url(&v,"x",1,url,sizeof url)) {
      melvi_error_(error,lines[M_SEARCH],"invalid HTTPS search template");return 0;
    }
  }
  *out=v; if(error) memset(error,0,sizeof *error);return 1;
}
int mulib_elvi_scrape_load(const char *path,
                           mulib_elvi_scrape_program_t *out,
                           mulib_elvi_scrape_error_t *error) {
  FILE *f;char *buf;size_t n;int ok;
  if(!path||!out||(f=fopen(path,"rb"))==NULL) {
    melvi_error_(error,0,"cannot open DSL file");return 0;
  }
  buf=(char*)malloc(65537);
  if(!buf) {fclose(f);melvi_error_(error,0,"out of memory");return 0;}
  n=fread(buf,1,65537,f);
  if(ferror(f)||n>65536) {free(buf);fclose(f);melvi_error_(error,0,"DSL file too large or unreadable");return 0;}
  if(fclose(f)) {free(buf);melvi_error_(error,0,"cannot close DSL file");return 0;}
  ok=mulib_elvi_scrape_compile(buf,n,out,error);
  free(buf);return ok;
}
static int melvi_append_(char *s,size_t cap,size_t *used,const char *p,size_t n) {
  if(n>=cap-*used) return 0;
  memcpy(s+*used,p,n);*used+=n;s[*used]=0;return 1;
}
int mulib_elvi_scrape_url(const mulib_elvi_scrape_program_t *p,
                          const char *query, unsigned limit, char *out, size_t cap) {
  char num[24],*enc=NULL,*tmp=NULL;
  size_t qlen,used=0; const char *fmt;int ok=0;
  if(!melvi_program_(p)||!query||!out||!cap||!limit||limit>p->max_results ||
     !melvi_template_host_(p)) return 0;
  if(cap>8192)cap=8192;
  if(!*query)
    return mulib_elvi_script_url_allowed(p,p->home) && melvi_copy_(out,cap,p->home);
  qlen=0;while(qlen<=2048 && query[qlen])qlen++;
  if(qlen>2048||qlen>(SIZE_MAX-1)/3) return 0;
  enc=(char*)malloc(qlen*3+1);tmp=(char*)malloc(cap);
  if(!enc||!tmp) goto done;
  if(!mulib_url_encode((const unsigned char*)query,qlen,enc,qlen*3+1)) goto done;
  snprintf(num,sizeof num,"%u",limit);
  fmt=p->search_template;tmp[0]=0;
  while(*fmt) {
    if(*fmt=='{') {
      const char *value=NULL;
      if(!strncmp(fmt,"{query}",7)) {value=enc;fmt+=7;}
      else if(!strncmp(fmt,"{limit}",7)) {value=num;fmt+=7;}
      else goto done;
      if(!melvi_append_(tmp,cap,&used,value,strlen(value))) goto done;
    } else if(*fmt=='%') {
      static const char hex[]="0123456789abcdefABCDEF";
      if(!fmt[1] || !strchr(hex,fmt[1]) || !fmt[2] || !strchr(hex,fmt[2]) ||
         !melvi_append_(tmp,cap,&used,fmt,3)) goto done;
      fmt+=3;
    } else if (*fmt=='}') goto done;
    else if(!melvi_append_(tmp,cap,&used,fmt++,1)) goto done;
  }
  if(!mulib_elvi_script_url_allowed(p,tmp)) goto done;
  memcpy(out,tmp,used+1);ok=1;
 done:free(enc);free(tmp);return ok;
}
/* Decode the five core entities and numeric character references. */
static size_t melvi_entity_(const char *p,size_t n,unsigned char out[4]) {
  if(n>=5&&!memcmp(p,"&amp;",5)) {out[0]='&';return 5;}
  if(n>=4&&!memcmp(p,"&lt;",4)) {out[0]='<';return 4;}
  if(n>=4&&!memcmp(p,"&gt;",4)) {out[0]='>';return 4;}
  if(n>=6&&!memcmp(p,"&quot;",6)) {out[0]='"';return 6;}
  if(n>=6&&!memcmp(p,"&apos;",6)) {out[0]='\'';return 6;}
  if(n>=6&&!memcmp(p,"&nbsp;",6)) {out[0]=' ';return 6;}
  if(n<4||p[0]!='&'||p[1]!='#')return 0;
  uint32_t v=0;size_t k=2;int radix=10;
  if(k<n&&(p[k]=='x'||p[k]=='X')) {radix=16;k++;}
  size_t digits=k;
  for(;k<n&&p[k]!=';';k++) {
    unsigned d=(unsigned char)p[k];
    if(d>='0'&&d<='9')d-='0';
    else if(radix==16&&d>='a'&&d<='f')d=d-'a'+10;
    else if(radix==16&&d>='A'&&d<='F')d=d-'A'+10;
    else return 0;
    if(d>=(unsigned)radix||v>(0x10ffffu-d)/(unsigned)radix)return 0;
    v=v*(unsigned)radix+d;
  }
  if(k==digits||k>=n||p[k]!=';'||!v||v>0x10ffffu||
     (v>=0xd800&&v<=0xdfff))return 0;
  if(v==160) v=' ';
  if(v<128) out[0]=(unsigned char)v;
  else if(v<2048) {out[0]=(unsigned char)(0xc0|(v>>6));out[1]=(unsigned char)(0x80|(v&63));}
  else if(v<65536) {out[0]=(unsigned char)(0xe0|(v>>12));out[1]=(unsigned char)(0x80|((v>>6)&63));out[2]=(unsigned char)(0x80|(v&63));}
  else {out[0]=(unsigned char)(0xf0|(v>>18));out[1]=(unsigned char)(0x80|((v>>12)&63));out[2]=(unsigned char)(0x80|((v>>6)&63));out[3]=(unsigned char)(0x80|(v&63));}
  return k+1;
}
/* Once a code point will not fit, do not resume with a later text span. */
static int melvi_text_(char *title,size_t cap,size_t *used,const char *src,size_t len) {
  size_t j=*used;int clipped=0;
  for(size_t i=0;i<len&&j+1<cap;) {
    unsigned char chars[4]={0};size_t consumed=0;
    unsigned char c=(unsigned char)src[i];
    /* The shared tag scanner skips comments/declarations, not text spans. */
    if(c=='<' && len-i>=4 && !memcmp(src+i,"<!--",4)) {
      size_t k=i+4;
      while(k+3<=len && memcmp(src+k,"-->",3)) k++;
      if(k+3<=len) {i=k+3;continue;}
      break;
    }
    if(c=='<' && i+1<len && (src[i+1]=='!' || src[i+1]=='?')) {
      char quote=0;size_t k=i+2;
      for(;k<len;k++) {
        if(quote) {if(src[k]==quote)quote=0;}
        else if(src[k]=='\'' || src[k]=='"')quote=src[k];
        else if(src[k]=='>')break;
      }
      if(k==len)break;
      i=k+1;continue;
    }
    if(c=='&') consumed=melvi_entity_(src+i,len-i,chars);
    if(consumed) {
      size_t n=chars[0]<128?1:(chars[0]<224?2:(chars[0]<240?3:4));
      if(chars[0]==' '||chars[0]=='\n'||chars[0]=='\r'||chars[0]=='\t'||chars[0]=='\f') {
        if(j&&title[j-1]!=' ')title[j++]=' ';
      } else if(chars[0]>=32 && chars[0]!=127) {
        if(n>=cap-j) {clipped=1;break;}
        memcpy(title+j,chars,n);j+=n;
      }
      i+=consumed;continue;
    }
    if(c=='\r'||c=='\n'||c=='\t'||c==' '||c=='\f') {
      if(j&&title[j-1]!=' ')title[j++]=' ';
    } else if(c>=128) {
      uint32_t point;size_t width;
      if(!mulib_utf8_decode((const unsigned char *)src+i,len-i,&point,&width)) {
        i++;continue;
      }
      if(point==160) {if(j&&title[j-1]!=' ')title[j++]=' ';i+=width;continue;}
      if(width>=cap-j) {clipped=1;break;}
      memcpy(title+j,src+i,width);j+=width;i+=width;continue;
    } else if(c>=32 && c!=127) title[j++]=(char)c;
    i++;
  }
  title[j]=0;*used=j;return clipped || j+1==cap;
}
/* HTML attribute values are entity-encoded; decode before URL analysis. */
static int melvi_attribute_(const char *raw, char *out, size_t cap) {
  size_t j=0,len=strlen(raw);
  for(size_t i=0;i<len;) {
    unsigned char utf[4]={0};size_t consumed=0,n=1;
    if(raw[i]=='&') consumed=melvi_entity_(raw+i,len-i,utf);
    if(!consumed && raw[i]=='&' && i+1<len && raw[i+1]=='#')return 0;
    if(consumed) {
      n=utf[0]<128?1:(utf[0]<224?2:(utf[0]<240?3:4));
      if(n>=cap-j)return 0;
      memcpy(out+j,utf,n);j+=n;i+=consumed;
    } else {
      if(j+1>=cap)return 0;
      out[j++]=raw[i++];
    }
  }
  out[j]=0;return 1;
}
/* An already-decoded result URL, independent of the response format. */
static int melvi_result_url_(const mulib_elvi_scrape_program_t *p,
                              const char *value,char *out,size_t cap) {
  char resolved[sizeof (((mulib_elvi_scrape_result_t *)0)->url)];
  if(p->rewrite==MULIB_ELVI_REWRITE_RELATIVE && value[0]=='/') {
    size_t used=0;
    /* Only root-relative paths, never an alternate authority or <base>. */
    if(value[1]=='/' ||
       !melvi_append_(resolved,sizeof resolved,&used,"https://",8) ||
       !melvi_append_(resolved,sizeof resolved,&used,p->host,strlen(p->host)) ||
       !melvi_append_(resolved,sizeof resolved,&used,value,strlen(value)))return 0;
    value=resolved;
  }
  /* We intentionally do not fetch result URLs; only return HTTPS links. */
  if(melvi_excluded_(p,value))return 0;
  return melvi_copy_(out,cap,value);
}
static int melvi_redirect_(const mulib_elvi_scrape_program_t *p,
                           const char *raw,char *out,size_t cap) {
  char normalized[4096],dec[4096];size_t n=0;
  const char *value;
  if(!melvi_attribute_(raw,normalized,sizeof normalized))return 0;
  value=normalized;
  if(p->rewrite==MULIB_ELVI_REWRITE_GOOGLE) {
    const char *path=NULL;
    if(!strncmp(normalized,"/url?",5)) path=normalized+5;
    else if(!strncmp(normalized,"https://www.google.com/url?",27)) path=normalized+27;
    if(path) {
      /* Recognize q= and url= regardless of their order in the query. */
      const char *stop=path+strcspn(path,"#");
      if(!melvi_url_bytes_(normalized))return 0;
      value=NULL;
      while(path<stop) {
        const char *end=(const char *)memchr(path,'&',(size_t)(stop-path));
        size_t length=end?(size_t)(end-path):(size_t)(stop-path),prefix=0;
        if(length>=2&&!memcmp(path,"q=",2))prefix=2;
        if(length>=4&&!memcmp(path,"url=",4))prefix=4;
        if(prefix) {
          if(value || length==prefix)return 0; /* ambiguous/empty target */
          value=path+prefix;n=length-prefix;
        }
        path=end?end+1:stop;
      }
      if(!value||n>=sizeof dec||
         !mulib_url_decode(value,n,(unsigned char*)dec,sizeof dec-1,&n)||
         memchr(dec,0,n))return 0;
      dec[n]=0;value=dec;
    }
  }
  return melvi_result_url_(p,value,out,cap);
}
static int melvi_tag_(const mulib_webscan_tag_ *tag,const char *name) {
  return mulib_webscan_equal_(tag->name,tag->length,name);
}
static int melvi_heading_(const mulib_webscan_tag_ *tag) {
  return tag->length==2 && mulib_webscan_lower_((unsigned char)tag->name[0])=='h' &&
    tag->name[1]>='1' && tag->name[1]<='6';
}
static unsigned char melvi_cell_(const mulib_webscan_tag_ *tag) {
  return melvi_tag_(tag,"td")?1:melvi_tag_(tag,"th")?2:0;
}
static int melvi_row_(const mulib_webscan_tag_ *tag) {
  return melvi_tag_(tag,"tr") || melvi_tag_(tag,"thead") ||
    melvi_tag_(tag,"tbody") || melvi_tag_(tag,"tfoot");
}
static const char *melvi_raw_(const mulib_webscan_tag_ *tag) {
  static const char *const names[]={"script","style","textarea","title","xmp",
    "iframe","noembed","noframes","noscript"};
  for(size_t i=0;i<sizeof names/sizeof *names;i++)if(melvi_tag_(tag,names[i]))return names[i];
  return NULL;
}
static int melvi_parse_html_(const mulib_elvi_scrape_program_t *p,
                             const char *html,size_t len,
                             mulib_elvi_scrape_result_t *results,size_t cap,
                             size_t *count) {
  char *copy;const char *scan,*prev;mulib_webscan_tag_ tag;
  mulib_elvi_scrape_result_t *pending;
  size_t used=0,work,parent=0,child=0,inert=0,title_len=0,tables=0,anchor_table=0;
  unsigned char cell_parent=0,*cells=NULL;
  int active=0,seen_child=0,parsed=0,clipped=0;
  char href[4096],title[512];
  if(!cap) {*count=0;return 1;}
  if(p->anchor_mode==MULIB_ELVI_ANCHOR_PARENT)
    cell_parent=melvi_eq_(p->anchor_tag,"td")?1:melvi_eq_(p->anchor_tag,"th")?2:0;
  pending=(mulib_elvi_scrape_result_t *)calloc(cap,sizeof *pending);
  if(!pending)return 0;
  /* One cell marker per open table, plus a base entry for fragment input.
   * A table opening tag takes at least seven bytes. */
  copy=(char*)malloc(len+1+(cell_parent?len/7+1:0));
  if(!copy){free(pending);return 0;}
  if(len) memcpy(copy,html,len);
  copy[len]=0;
  if(cell_parent) {cells=(unsigned char *)copy+len+1;cells[0]=0;}
  scan=prev=copy;title[0]=href[0]=0;
  work=len*16+1024;
  while(used<cap && (parsed=mulib_webscan_next_work_(&scan,&tag,&work))==1) {
    const char *start=tag.name-(tag.closing?2:1);
    const char *raw=melvi_raw_(&tag);
    if(active && !clipped && !inert && start>=prev &&
       (p->anchor_mode!=MULIB_ELVI_ANCHOR_CHILD || child))
      clipped=melvi_text_(title,sizeof title,&title_len,prev,(size_t)(start-prev));
    if(!tag.closing && melvi_tag_(&tag,"plaintext"))break;
    if(!tag.closing && raw) {
      /* webscan already skipped script/style; opaque/RCDATA tags must never
       * introduce anchors or headings, even with result-like literal markup. */
      if(!melvi_tag_(&tag,"script") && !melvi_tag_(&tag,"style") &&
         mulib_webscan_raw_skip_work_(scan,raw,&work,&scan)<0) {parsed=-1;break;}
      prev=scan;continue;
    }
    if(melvi_tag_(&tag,"template")) {
      if(!tag.closing)inert++;
      else if(inert)inert--;
      prev=tag.end+1;continue;
    }
    if(inert) {prev=scan;continue;}
    /* HTML headings implicitly end prior headings. Do not let malformed
     * provider context leak into later unrelated anchors or title text. */
    if(melvi_heading_(&tag) && strlen(p->anchor_tag)==2 &&
       p->anchor_tag[0]=='h' && p->anchor_tag[1]>='1' && p->anchor_tag[1]<='6') {
      if(p->anchor_mode==MULIB_ELVI_ANCHOR_PARENT) {
        parent=0;active=0;
      } else if(p->anchor_mode==MULIB_ELVI_ANCHOR_CHILD)child=0;
    }
    if(cells) {
      unsigned char cell=melvi_cell_(&tag);
      int table=melvi_tag_(&tag,"table");
      if(table && !tag.closing) cells[++tables]=0;
      else if(table || melvi_row_(&tag) ||
              (cell && (!tag.closing || cell==cells[tables]))) {
        if(cells[tables]==cell_parent)parent--;
        cells[tables]=tag.closing?0:cell;
        if(cells[tables]==cell_parent)parent++;
        if(active && tables==anchor_table)active=0;
        if(table && tables)tables--;
      }
    } else if(!tag.closing && melvi_tag_(&tag,p->anchor_tag) &&
              p->anchor_mode==MULIB_ELVI_ANCHOR_PARENT) parent++;
    if(!tag.closing && melvi_tag_(&tag,"a")) {
      active=0;seen_child=0;child=0;clipped=0;title_len=0;title[0]=href[0]=0;
      if(p->anchor_mode!=MULIB_ELVI_ANCHOR_PARENT||parent) {
        const char *v;size_t n;
        int match=mulib_webscan_attr_first_work_(&tag,"href",&v,&n,&work);
        if(match<0) {parsed=-1;break;}
        if(match && n<sizeof href && p->anchor_class[0]) {
          match=mulib_webscan_class_work_(&tag,p->anchor_class,&work);
          if(match<0) {parsed=-1;break;}
        }
        if(match && n<sizeof href) {
          memcpy(href,v,n);href[n]=0;active=1;anchor_table=tables;
        }
      }
    } else if(active && !tag.closing && p->anchor_mode==MULIB_ELVI_ANCHOR_CHILD &&
              melvi_tag_(&tag,p->anchor_tag)) {child++;seen_child=1;}
    else if(active && tag.closing && p->anchor_mode==MULIB_ELVI_ANCHOR_CHILD &&
            melvi_tag_(&tag,p->anchor_tag)) {if(child)child--;}
    else if(active && tag.closing && melvi_tag_(&tag,"a")) {
      int qualified=p->anchor_mode!=MULIB_ELVI_ANCHOR_CHILD || seen_child;
      while(title_len && title[title_len-1]==' ')title[--title_len]=0;
      if(qualified && title[0]) {
        char url[2048];
        if(melvi_redirect_(p,href,url,sizeof url)) {
          int dupe=0;
          for(size_t k=0;k<used;k++)if(!strcmp(pending[k].url,url)){dupe=1;break;}
          if(!dupe) {
            melvi_copy_(pending[used].url,sizeof(pending[used].url),url);
            melvi_copy_(pending[used].title,sizeof(pending[used].title),title);
            used++;
          }
        }
      }
      active=0;child=0;seen_child=0;
    }
    if(!cells && tag.closing && melvi_tag_(&tag,p->anchor_tag) &&
       p->anchor_mode==MULIB_ELVI_ANCHOR_PARENT && parent) parent--;
    prev=tag.end+1;
  }
  free(copy);
  if(parsed<0) {free(pending);return 0;}
  if(used)memcpy(results,pending,used*sizeof *pending);
  free(pending);*count=used;return 1;
}
/* Traverse a pointer that the compiler has already validated. Object key
 * lookups are byte-exact; array indexes are canonical unsigned decimals. */
static const mulib_json_t *melvi_json_at_(const mulib_json_t *v,const char *path) {
  while(v && *path) {
    char key[256];size_t n=0;
    path++;
    for(;*path && *path!='/';path++) {
      if(*path=='~') key[n++]=*++path=='0'?'~':'/';
      else key[n++]=*path;
    }
    if(mulib_json_typeof(v)==MULIB_JSON_OBJECT)v=mulib_json_get_n(v,key,n);
    else if(mulib_json_typeof(v)==MULIB_JSON_ARRAY) {
      size_t idx=0;
      if(!n || (n>1 && key[0]=='0'))return NULL;
      for(size_t i=0;i<n;i++) {
        if(key[i]<'0' || key[i]>'9' || idx>(SIZE_MAX-(size_t)(key[i]-'0'))/10)return NULL;
        idx=idx*10+(size_t)(key[i]-'0');
      }
      v=mulib_json_at(v,idx);
    } else return NULL;
  }
  return v;
}
/* JSON strings are already decoded text, not HTML. Normalize whitespace
 * without interpreting literal markup/entities, and clip on UTF-8 boundaries. */
static int melvi_json_title_(const char *s,size_t n,char *out,size_t cap) {
  size_t j=0;
  if(!s || memchr(s,0,n))return 0;
  for(size_t i=0;i<n && j+1<cap;) {
    uint32_t cp;size_t w;
    if(!mulib_utf8_decode((const unsigned char *)s+i,n-i,&cp,&w))return 0;
    if(cp==160 || (cp<128 && mulib_webscan_space_((unsigned char)cp))) {
      if(j && out[j-1]!=' ')out[j++]=' ';
    } else if(cp>=32 && cp!=127) {
      if(w>=cap-j)break;
      memcpy(out+j,s+i,w);j+=w;
    }
    i+=w;
  }
  while(j && out[j-1]==' ')j--;
  out[j]=0;return j!=0;
}
static int melvi_parse_json_(const mulib_elvi_scrape_program_t *p,
                             const char *body,size_t len,
                             mulib_elvi_scrape_result_t *results,size_t cap,
                             size_t *count) {
  mulib_json_t *root=mulib_json_parse(body,len,NULL);
  const mulib_json_t *items;size_t used=0;
  if(!root)return 0;
  items=melvi_eq_(p->json_results,"/")?root:melvi_json_at_(root,p->json_results);
  if(mulib_json_typeof(items)!=MULIB_JSON_ARRAY) {mulib_json_destroy(root);return 0;}
  /* No fallible operations from here on; ineligible rows are simply skipped. */
  for(size_t i=0,n=mulib_json_size(items);i<n && used<cap;i++) {
    const mulib_json_t *item=mulib_json_at(items,i);
    mulib_elvi_scrape_result_t hit={0};
    size_t ulen=0,tlen=0,k;
    const char *url=mulib_json_text(melvi_json_at_(item,p->json_url),&ulen);
    const char *title=mulib_json_text(melvi_json_at_(item,p->json_title),&tlen);
    if(!url || !ulen || ulen>=sizeof hit.url || memchr(url,0,ulen) ||
       !melvi_result_url_(p,url,hit.url,sizeof hit.url))continue;
    for(k=0;k<used;k++)if(melvi_eq_(results[k].url,hit.url))break;
    if(k==used && melvi_json_title_(title,tlen,hit.title,sizeof hit.title))results[used++]=hit;
  }
  mulib_json_destroy(root);*count=used;return 1;
}
int mulib_elvi_scrape_parse(const mulib_elvi_scrape_program_t *p,
                            const char *body,size_t len,
                            mulib_elvi_scrape_result_t *results,size_t cap,
                            size_t *count) {
  if(!melvi_program_(p)||(!body&&len)||!count||(!results&&cap)||len>2097152||
     (len&&memchr(body,0,len)))return 0;
  if(cap>p->max_results)cap=p->max_results;
  if(p->format==MULIB_ELVI_FORMAT_JSON)return melvi_parse_json_(p,body,len,results,cap,count);
  return melvi_parse_html_(p,body,len,results,cap,count);
}
static int melvi_media_type_(const char *ct,mulib_elvi_format_t format) {
  size_t n=0;while(ct[n] && ct[n]!=';' && ct[n]!=' ' && ct[n]!='\t')n++;
  if(format==MULIB_ELVI_FORMAT_JSON) {
    if(!mulib_webscan_equal_(ct,n,"application/json"))return 0;
  } else if(!mulib_webscan_equal_(ct,n,"text/html") &&
            !mulib_webscan_equal_(ct,n,"application/xhtml+xml"))return 0;
  while(ct[n]==' ' || ct[n]=='\t')n++;
  return !ct[n] || ct[n]==';';
}
int mulib_elvi_scrape_search(const mulib_elvi_scrape_program_t *p,
                             const mulib_fetch_client_t *client,
                             const char *query,unsigned limit,
                             mulib_elvi_scrape_result_t *results,size_t cap,
                             size_t *count,unsigned *http_status) {
  char url[8192],ct[128]={0};unsigned status=0;int ok=0;
  mulib_bounded_text_buffer_t body={0};mulib_fetch_client_t safe;
  if(!p||!client||!count||(!results&&cap)||!client->transport||
     !mulib_elvi_scrape_url(p,query,limit,url,sizeof url))return 0;
  safe=*client;
  /* Narrow the outgoing fetch policy regardless of caller configuration. */
  safe.policy.allow_http=0; /* Preserve an explicit HTTPS prohibition. */
  safe.policy.allow_private=0;
  if(!safe.policy.max_bytes||safe.policy.max_bytes>2097152) safe.policy.max_bytes=2097152;
  if(!safe.policy.max_url||safe.policy.max_url>8192)safe.policy.max_url=8192;
  body.limit=safe.policy.max_bytes;
  if(!mulib_fetch_get(&safe,url,&body,&status,ct,sizeof ct))goto done;
  if(http_status)*http_status=status;
  if(status!=200 || !melvi_media_type_(ct,p->format))goto done;
  if(body.len && !body.data)goto done;
  ok=mulib_elvi_scrape_parse(p,body.data,body.len,results,
                              cap<limit?cap:limit,count);
 done:mulib_bounded_text_buffer_free(&body);return ok;
}
int mulib_elvi_scrape_https_connect_verified(void *ctx,const mulib_uri_t *endpoint,
                                             mulib_tsl_t *session) {
  const mulib_elvi_scrape_https_t *tls=(const mulib_elvi_scrape_https_t *)ctx;
  mulib_elvi_scrape_peer_t peer={0};
  if(!tls || !tls->connect || !tls->peer || !endpoint || !endpoint->host.data ||
     !endpoint->host.length || !session || tls->connect(tls->context,endpoint,session)!=1 ||
     mulib_tsl_handshake(session)!=MULIB_TSL_OK ||
     tls->peer(tls->context,session,&peer)!=1 ||
     peer.certificate_verified!=1 || peer.hostname_verified!=1 ||
     (peer.tls_version!=0x0303 && peer.tls_version!=0x0304) ||
     !memchr(peer.hostname,0,sizeof peer.hostname) ||
     !memchr(peer.alpn,0,sizeof peer.alpn) ||
     (peer.alpn[0] && strcmp(peer.alpn,"http/1.1")) ||
     !mulib_webscan_equal_(endpoint->host.data,endpoint->host.length,peer.hostname))return 0;
  return 1;
}
int mulib_elvi_scrape_https_transport(void *ctx,const char *url,
    mulib_bounded_text_buffer_t *body,unsigned *status,char *type,size_t cap) {
  mulib_elvi_scrape_https_t *tls=(mulib_elvi_scrape_https_t *)ctx;
  mulib_http_message_t message={0};mulib_webclient_options_t options={0};
  char host[256];const char *ct,*encoding;size_t cursor=0,limit;int ok=0;
  if(!tls || !tls->connect || !tls->peer || !url || !body || !status || !type || !cap ||
     !melvi_host_(url,host,sizeof host) || strchr(url,'#') ||
     (body->cap && !body->data) || (body->len && body->len>=body->cap))return 0;
  limit=body->limit && body->limit<2097152?body->limit:2097152;
  if(body->len>=limit)return 0;
  options.http.max_body_bytes=limit-body->len;
  options.http.max_header_bytes=32768;options.http.max_headers=100;
  options.max_response_bytes=options.http.max_body_bytes+65536;
  if(mulib_https_request(url,"GET",NULL,0,NULL,0,&options,
        mulib_elvi_scrape_https_connect_verified,tls,&message)!=MULIB_WEBCLIENT_OK)goto done;
  ct=mulib_http_header_next(&message,"Content-Type",&cursor);
  if(mulib_http_header_next(&message,"Content-Type",&cursor))goto done;
  cursor=0;encoding=mulib_http_header_next(&message,"Content-Encoding",&cursor);
  if(encoding && (!mulib_webscan_equal_(encoding,strlen(encoding),"identity") ||
      mulib_http_header_next(&message,"Content-Encoding",&cursor)))goto done;
  if(!ct)ct="";
  if(strlen(ct)>=cap || !mulib_bounded_text_append(body,message.body,message.body_length))goto done;
  memcpy(type,ct,strlen(ct)+1);*status=message.status;ok=1;
 done:mulib_http_message_free(&message);return ok;
}
#ifdef __cplusplus
}
#endif
#endif
