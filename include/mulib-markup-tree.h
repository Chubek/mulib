/** @file mulib-markup-tree.h
 * @brief Arena-backed, ordered markup CST/AST/DOM-like tree and SXML output.
 * ISO C11. Define MULIB_IMPL_CDD_MARKUP_TREE in exactly one translation unit
 * (legacy alias: MULIB_IMPL_MARKUP_TREE).
 * Stable node IDs; strings and attributes are owned; all offsets are byte offsets.
 * This is not a WHATWG DOM or a standards-compliant XML parser.
 */
#ifndef MULIB_MARKUP_TREE_H
#define MULIB_MARKUP_TREE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_MKIR_NONE SIZE_MAX
typedef enum mulib_mkir_kind {
  MULIB_MKIR_DOCUMENT=1, MULIB_MKIR_ELEMENT, MULIB_MKIR_TEXT,
  MULIB_MKIR_COMMENT, MULIB_MKIR_CDATA, MULIB_MKIR_PI,
  MULIB_MKIR_DOCTYPE, MULIB_MKIR_TRIVIA
} mulib_mkir_kind_t;
typedef struct mulib_mkir_node {
  mulib_mkir_kind_t kind;
  size_t parent, first_child, last_child, prev, next;
  size_t source_begin, source_end; /* half-open; NONE when synthetic */
  char *name, *value; size_t value_length;
} mulib_mkir_node_t;
typedef struct mulib_mkir_attr {size_t owner; char *key, *value;} mulib_mkir_attr_t;
typedef struct mulib_mkir_tree {
  mulib_mkir_node_t *nodes; size_t count, capacity;
  mulib_mkir_attr_t *attrs; size_t attr_count, attr_capacity;
  uint64_t version;
} mulib_mkir_tree_t;
/* Zero-initialize all owner structs; destroy before reusing them.
   Any kind may contain children; this generic forest does not impose XML
   content rules. Detached roots are visited in increasing node-ID order. */
int mulib_mkir_tree_add(mulib_mkir_tree_t *t, size_t parent, mulib_mkir_kind_t kind,
                        const char *name, const void *value, size_t length,
                        size_t begin, size_t end, size_t *id);
/* Reparent an existing node at the end of parent's child list; NONE detaches. */
int mulib_mkir_tree_attach(mulib_mkir_tree_t *t,size_t child,size_t parent);
/* Move child immediately before a child of parent; before=NONE appends,
   parent=before=NONE detaches. IDs and source offsets do not change.
   A rejected move or a move that leaves the order unchanged is not a mutation. */
int mulib_cdd_mkir_tree_insert_before(mulib_mkir_tree_t *t,size_t child,
                                     size_t parent,size_t before);
int mulib_mkir_tree_value(mulib_mkir_tree_t *t,size_t id,const void *bytes,size_t length);
int mulib_mkir_tree_attr(mulib_mkir_tree_t *t,size_t id,const char *key,const char *value);
const char *mulib_mkir_tree_get_attr(const mulib_mkir_tree_t *t,size_t id,const char *key);
/* Validate an ordered forest, including detached roots and unique per-node
   attribute keys. Non-NULL strings and arrays must refer to valid storage. */
int mulib_mkir_tree_validate(const mulib_mkir_tree_t *t);
/* Deep-copy the forest; optionally prune comment/trivia subtrees. */
int mulib_mkir_tree_ast(const mulib_mkir_tree_t *source,mulib_mkir_tree_t *out,int strip_trivia);
/* Allocates a zero-terminated SXML-style S-expression with escaped symbols
   and binary text. This is a display format, not a lossless tree codec. */
char *mulib_mkir_tree_sxml(const mulib_mkir_tree_t *t,size_t root);
void mulib_mkir_tree_free(mulib_mkir_tree_t *t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CDD_MARKUP_TREE) || defined(MULIB_IMPL_MARKUP_TREE)
#ifndef MULIB_MARKUP_TREE_IMPLEMENTED
#define MULIB_MARKUP_TREE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static void *mulib_mkir_tree_reserve_(void *ptr,size_t *capacity,size_t need,size_t unit) {
  size_t n,limit=SIZE_MAX/unit;void *p;
  if(need<=*capacity)return ptr;
  if(need>limit)return NULL;
  n=*capacity?*capacity:(limit<8?limit:8);
  while(n<need){if(n>limit/2){n=need;break;}n*=2;}
  p=realloc(ptr,n*unit);if(!p)return NULL;
  *capacity=n;return p;
}
static char *mulib_mkir_tree_dup_(const void *p,size_t n) {
  char *s;if((!p&&n)||n==SIZE_MAX)return NULL;
  s=(char*)malloc(n+1);if(!s)return NULL;
  if(n)memcpy(s,p,n);
  s[n]=0;return s;
}
static int mulib_mkir_tree_kind_(mulib_mkir_kind_t k){
  return k>=MULIB_MKIR_DOCUMENT&&k<=MULIB_MKIR_TRIVIA;
}
static int mulib_mkir_tree_range_(size_t begin,size_t end){
  return end==MULIB_MKIR_NONE?begin==MULIB_MKIR_NONE:begin<=end;
}
static int mulib_mkir_tree_storage_(const mulib_mkir_tree_t *t){
  return t&&t->count<=t->capacity&&t->capacity<=SIZE_MAX/sizeof(*t->nodes)&&
    (t->nodes!=NULL)==(t->capacity!=0)&&
    t->attr_count<=t->attr_capacity&&t->attr_capacity<=SIZE_MAX/sizeof(*t->attrs)&&
    (t->attrs!=NULL)==(t->attr_capacity!=0);
}
static int mulib_mkir_tree_ends_(const mulib_mkir_tree_t *t,size_t parent){
  size_t first,last;
  if(parent==MULIB_MKIR_NONE)return 1;
  if(parent>=t->count)return 0;
  first=t->nodes[parent].first_child;last=t->nodes[parent].last_child;
  if(first==MULIB_MKIR_NONE)return last==MULIB_MKIR_NONE;
  return first<t->count&&last<t->count&&first!=parent&&last!=parent&&
    t->nodes[first].parent==parent&&t->nodes[first].prev==MULIB_MKIR_NONE&&
    t->nodes[last].parent==parent&&t->nodes[last].next==MULIB_MKIR_NONE;
}
static int mulib_mkir_tree_linked_(const mulib_mkir_tree_t *t,size_t id){
  const mulib_mkir_node_t *n=&t->nodes[id];
  size_t parent=n->parent,prev=n->prev,next=n->next;
  if(parent==MULIB_MKIR_NONE)return prev==MULIB_MKIR_NONE&&next==MULIB_MKIR_NONE;
  if(parent==id||!mulib_mkir_tree_ends_(t,parent))return 0;
  return (prev==MULIB_MKIR_NONE?t->nodes[parent].first_child==id:
          prev<t->count&&prev!=id&&t->nodes[prev].parent==parent&&t->nodes[prev].next==id)&&
         (next==MULIB_MKIR_NONE?t->nodes[parent].last_child==id:
          next<t->count&&next!=id&&t->nodes[next].parent==parent&&t->nodes[next].prev==id);
}
int mulib_mkir_tree_add(mulib_mkir_tree_t *t,size_t parent,mulib_mkir_kind_t kind,
 const char *name,const void *value,size_t length,size_t begin,size_t end,size_t *id){
  char *name_copy=NULL,*value_copy=NULL;size_t i;mulib_mkir_node_t *nodes;
  if(!mulib_mkir_tree_storage_(t)||!id||(!value&&length)||length==SIZE_MAX||
     !mulib_mkir_tree_kind_(kind)||!mulib_mkir_tree_ends_(t,parent)||
     !mulib_mkir_tree_range_(begin,end)||(kind==MULIB_MKIR_ELEMENT&&(!name||!*name)))return 0;
  if(name){name_copy=mulib_mkir_tree_dup_(name,strlen(name));if(!name_copy)return 0;}
  if(value){value_copy=mulib_mkir_tree_dup_(value,length);if(!value_copy){free(name_copy);return 0;}}
  nodes=(mulib_mkir_node_t*)mulib_mkir_tree_reserve_(t->nodes,&t->capacity,t->count+1,sizeof(*nodes));
  if(!nodes){free(name_copy);free(value_copy);return 0;}
  t->nodes=nodes;
  i=t->count++;
  {
    const mulib_mkir_node_t n={kind,parent,MULIB_MKIR_NONE,MULIB_MKIR_NONE,
      MULIB_MKIR_NONE,MULIB_MKIR_NONE,begin,end,name_copy,value_copy,length};
    t->nodes[i]=n;
  }
  if(parent!=MULIB_MKIR_NONE){mulib_mkir_node_t *p=&t->nodes[parent];
    t->nodes[i].prev=p->last_child;
    if(p->last_child==MULIB_MKIR_NONE)p->first_child=i;
    else t->nodes[p->last_child].next=i;
    p->last_child=i;
  }
  t->version++;*id=i;return 1;
}
int mulib_mkir_tree_attach(mulib_mkir_tree_t *t,size_t child,size_t parent){
 return mulib_cdd_mkir_tree_insert_before(t,child,parent,MULIB_MKIR_NONE);
}
int mulib_cdd_mkir_tree_insert_before(mulib_mkir_tree_t *t,size_t child,size_t parent,size_t before){
 size_t p,old,steps=0;mulib_mkir_node_t *c,*n;
 if(!mulib_mkir_tree_storage_(t)||child>=t->count||!mulib_mkir_tree_ends_(t,parent)||
    !mulib_mkir_tree_linked_(t,child))return 0;
 if(before!=MULIB_MKIR_NONE&&(parent==MULIB_MKIR_NONE||before>=t->count||
    t->nodes[before].parent!=parent||!mulib_mkir_tree_linked_(t,before)))return 0;
 for(p=parent;p!=MULIB_MKIR_NONE;p=t->nodes[p].parent){
   if(p>=t->count||++steps>t->count||p==child)return 0;
 }
 c=&t->nodes[child];old=c->parent;
 if(old==parent&&(before==child||before==c->next))return 1;
 if(old!=MULIB_MKIR_NONE){n=&t->nodes[old];
    if(n->first_child==child)n->first_child=c->next;
    if(n->last_child==child)n->last_child=c->prev;
 }
 if(c->prev!=MULIB_MKIR_NONE)t->nodes[c->prev].next=c->next;
 if(c->next!=MULIB_MKIR_NONE)t->nodes[c->next].prev=c->prev;
 c->parent=parent;c->prev=c->next=MULIB_MKIR_NONE;
 if(parent!=MULIB_MKIR_NONE){
   n=&t->nodes[parent];c->next=before;
   c->prev=before==MULIB_MKIR_NONE?n->last_child:t->nodes[before].prev;
   if(c->prev==MULIB_MKIR_NONE)n->first_child=child;
   else t->nodes[c->prev].next=child;
   if(before==MULIB_MKIR_NONE)n->last_child=child;
   else t->nodes[before].prev=child;
 }
 t->version++;return 1;
}
int mulib_mkir_tree_value(mulib_mkir_tree_t *t,size_t id,const void *p,size_t n){
 char *copy=NULL;if(!mulib_mkir_tree_storage_(t)||id>=t->count||(!p&&n))return 0;
 if(p){copy=mulib_mkir_tree_dup_(p,n);if(!copy)return 0;}
 free(t->nodes[id].value);t->nodes[id].value=copy;t->nodes[id].value_length=n;t->version++;return 1;
}
/* Append one attribute with no duplicate check (caller has established uniqueness). */
static int mulib_mkir_tree_append_attr_(mulib_mkir_tree_t *t,size_t id,const char *key,const char *value){
 mulib_mkir_attr_t *attrs;
 char *v=mulib_mkir_tree_dup_(value,strlen(value)),*k;
 if(!v)return 0;
 k=mulib_mkir_tree_dup_(key,strlen(key));if(!k){free(v);return 0;}
 attrs=(mulib_mkir_attr_t*)mulib_mkir_tree_reserve_(t->attrs,&t->attr_capacity,t->attr_count+1,sizeof(*attrs));
 if(!attrs){free(k);free(v);return 0;}
 t->attrs=attrs;
 {
   const mulib_mkir_attr_t a={id,k,v};t->attrs[t->attr_count++]=a;
 }
 t->version++;return 1;
}
int mulib_mkir_tree_attr(mulib_mkir_tree_t *t,size_t id,const char *key,const char *value){
 if(!mulib_mkir_tree_storage_(t)||id>=t->count||t->nodes[id].kind!=MULIB_MKIR_ELEMENT||
    !key||!*key||!value)return 0;
 for(size_t i=0;i<t->attr_count;i++)if(t->attrs[i].owner==id&&t->attrs[i].key&&
                                      !strcmp(t->attrs[i].key,key)){
   char *v=mulib_mkir_tree_dup_(value,strlen(value));if(!v)return 0;
   free(t->attrs[i].value);t->attrs[i].value=v;t->version++;return 1;
 }
 return mulib_mkir_tree_append_attr_(t,id,key,value);
}
const char *mulib_mkir_tree_get_attr(const mulib_mkir_tree_t *t,size_t id,const char *key){
 if(!mulib_mkir_tree_storage_(t)||id>=t->count||!key)return NULL;
 for(size_t i=0;i<t->attr_count;i++)if(t->attrs[i].owner==id&&t->attrs[i].key&&
                                      !strcmp(t->attrs[i].key,key))return t->attrs[i].value;
 return NULL;
}
static int mulib_mkir_tree_attr_key_order_(const void *ap,const void *bp){
 const mulib_mkir_attr_t *a=*(const mulib_mkir_attr_t *const*)ap;
 const mulib_mkir_attr_t *b=*(const mulib_mkir_attr_t *const*)bp;
 if(a->owner!=b->owner)return a->owner<b->owner?-1:1;
 return strcmp(a->key,b->key);
}
static const mulib_mkir_attr_t **mulib_mkir_tree_attr_index_(const mulib_mkir_tree_t *t,
                                                           int (*cmp)(const void*,const void*)){
 const mulib_mkir_attr_t **index;
 if(!t->attr_count||t->attr_count>SIZE_MAX/sizeof(*index))return NULL;
 index=(const mulib_mkir_attr_t**)malloc(t->attr_count*sizeof(*index));if(!index)return NULL;
 for(size_t i=0;i<t->attr_count;i++)index[i]=&t->attrs[i];
 qsort(index,t->attr_count,sizeof(*index),cmp);return index;
}
int mulib_mkir_tree_validate(const mulib_mkir_tree_t *t){
 size_t edges=0,roots=0,visited=0;
 if(!mulib_mkir_tree_storage_(t))return 0;
 for(size_t i=0;i<t->count;i++){
   const mulib_mkir_node_t *n=&t->nodes[i];size_t last=MULIB_MKIR_NONE;
   if(!mulib_mkir_tree_kind_(n->kind)||(n->kind==MULIB_MKIR_ELEMENT&&(!n->name||!*n->name))||
      (n->value_length&&!n->value)||n->value_length==SIZE_MAX||
      !mulib_mkir_tree_range_(n->source_begin,n->source_end)||
      !mulib_mkir_tree_linked_(t,i))return 0;
   if(n->parent==MULIB_MKIR_NONE)roots++;
   for(size_t c=n->first_child;c!=MULIB_MKIR_NONE;c=t->nodes[c].next){
     if(c>=t->count||edges==t->count||t->nodes[c].parent!=i||t->nodes[c].prev!=last)return 0;
     edges++;last=c;
   }
   if(last!=n->last_child)return 0;
 }
 if(edges!=t->count-roots)return 0;
 /* Local checks prove each listed child occurs once. Walk only from roots:
    any disconnected parent cycle is then unvisited, without ancestor rescans. */
 for(size_t root=0;root<t->count;root++)if(t->nodes[root].parent==MULIB_MKIR_NONE){
   size_t i=root;
   for(;;){
     visited++;
     if(t->nodes[i].first_child!=MULIB_MKIR_NONE){i=t->nodes[i].first_child;continue;}
     while(i!=root&&t->nodes[i].next==MULIB_MKIR_NONE)i=t->nodes[i].parent;
     if(i==root)break;
     i=t->nodes[i].next;
   }
 }
 if(visited!=t->count)return 0;
 for(size_t i=0;i<t->attr_count;i++){
   const mulib_mkir_attr_t *a=&t->attrs[i];
   if(a->owner>=t->count||t->nodes[a->owner].kind!=MULIB_MKIR_ELEMENT||!a->key||!*a->key||!a->value)return 0;
 }
 if(t->attr_count>1){
   const mulib_mkir_attr_t **index=mulib_mkir_tree_attr_index_(t,mulib_mkir_tree_attr_key_order_);
   if(!index)return 0;
   for(size_t i=1;i<t->attr_count;i++)if(!mulib_mkir_tree_attr_key_order_(&index[i-1],&index[i])){
     free(index);return 0;
   }
   free(index);
 }
 return 1;
}
void mulib_mkir_tree_free(mulib_mkir_tree_t *t){
 const mulib_mkir_tree_t empty={NULL,0,0,NULL,0,0,0};
 if(!t)return;
 for(size_t i=0;i<t->count;i++){free(t->nodes[i].name);free(t->nodes[i].value);}
 for(size_t i=0;i<t->attr_count;i++){free(t->attrs[i].key);free(t->attrs[i].value);}
 free(t->nodes);free(t->attrs);*t=empty;
}
int mulib_mkir_tree_ast(const mulib_mkir_tree_t *src,mulib_mkir_tree_t *out,int strip){
 mulib_mkir_tree_t tmp={NULL,0,0,NULL,0,0,0};size_t *map;int ok=0;
 if(!out||src==out||out->nodes||out->attrs||out->count||out->attr_count||
    out->capacity||out->attr_capacity||out->version||
    !mulib_mkir_tree_validate(src)||src->count>SIZE_MAX/sizeof(size_t))return 0;
 if(!src->count)return 1;
 map=(size_t*)malloc(src->count*sizeof(*map));if(!map)return 0;
 for(size_t i=0;i<src->count;i++)map[i]=MULIB_MKIR_NONE;
 /* Iterative, sibling-ordered DFS: children need not have greater IDs after a move. */
 for(size_t root=0;root<src->count;root++)if(src->nodes[root].parent==MULIB_MKIR_NONE){
   size_t i=root;
   for(;;){
     const mulib_mkir_node_t *n=&src->nodes[i];size_t p=n->parent;
     if(!strip||(n->kind!=MULIB_MKIR_COMMENT&&n->kind!=MULIB_MKIR_TRIVIA)){
       if(!mulib_mkir_tree_add(&tmp,p==MULIB_MKIR_NONE?MULIB_MKIR_NONE:map[p],
          n->kind,n->name,n->value,n->value_length,n->source_begin,n->source_end,&map[i]))goto finish;
       if(n->first_child!=MULIB_MKIR_NONE){i=n->first_child;continue;}
     }
     while(i!=root&&src->nodes[i].next==MULIB_MKIR_NONE)i=src->nodes[i].parent;
     if(i==root)break;
     i=src->nodes[i].next;
   }
 }
 for(size_t i=0;i<src->attr_count;i++){
   const mulib_mkir_attr_t *a=&src->attrs[i];size_t owner=map[a->owner];
   if(owner!=MULIB_MKIR_NONE&&!mulib_mkir_tree_append_attr_(&tmp,owner,a->key,a->value))goto finish;
 }
 *out=tmp;ok=1;
 finish:free(map);if(!ok)mulib_mkir_tree_free(&tmp);return ok;
}
/* Append builder used for SXML rendering, with explicit capacity / NUL. */
typedef struct mulib_mkir_tree_textbuf_ {char *p;size_t n,cap;} mulib_mkir_tree_textbuf_;
static int mulib_mkir_tree_put_(mulib_mkir_tree_textbuf_ *b,const void *s,size_t n){
 char *p;
 if(n>=SIZE_MAX-b->n)return 0;
 p=(char*)mulib_mkir_tree_reserve_(b->p,&b->cap,b->n+n+1,1);if(!p)return 0;
 b->p=p;
 if(n)memcpy(b->p+b->n,s,n);
 b->n+=n;b->p[b->n]=0;return 1;
}
static int mulib_mkir_tree_literal_(mulib_mkir_tree_textbuf_ *b,const char *s){return mulib_mkir_tree_put_(b,s,strlen(s));}
static int mulib_mkir_tree_quote_(mulib_mkir_tree_textbuf_ *b,const char *s,size_t n,char delimiter){
 const char h[]="0123456789abcdef";if(!mulib_mkir_tree_put_(b,&delimiter,1))return 0;
 for(size_t i=0;i<n;i++){
   unsigned char c=(unsigned char)s[i];char esc[6]={'\\','u','0','0',h[c>>4],h[c&15]};
   if(c==(unsigned char)delimiter||c=='\\'){char pair[2]={'\\',(char)c};if(!mulib_mkir_tree_put_(b,pair,2))return 0;}
   else if(c<32 || c==127){if(!mulib_mkir_tree_put_(b,esc,6))return 0;}
   else if(!mulib_mkir_tree_put_(b,s+i,1))return 0;
 }
 return mulib_mkir_tree_put_(b,&delimiter,1);
}
static int mulib_mkir_tree_symbol_(mulib_mkir_tree_textbuf_ *b,const char *s){
 size_t n=strlen(s);
 if(!n)return mulib_mkir_tree_quote_(b,s,n,'|');
 for(size_t i=0;i<n;i++){
   unsigned char c=(unsigned char)s[i];
   if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c==':'||c=='_'||
        (i&&((c>='0'&&c<='9')||c=='-'||c=='.'))))
     return mulib_mkir_tree_quote_(b,s,n,'|');
 }
 return mulib_mkir_tree_put_(b,s,n);
}
static int mulib_mkir_tree_attr_owner_order_(const void *ap,const void *bp){
 const mulib_mkir_attr_t *a=*(const mulib_mkir_attr_t *const*)ap;
 const mulib_mkir_attr_t *b=*(const mulib_mkir_attr_t *const*)bp;
 if(a->owner!=b->owner)return a->owner<b->owner?-1:1;
 /* Every pointer in the index belongs to the same attribute array. */
 return a==b?0:a<b?-1:1;
}
static int mulib_mkir_tree_text_kind_(mulib_mkir_kind_t k){
 return k==MULIB_MKIR_TEXT||k==MULIB_MKIR_CDATA||k==MULIB_MKIR_TRIVIA;
}
char *mulib_mkir_tree_sxml(const mulib_mkir_tree_t *t,size_t root){
 mulib_mkir_tree_textbuf_ b={NULL,0,0};
 const mulib_mkir_attr_t **index=NULL;size_t current;int entering=1;
 if(!t||root>=t->count||!mulib_mkir_tree_validate(t))return NULL;
 if(t->attr_count){
   index=mulib_mkir_tree_attr_index_(t,mulib_mkir_tree_attr_owner_order_);
   if(!index)return NULL;
 }
 current=root;
 for(;;){
   const mulib_mkir_node_t *n=&t->nodes[current];
   int bare=mulib_mkir_tree_text_kind_(n->kind)&&n->first_child==MULIB_MKIR_NONE;
   if(entering){
     if(bare){
       if(!mulib_mkir_tree_quote_(&b,n->value?n->value:"",n->value_length,'"'))goto fail;
     }else{
       const char *name=n->kind==MULIB_MKIR_DOCUMENT?"*TOP*":
         n->kind==MULIB_MKIR_ELEMENT?n->name:n->kind==MULIB_MKIR_COMMENT?"*COMMENT*":
         n->kind==MULIB_MKIR_PI?"*PI*":n->kind==MULIB_MKIR_DOCTYPE?"*DOCTYPE*":
         n->kind==MULIB_MKIR_TEXT?"*TEXT*":n->kind==MULIB_MKIR_CDATA?"*CDATA*":"*TRIVIA*";
       if(!mulib_mkir_tree_literal_(&b,"(")||
          !(n->kind==MULIB_MKIR_ELEMENT?mulib_mkir_tree_symbol_(&b,name):mulib_mkir_tree_literal_(&b,name)))goto fail;
       if(n->kind==MULIB_MKIR_ELEMENT&&index){
         size_t first=0,end=t->attr_count;
         while(first<end){
           size_t mid=first+(end-first)/2;
           if(index[mid]->owner<current)first=mid+1;
           else end=mid;
         }
         if(first<t->attr_count&&index[first]->owner==current){
           if(!mulib_mkir_tree_literal_(&b," (@"))goto fail;
           for(size_t j=first;j<t->attr_count&&index[j]->owner==current;j++){
             if(!mulib_mkir_tree_literal_(&b," (")||!mulib_mkir_tree_symbol_(&b,index[j]->key)||
                !mulib_mkir_tree_literal_(&b," ")||!mulib_mkir_tree_quote_(&b,index[j]->value,strlen(index[j]->value),'"')||
                !mulib_mkir_tree_literal_(&b,")"))goto fail;
           }
           if(!mulib_mkir_tree_literal_(&b,")"))goto fail;
         }
       }
       if((n->value&&n->value_length)||mulib_mkir_tree_text_kind_(n->kind)){
         if(!mulib_mkir_tree_literal_(&b," ")||
            !mulib_mkir_tree_quote_(&b,n->value?n->value:"",n->value_length,'"'))goto fail;
       }
     }
     if(n->first_child!=MULIB_MKIR_NONE){
       if(!mulib_mkir_tree_literal_(&b," "))goto fail;
       current=n->first_child;continue;
     }
   }
   if(!bare&&!mulib_mkir_tree_literal_(&b,")"))goto fail;
   if(current==root)break;
   if(n->next!=MULIB_MKIR_NONE){if(!mulib_mkir_tree_literal_(&b," "))goto fail;current=n->next;entering=1;}
   else {current=n->parent;entering=0;}
 }
 free(index);return b.p;
 fail:free(index);free(b.p);return NULL;
}
#ifdef __cplusplus
}
#endif
#endif
#endif
