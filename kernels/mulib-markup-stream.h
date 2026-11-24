/** @file mulib-markup-stream.h
 * @brief Source token stream, lossless CST snapshot, strict XML-like builder, tree event iterator.
 * ISO C11; define MULIB_IMPL_CDD_MARKUP_STREAM and MULIB_IMPL_CDD_MARKUP_TREE
 * once each, in the same or separate implementation translation units.
 * Legacy aliases: MULIB_IMPL_MARKUP_STREAM and MULIB_IMPL_MARKUP_TREE.
 * Grammar intentionally limited: ASCII XML names, quoted attributes, comments,
 * processing instructions, basic declarations, CDATA and self-closing tags.
 * No entity expansion, Unicode name validation, namespaces, HTML recovery or DTD.
 */
#ifndef MULIB_MARKUP_STREAM_H
#define MULIB_MARKUP_STREAM_H
#include "mulib-markup-tree.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_mkir_token_kind {
 MULIB_MKIR_T_TEXT=1,MULIB_MKIR_T_START,MULIB_MKIR_T_END,
 MULIB_MKIR_T_EMPTY,MULIB_MKIR_T_COMMENT,MULIB_MKIR_T_PI,MULIB_MKIR_T_DECL,
 MULIB_MKIR_T_CDATA
} mulib_mkir_token_kind_t;
typedef struct mulib_mkir_token {
  mulib_mkir_token_kind_t kind;
 /* Half-open byte offsets. Unavailable name/content ranges are both NONE.
    A CDATA token covers its delimiters; its content range excludes them. */
  size_t begin,end,name_begin,name_end,content_begin,content_end;
} mulib_mkir_token_t;
typedef struct mulib_mkir_scanner {
 const char *source;size_t length,cursor,error_offset;
} mulib_mkir_scanner_t;
/* Returns 1 token, 0 EOF, -1 malformed/invalid. The result and cursor change
   only on success. On failure error_offset is set if scan is non-NULL. */
int mulib_mkir_scan_next(mulib_mkir_scanner_t *scan,mulib_mkir_token_t *out);
typedef int (*mulib_mkir_token_callback_t)(const mulib_mkir_token_t *token,void *user);
/* Push all tokens to a callback: 1 success, 0 syntax/error, -1 callback stopped. */
int mulib_mkir_scan_walk(const char *src,size_t length,mulib_mkir_token_callback_t fn,
                         void *user,size_t *error_offset);
/* Parse a fragment into a document wrapper spanning the input; no implicit repair.
   Element depth (including self-closing tags) starts at 1, with 0 choosing
   the default limit 256. Output must be zero-initialized, and is published
   only on success; error_offset is initialized to 0 if non-NULL. */
int mulib_mkir_parse(const char *src,size_t length,size_t max_depth,
                     mulib_mkir_tree_t *out,size_t *error_offset);
typedef struct mulib_mkir_cst {
 char *source;size_t length;
 mulib_mkir_token_t *tokens;size_t count,capacity;
 mulib_mkir_tree_t tree;
} mulib_mkir_cst_t;
int mulib_mkir_cst_parse(const char *src,size_t length,size_t max_depth,
                         mulib_mkir_cst_t *out,size_t *error_offset);
/* Borrow a token's complete raw bytes (not necessarily NUL-terminated).
   Slices cover the original input in order and survive edits to cst->tree.
   Returns 0 without changing either output on error. */
int mulib_cdd_mkir_cst_token_bytes(const mulib_mkir_cst_t *cst,size_t token,
                                   const char **bytes,size_t *length);
void mulib_mkir_cst_free(mulib_mkir_cst_t *c);
typedef enum mulib_mkir_event_kind {
 MULIB_MKIR_E_ENTER=1,MULIB_MKIR_E_LEAVE,MULIB_MKIR_E_TEXT,
 MULIB_MKIR_E_COMMENT,MULIB_MKIR_E_OTHER
} mulib_mkir_event_kind_t;
typedef struct mulib_mkir_event {
 mulib_mkir_event_kind_t kind;
 size_t node,depth;const mulib_mkir_node_t *data;
} mulib_mkir_event_t;
typedef struct mulib_mkir_events {
 const mulib_mkir_tree_t *tree;
 size_t root,current,depth;
 int leaving;
 uint64_t version;
} mulib_mkir_events_t;
/* Zero-copy, sibling-ordered traversal confined to root (depth 0).
   Document, element and trivia nodes have paired ENTER/LEAVE events. Other
   nodes have one event, before any children. A mutation invalidates both
   the cursor and borrowed event data. Discard cursors before freeing or
   reusing the tree. Next returns 1 event, 0 EOF or -1 error;
   the output is unchanged on EOF/error. */
int mulib_mkir_events_begin(const mulib_mkir_tree_t *t,size_t root,mulib_mkir_events_t *it);
int mulib_mkir_events_next(mulib_mkir_events_t *it,mulib_mkir_event_t *event);
typedef int (*mulib_mkir_event_callback_t)(const mulib_mkir_event_t *event,void *user);
/* Returns 1 on completion, 0 on error, -1 when the callback returns 0. */
int mulib_mkir_events_walk(const mulib_mkir_tree_t *t,size_t root,
                           mulib_mkir_event_callback_t fn,void *user);
/* As events_walk, but returns -2 before delivering an event that would exceed
   max_events or max_depth. Root depth is 0; SIZE_MAX means unlimited.
   These limits apply to callback delivery, not to initial tree validation. */
int mulib_cdd_mkir_events_walk_bounded(const mulib_mkir_tree_t *t,size_t root,
                                      size_t max_depth,size_t max_events,
                                      mulib_mkir_event_callback_t fn,void *user);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CDD_MARKUP_STREAM) || defined(MULIB_IMPL_MARKUP_STREAM)
#ifndef MULIB_MARKUP_STREAM_IMPLEMENTED
#define MULIB_MARKUP_STREAM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_mkir_stream_space_(char c){return c==' '||c=='\n'||c=='\r'||c=='\t';}
static int mulib_mkir_stream_name_start_(char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'||c==':';}
static int mulib_mkir_stream_name_char_(char c){return mulib_mkir_stream_name_start_(c)||(c>='0'&&c<='9')||c=='.'||c=='-';}
static int mulib_mkir_stream_starts_(const char *s,size_t n,size_t at,const char *v,size_t l){return at<=n&&l<=n-at&&!memcmp(s+at,v,l);}
static char *mulib_mkir_stream_dup_(const char *p,size_t n){
 char *copy;
 if(n==SIZE_MAX)return NULL;
 copy=(char*)malloc(n+1);if(!copy)return NULL;
 if(n)memcpy(copy,p,n);
 copy[n]=0;return copy;
}
static void *mulib_mkir_stream_reserve_(void *p,size_t *cap,size_t n,size_t stride){
 size_t limit=SIZE_MAX/stride,next;
 if(n<=*cap)return p;
 if(n>limit)return NULL;
 next=*cap?*cap:(limit<8?limit:8);
 while(next<n){if(next>limit/2){next=n;break;}next*=2;}
 p=realloc(p,next*stride);if(!p)return NULL;
 *cap=next;return p;
}
int mulib_mkir_scan_next(mulib_mkir_scanner_t *s,mulib_mkir_token_t *out){
 const char *p;size_t n,i,start,mark,finish=0;mulib_mkir_token_t t;
 if(!s)return -1;
 if(!out||(!s->source&&s->length)||s->cursor>s->length||s->length==MULIB_MKIR_NONE){
   s->error_offset=s->cursor<=s->length?s->cursor:s->length;return -1;
 }
 p=s->source;n=s->length;i=s->cursor;if(i==n)return 0;
 start=i;t.begin=start;t.name_begin=t.name_end=MULIB_MKIR_NONE;
 t.content_begin=t.content_end=MULIB_MKIR_NONE;
 if(p[i]!='<'){
   while(i<n&&p[i]!='<')i++;
   t.kind=MULIB_MKIR_T_TEXT;t.end=i;t.content_begin=start;t.content_end=i;
 }else if(mulib_mkir_stream_starts_(p,n,i,"<!--",4)){
   i+=4;mark=i;
   while(n-i>=3&&!mulib_mkir_stream_starts_(p,n,i,"-->",3))i++;
   if(n-i<3)goto fail;
   t.kind=MULIB_MKIR_T_COMMENT;t.content_begin=mark;t.content_end=i;t.end=i+3;
 }else if(mulib_mkir_stream_starts_(p,n,i,"<?",2)){
   i+=2;mark=i;while(n-i>=2&&!mulib_mkir_stream_starts_(p,n,i,"?>",2))i++;
   if(n-i<2)goto fail;
   t.kind=MULIB_MKIR_T_PI;t.content_begin=mark;t.content_end=i;t.end=i+2;
 }else if(mulib_mkir_stream_starts_(p,n,i,"<![CDATA[",9)){
   i+=9;mark=i;
   while(n-i>=3&&!mulib_mkir_stream_starts_(p,n,i,"]]>",3))i++;
   if(n-i<3)goto fail;
   t.kind=MULIB_MKIR_T_CDATA;t.content_begin=mark;t.content_end=i;t.end=i+3;
 }else if(mulib_mkir_stream_starts_(p,n,i,"<!",2)){
   i+=2;mark=i;while(i<n&&p[i]!='>')i++;
   if(i==n)goto fail;
   t.kind=MULIB_MKIR_T_DECL;t.content_begin=mark;t.content_end=i;t.end=i+1;
 }else{
   i++;if(i<n&&p[i]=='/'){t.kind=MULIB_MKIR_T_END;i++;}else t.kind=MULIB_MKIR_T_START;
   if(i==n||!mulib_mkir_stream_name_start_(p[i]))goto fail;
   t.name_begin=i;
   do{i++;}while(i<n&&mulib_mkir_stream_name_char_(p[i]));
   t.name_end=i;
   if(t.kind==MULIB_MKIR_T_END){
     while(i<n&&mulib_mkir_stream_space_(p[i]))i++;
     if(i==n||p[i]!='>')goto fail;
     t.end=i+1;
   }else{
     t.content_begin=i;
     while(i<n){
       if(p[i]=='\''||p[i]=='"'){char quote=p[i++];while(i<n&&p[i]!=quote)i++;if(i==n)goto fail;i++;continue;}
       if(p[i]=='>'){t.end=i+1;finish=i;break;}
       i++;
     }
     if(i==n)goto fail;
     /* Whitespace before the closing '>' is allowed; empty tag has '/'. */
     while(finish>t.content_begin&&mulib_mkir_stream_space_(p[finish-1]))finish--;
     if(finish>t.content_begin&&p[finish-1]=='/'){t.kind=MULIB_MKIR_T_EMPTY;t.content_end=finish-1;}
     else t.content_end=finish;
   }
 }
 s->cursor=t.end;*out=t;return 1;
fail:s->error_offset=start;return -1;
}
int mulib_mkir_scan_walk(const char *src,size_t n,mulib_mkir_token_callback_t fn,void *user,size_t *error){
 mulib_mkir_scanner_t s={src,n,0,0};mulib_mkir_token_t token;int r;
 if(error)*error=0;
 if(!fn||(!src&&n))return 0;
 while((r=mulib_mkir_scan_next(&s,&token))==1)if(!fn(&token,user))return -1;
 if(r<0&&error)*error=s.error_offset;
 return r<0?0:1;
}
static int mulib_mkir_stream_equal_name_(const char *p,const mulib_mkir_token_t *tok,const char *s){
 size_t n=tok->name_end-tok->name_begin;
 return s&&strlen(s)==n&&!memcmp(p+tok->name_begin,s,n);
}
typedef struct mulib_mkir_stream_key_ {const char *key;size_t offset;} mulib_mkir_stream_key_;
typedef struct mulib_mkir_stream_keybuf_ {mulib_mkir_stream_key_ *keys;size_t capacity;} mulib_mkir_stream_keybuf_;
static int mulib_mkir_stream_key_order_(const void *ap,const void *bp){
 const mulib_mkir_stream_key_ *a=(const mulib_mkir_stream_key_*)ap;
 const mulib_mkir_stream_key_ *b=(const mulib_mkir_stream_key_*)bp;
 int c=strcmp(a->key,b->key);
 return c?c:a->offset==b->offset?0:a->offset<b->offset?-1:1;
}
static int mulib_mkir_stream_parse_attrs_(mulib_mkir_tree_t *t,size_t id,const char *src,
                                          mulib_mkir_token_t tok,mulib_mkir_stream_keybuf_ *scratch,
                                          size_t *bad){
 size_t at=tok.content_begin,end=tok.content_end,used=0;
 mulib_mkir_stream_key_ first;
 while(at<end){
   size_t key,ke,v,ve;char quote,*k,*value;mulib_mkir_attr_t *attrs;
   if(!mulib_mkir_stream_space_(src[at]))goto invalid;
   while(at<end&&mulib_mkir_stream_space_(src[at]))at++;
   if(at==end)break;
   key=at;if(!mulib_mkir_stream_name_start_(src[at]))goto invalid;
   while(at<end&&mulib_mkir_stream_name_char_(src[at]))at++;
   ke=at;while(at<end&&mulib_mkir_stream_space_(src[at]))at++;
   if(at==end||src[at]!='=')goto invalid;
   at++;while(at<end&&mulib_mkir_stream_space_(src[at]))at++;
   if(at==end||(src[at]!='"'&&src[at]!='\''))goto invalid;
   quote=src[at++];v=at;while(at<end&&src[at]!=quote){if(!src[at])goto invalid;at++;}
   if(at==end)goto invalid;
   ve=at++;
   k=mulib_mkir_stream_dup_(src+key,ke-key);
   if(!k)goto invalid;
   value=mulib_mkir_stream_dup_(src+v,ve-v);
   if(!value){free(k);goto invalid;}
   attrs=(mulib_mkir_attr_t*)mulib_mkir_stream_reserve_(t->attrs,&t->attr_capacity,
                                                       t->attr_count+1,sizeof(*attrs));
   if(!attrs){free(k);free(value);goto invalid;}
   t->attrs=attrs;
   {
     const mulib_mkir_attr_t attr={id,k,value};t->attrs[t->attr_count++]=attr;
   }
   t->version++;
   /* Sort an index rather than the attribute records, to retain source
      order. Reuse scratch across tags and avoid it for single attributes. */
   if(used==0){first.key=k;first.offset=key;}
   else{
     mulib_mkir_stream_key_ *keys=(mulib_mkir_stream_key_*)mulib_mkir_stream_reserve_(
       scratch->keys,&scratch->capacity,used+1,sizeof(*keys));
     if(!keys)goto invalid;
     scratch->keys=keys;
     if(used==1)keys[0]=first;
     keys[used].key=k;keys[used].offset=key;
   }
   used++;
 }
 if(used>1){
   const mulib_mkir_stream_key_ *keys=scratch->keys;
   size_t conflict=MULIB_MKIR_NONE;
   qsort(scratch->keys,used,sizeof(*keys),mulib_mkir_stream_key_order_);
   for(size_t i=1;i<used;i++){
     if(keys[i].offset<conflict&&!strcmp(keys[i-1].key,keys[i].key))conflict=keys[i].offset;
   }
   if(conflict!=MULIB_MKIR_NONE){at=conflict;goto invalid;}
 }
 return 1;
invalid:if(bad)*bad=at;return 0;
}
static int mulib_mkir_stream_empty_tree_(const mulib_mkir_tree_t *t){
 return t&&!t->nodes&&!t->count&&!t->capacity&&!t->attrs&&
        !t->attr_count&&!t->attr_capacity&&!t->version;
}
/* Build the tree and (optionally) retain its tokens in the same pass. */
static int mulib_mkir_stream_build_(const char *src,size_t length,size_t depth_limit,
                                    mulib_mkir_tree_t *t,mulib_mkir_cst_t *cst,size_t *error){
 mulib_mkir_scanner_t scan={src,length,0,0};
 mulib_mkir_stream_keybuf_ scratch={NULL,0};
 mulib_mkir_token_t tok;size_t root,current,depth=0,id,bad=0;int r;
 if(!depth_limit)depth_limit=256;
 if(!mulib_mkir_tree_add(t,MULIB_MKIR_NONE,MULIB_MKIR_DOCUMENT,NULL,NULL,0,0,length,&root))goto fail;
 current=root;
 while((r=mulib_mkir_scan_next(&scan,&tok))==1){
   size_t b=tok.begin,e=tok.end;
   if(cst){
     mulib_mkir_token_t *tokens=(mulib_mkir_token_t*)mulib_mkir_stream_reserve_(
       cst->tokens,&cst->capacity,cst->count+1,sizeof(*tokens));
     if(!tokens){bad=b;goto fail;}
     cst->tokens=tokens;cst->tokens[cst->count++]=tok;
   }
   if(tok.kind==MULIB_MKIR_T_END){
     if(current==root||!mulib_mkir_stream_equal_name_(src,&tok,t->nodes[current].name)){bad=b;goto fail;}
     t->nodes[current].source_end=e;
     current=t->nodes[current].parent;depth--;
   }else if(tok.kind==MULIB_MKIR_T_START||tok.kind==MULIB_MKIR_T_EMPTY){
     char *name;
     if(depth>=depth_limit){bad=b;goto fail;}
     name=mulib_mkir_stream_dup_(src+tok.name_begin,tok.name_end-tok.name_begin);
     if(!name){bad=b;goto fail;}
     if(!mulib_mkir_tree_add(t,current,MULIB_MKIR_ELEMENT,name,NULL,0,b,e,&id)){
       free(name);bad=b;goto fail;
     }
     free(name);
     if(!mulib_mkir_stream_parse_attrs_(t,id,src,tok,&scratch,&bad))goto fail;
     if(tok.kind==MULIB_MKIR_T_START){current=id;depth++;}
   }else{
     mulib_mkir_kind_t kind=tok.kind==MULIB_MKIR_T_TEXT?MULIB_MKIR_TEXT:
       tok.kind==MULIB_MKIR_T_COMMENT?MULIB_MKIR_COMMENT:
       tok.kind==MULIB_MKIR_T_CDATA?MULIB_MKIR_CDATA:
       tok.kind==MULIB_MKIR_T_PI?MULIB_MKIR_PI:MULIB_MKIR_DOCTYPE;
     if(!mulib_mkir_tree_add(t,current,kind,NULL,src+tok.content_begin,
           tok.content_end-tok.content_begin,b,e,&id)){bad=b;goto fail;}
   }
 }
 if(r<0){bad=scan.error_offset;goto fail;}
 if(current!=root){bad=length;goto fail;}
 free(scratch.keys);return 1;
 fail:free(scratch.keys);if(error)*error=bad;return 0;
}
int mulib_mkir_parse(const char *src,size_t length,size_t depth_limit,mulib_mkir_tree_t *out,size_t *error){
 mulib_mkir_tree_t tmp={NULL,0,0,NULL,0,0,0};
 if(error)*error=0;
 if(!mulib_mkir_stream_empty_tree_(out)||(!src&&length)||length==MULIB_MKIR_NONE)return 0;
 if(!mulib_mkir_stream_build_(src,length,depth_limit,&tmp,NULL,error)){mulib_mkir_tree_free(&tmp);return 0;}
 *out=tmp;return 1;
}
int mulib_mkir_cst_parse(const char *src,size_t n,size_t max_depth,mulib_mkir_cst_t *out,size_t *error){
 mulib_mkir_cst_t tmp={NULL,0,NULL,0,0,{NULL,0,0,NULL,0,0,0}};
 if(error)*error=0;
 if(!out||out->source||out->length||out->tokens||out->count||out->capacity||
    !mulib_mkir_stream_empty_tree_(&out->tree)||(!src&&n)||n==MULIB_MKIR_NONE)return 0;
 tmp.source=mulib_mkir_stream_dup_(src,n);if(!tmp.source)return 0;tmp.length=n;
 if(!mulib_mkir_stream_build_(tmp.source,n,max_depth,&tmp.tree,&tmp,error))goto fail;
 *out=tmp;return 1;
 fail:mulib_mkir_cst_free(&tmp);return 0;
}
int mulib_cdd_mkir_cst_token_bytes(const mulib_mkir_cst_t *c,size_t index,const char **bytes,size_t *length){
 const mulib_mkir_token_t *t;
 if(!c||!bytes||!length||!c->source||!c->tokens||c->count>c->capacity||
    c->capacity>SIZE_MAX/sizeof(*c->tokens)||c->length==MULIB_MKIR_NONE||index>=c->count)return 0;
 t=&c->tokens[index];
 if(t->begin>=t->end||t->end>c->length)return 0;
 *bytes=c->source+t->begin;*length=t->end-t->begin;return 1;
}
void mulib_mkir_cst_free(mulib_mkir_cst_t *c){
 const mulib_mkir_cst_t empty={NULL,0,NULL,0,0,{NULL,0,0,NULL,0,0,0}};
 if(!c)return;
 free(c->source);free(c->tokens);mulib_mkir_tree_free(&c->tree);*c=empty;
}
int mulib_mkir_events_begin(const mulib_mkir_tree_t *t,size_t root,mulib_mkir_events_t *it){
 mulib_mkir_events_t start;
 if(!t||!it||root>=t->count||!mulib_mkir_tree_validate(t))return 0;
 start.tree=t;start.root=root;start.current=root;start.depth=0;start.leaving=0;
 start.version=t->version;*it=start;return 1;
}
static mulib_mkir_event_kind_t mulib_mkir_stream_enter_(mulib_mkir_kind_t k){
 switch(k){
 case MULIB_MKIR_TEXT:case MULIB_MKIR_CDATA:return MULIB_MKIR_E_TEXT;
 case MULIB_MKIR_COMMENT:return MULIB_MKIR_E_COMMENT;
 case MULIB_MKIR_PI:case MULIB_MKIR_DOCTYPE:return MULIB_MKIR_E_OTHER;
 default:return MULIB_MKIR_E_ENTER;
 }
}
int mulib_mkir_events_next(mulib_mkir_events_t *it,mulib_mkir_event_t *out){
 const mulib_mkir_tree_t *t;const mulib_mkir_node_t *n;mulib_mkir_event_t event;
 size_t next,depth;int leaving=0;
 if(!it||!out||!(t=it->tree)||it->version!=t->version||
    !t->nodes||t->count>t->capacity||it->root>=t->count)return -1;
 if(it->current==MULIB_MKIR_NONE)return 0;
 next=it->current;depth=it->depth;
 if(next>=t->count||depth>=t->count||(next==it->root)!=(depth==0))return -1;
 n=&t->nodes[next];
 event.node=next;event.depth=depth;event.data=n;
 event.kind=mulib_mkir_stream_enter_(n->kind);
 if(it->leaving){
   if(event.kind!=MULIB_MKIR_E_ENTER)return -1;
   event.kind=MULIB_MKIR_E_LEAVE;
 }
 if(!it->leaving&&n->first_child!=MULIB_MKIR_NONE){
   next=n->first_child;depth++;
   if(next>=t->count||depth>=t->count||t->nodes[next].parent!=event.node)return -1;
 }else if(event.kind==MULIB_MKIR_E_ENTER){
   leaving=1;
 }else{
   for(;;){
     size_t parent;
     if(next==it->root){next=MULIB_MKIR_NONE;break;}
     if(!depth)return -1;
     parent=n->parent;
     if(parent>=t->count)return -1;
     if(n->next!=MULIB_MKIR_NONE){
       next=n->next;
       if(next>=t->count||t->nodes[next].parent!=parent)return -1;
       break;
     }
     next=parent;n=&t->nodes[next];depth--;
     if(mulib_mkir_stream_enter_(n->kind)==MULIB_MKIR_E_ENTER){leaving=1;break;}
   }
 }
 it->current=next;it->depth=depth;it->leaving=leaving;*out=event;return 1;
}
int mulib_mkir_events_walk(const mulib_mkir_tree_t *t,size_t root,mulib_mkir_event_callback_t fn,void *user){
 return mulib_cdd_mkir_events_walk_bounded(t,root,SIZE_MAX,SIZE_MAX,fn,user);
}
int mulib_cdd_mkir_events_walk_bounded(const mulib_mkir_tree_t *t,size_t root,
                                      size_t max_depth,size_t max_events,
                                      mulib_mkir_event_callback_t fn,void *user){
 mulib_mkir_events_t it;mulib_mkir_event_t ev;int r;
 if(!fn||!mulib_mkir_events_begin(t,root,&it))return 0;
 while((r=mulib_mkir_events_next(&it,&ev))==1){
   if(ev.depth>max_depth||!max_events)return -2;
   max_events--;
   if(!fn(&ev,user))return -1;
 }
 return r==0?1:0;
}
#ifdef __cplusplus
}
#endif
#endif
#endif
