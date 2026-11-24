/* Private ownership records preserve the public, legacy node layout. */
#ifndef MULIB_DOCIR_CALLOC
#define MULIB_DOCIR_CALLOC calloc
#endif
#ifndef MULIB_DOCIR_REALLOC
#define MULIB_DOCIR_REALLOC realloc
#endif
#ifndef MULIB_DOCIR_FREE
#define MULIB_DOCIR_FREE free
#endif
#if defined(MULIB_DOCIR_LOCK) != defined(MULIB_DOCIR_UNLOCK)
#error "Define both MULIB_DOCIR_LOCK and MULIB_DOCIR_UNLOCK"
#endif
#ifndef MULIB_DOCIR_LOCK
#if !defined(__STDC_NO_ATOMICS__) && !defined(_MSC_VER)
#include <stdatomic.h>
static atomic_flag mulib_docir_lock_=ATOMIC_FLAG_INIT;
#define MULIB_DOCIR_LOCK() while(atomic_flag_test_and_set_explicit(&mulib_docir_lock_,memory_order_acquire)){}
#define MULIB_DOCIR_UNLOCK() atomic_flag_clear_explicit(&mulib_docir_lock_,memory_order_release)
#else
/* Supply lock hooks for concurrent use on implementations without C11 atomics. */
#define MULIB_DOCIR_LOCK() ((void)0)
#define MULIB_DOCIR_UNLOCK() ((void)0)
#endif
#endif
typedef struct mulib_docir_owned_ {
    mulib_docir_node_t node;
    struct mulib_docir_owned_ *hash_next,*parent,*first,*last,*next,*previous;
    char *text,*id,*target,*source;
    mulib_docir_node_t **children;
    size_t count,capacity,text_size,text_capacity,order;
    unsigned kind,level;mulib_docir_style_t style;
    size_t begin,end,line,column;
} mulib_docir_owned_;
static mulib_docir_owned_ *mulib_docir_registry_[1024];
static size_t mulib_docir_hash_(const void *p) {
    uintptr_t v=(uintptr_t)p;return (size_t)(((v>>4)^(v>>14))&1023u);
}
static mulib_docir_owned_ *mulib_docir_find_(const mulib_docir_node_t *n) {
    for(mulib_docir_owned_ *m=mulib_docir_registry_[mulib_docir_hash_(n)];m;m=m->hash_next)
        if(&m->node==n)return m;
    return NULL;
}
static int mulib_docir_report_(mulib_docir_report_t *r,unsigned code,size_t node) {
    if(r){r->code=code;r->node=node;}return code==mulib_docir_OK;
}
static int mulib_docir_text_valid_(const char *text,size_t length) {
    if((!text&&length)||length>64u*1024u*1024u)return 0;
    size_t at=0;uint32_t c;
    while(at<length) {
        if(!mulib_typecore_utf8((const unsigned char*)text,length,&at,&c)||
           (c<32&&c!=9&&c!=10&&c!=13)||c==0xfffeu||c==0xffffu)return 0;
    }
    return 1;
}
static char *mulib_docir_copy_(const char *text) {
    if(!text)return NULL;
    size_t n=strlen(text);
    if(n==SIZE_MAX||!mulib_docir_text_valid_(text,n))return NULL;
    char *copy=(char*)MULIB_DOCIR_CALLOC(n+1,1);
    if(copy)memcpy(copy,text,n+1);
    return copy;
}
static int mulib_docir_id_(const char *s) {
    if(!s||!*s||strlen(s)>255)return 0;
    for(size_t i=0;s[i];i++) {
        unsigned char c=(unsigned char)s[i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'||
             (i&&((c>='0'&&c<='9')||c=='-'||c=='.'||c==':'))))return 0;
    }
    return 1;
}
static int mulib_docir_uri_(const char *s) {
    if(!s||!*s||!mulib_docir_text_valid_(s,strlen(s)))return 0;
    if(*s=='#')return mulib_docir_id_(s+1);
    if(s[0]=='/'&&s[1]=='/')return 0;
    for(const unsigned char *p=(const unsigned char*)s;*p;p++)if(*p<=32||*p==127||*p=='\\')return 0;
    size_t first=strcspn(s,"/?#");const char *colon=(const char*)memchr(s,':',first);
    if(colon) {
        size_t n=(size_t)(colon-s);char scheme[8];
        if(n>=sizeof(scheme))return 0;
        for(size_t i=0;i<n;i++)scheme[i]=(char)(s[i]>='A'&&s[i]<='Z'?s[i]+32:s[i]);
        scheme[n]=0;
        if(strcmp(scheme,"http")&&strcmp(scheme,"https")&&strcmp(scheme,"mailto"))return 0;
        if(!colon[1])return 0;
    }
    return 1;
}
static int mulib_docir_style_valid_(mulib_docir_style_t style) {
    return !(style.flags&~7u)&&style.font_size_sp>=0&&style.font_size_sp<=134217728&&
           style.indent_sp>=-67108864&&style.indent_sp<=67108864;
}
static int mulib_docir_basic_(const mulib_docir_owned_ *m) {
    return m&&m->kind<=mulib_docir_CODE&&m->node.kind==m->kind&&m->node.text==m->text&&
           m->node.children==m->children&&m->node.count==m->count&&m->node.capacity==m->capacity&&
           (!m->text||(m->text_size<m->text_capacity&&m->text[m->text_size]==0));
}
static int mulib_docir_limits_(const mulib_docir_limits_t *in,mulib_docir_limits_t *out) {
    *out=in?*in:(mulib_docir_limits_t){0,0,0};
    if(!out->max_nodes)out->max_nodes=100000;
    if(!out->max_text_bytes)out->max_text_bytes=16u*1024u*1024u;
    if(!out->max_depth)out->max_depth=256;
    return out->max_nodes<=1000000&&out->max_text_bytes<=64u*1024u*1024u&&out->max_depth<=4096;
}
mulib_docir_node_t *mulib_docir_new(unsigned kind,const char *text) {
    if(kind>mulib_docir_CODE)return NULL;
    mulib_docir_owned_ *m=(mulib_docir_owned_*)MULIB_DOCIR_CALLOC(1,sizeof(*m));
    if(!m)return NULL;
    if(text&&!(m->text=mulib_docir_copy_(text))){MULIB_DOCIR_FREE(m);return NULL;}
    m->node.kind=m->kind=kind;m->node.text=m->text;m->level=1;
    if(text){m->text_size=strlen(text);m->text_capacity=m->text_size+1;}
    MULIB_DOCIR_LOCK();size_t bucket=mulib_docir_hash_(&m->node);
    m->hash_next=mulib_docir_registry_[bucket];mulib_docir_registry_[bucket]=m;MULIB_DOCIR_UNLOCK();
    return &m->node;
}
int mulib_docir_append(mulib_docir_node_t *parent,mulib_docir_node_t *child) {
    int ok=0;MULIB_DOCIR_LOCK();
    mulib_docir_owned_ *p=mulib_docir_find_(parent),*c=mulib_docir_find_(child);
    if(!mulib_docir_basic_(p)||!mulib_docir_basic_(c)||c->parent||c==p||c->kind==mulib_docir_ROOT)goto done;
    size_t depth=0;
    for(mulib_docir_owned_ *q=p;q;q=q->parent)if(q==c||++depth>1000000)goto done;
    if(p->count>=1000000)goto done;
    if(p->count==p->capacity) {
        if(p->capacity>SIZE_MAX/2)goto done;
        size_t capacity=p->capacity?p->capacity*2:4;
        if(capacity>1000000)capacity=1000000;
        if(capacity>SIZE_MAX/sizeof(*p->children))goto done;
        void *memory=MULIB_DOCIR_REALLOC(p->children,capacity*sizeof(*p->children));
        if(!memory)goto done;
        p->children=(mulib_docir_node_t**)memory;p->capacity=capacity;
        p->node.children=p->children;p->node.capacity=capacity;
    }
    c->parent=p;c->previous=p->last;
    if(p->last)p->last->next=c;else p->first=c;
    p->last=c;p->children[p->count++]=child;p->node.count=p->count;ok=1;
done:MULIB_DOCIR_UNLOCK();return ok;
}
static void mulib_docir_unlink_(mulib_docir_owned_ *m) {
    mulib_docir_owned_ *p=m->parent;
    if(!p)return;
    if(m->previous)m->previous->next=m->next;else p->first=m->next;
    if(m->next)m->next->previous=m->previous;else p->last=m->previous;
    p->count--;size_t i=0;
    for(mulib_docir_owned_ *c=p->first;c;c=c->next)p->children[i++]=&c->node;
    p->node.children=p->children;p->node.capacity=p->capacity;p->node.count=p->count;
    m->parent=m->next=m->previous=NULL;
}
mulib_docir_node_t *mulib_docir_detach(mulib_docir_node_t *parent,size_t index) {
    mulib_docir_node_t *result=NULL;MULIB_DOCIR_LOCK();mulib_docir_owned_ *p=mulib_docir_find_(parent);
    if(mulib_docir_basic_(p)&&index<p->count) {
        mulib_docir_owned_ *c=p->first;for(size_t i=0;i<index;i++)c=c->next;
        mulib_docir_unlink_(c);result=&c->node;
    }
    MULIB_DOCIR_UNLOCK();return result;
}
const mulib_docir_node_t *mulib_docir_parent(const mulib_docir_node_t *node) {
    const mulib_docir_node_t *result=NULL;MULIB_DOCIR_LOCK();mulib_docir_owned_ *m=mulib_docir_find_(node);
    if(m&&m->parent)result=&m->parent->node;
    MULIB_DOCIR_UNLOCK();return result;
}
const mulib_docir_node_t *mulib_docir_owner(const mulib_docir_node_t *node) {
    const mulib_docir_node_t *result=NULL;MULIB_DOCIR_LOCK();mulib_docir_owned_ *m=mulib_docir_find_(node);
    if(m){while(m->parent)m=m->parent;result=&m->node;}
    MULIB_DOCIR_UNLOCK();return result;
}
int mulib_docir_set_text(mulib_docir_node_t *node,const char *text) {
    char *copy=text?mulib_docir_copy_(text):NULL;int ok=0;
    if(text&&!copy)return 0;
    MULIB_DOCIR_LOCK();mulib_docir_owned_ *m=mulib_docir_find_(node);
    if(mulib_docir_basic_(m)) {
        MULIB_DOCIR_FREE(m->text);m->text=copy;m->node.text=copy;
        m->text_size=copy?strlen(copy):0;m->text_capacity=copy?m->text_size+1:0;copy=NULL;ok=1;
    }
    MULIB_DOCIR_UNLOCK();MULIB_DOCIR_FREE(copy);return ok;
}
int mulib_docir_text_append(mulib_docir_node_t *node,const char *text,size_t length) {
    if(!mulib_docir_text_valid_(text,length))return 0;
    int ok=0;MULIB_DOCIR_LOCK();mulib_docir_owned_ *m=mulib_docir_find_(node);
    if(!mulib_docir_basic_(m)||length>64u*1024u*1024u-m->text_size)goto done;
    size_t need=m->text_size+length+1,offset=0;int aliased=0;
    if(length&&m->text) {
        uintptr_t p=(uintptr_t)text,base=(uintptr_t)m->text;
        if(p>=base&&p-base<m->text_capacity) {
            offset=(size_t)(p-base);if(offset>m->text_size||length>m->text_size-offset)goto done;aliased=1;
        }
    }
    if(need>m->text_capacity) {
        size_t capacity=m->text_capacity?m->text_capacity:32;
        while(capacity<need){if(capacity>(64u*1024u*1024u+1)/2){capacity=64u*1024u*1024u+1;break;}capacity*=2;}
        char *p=(char*)MULIB_DOCIR_REALLOC(m->text,capacity);if(!p)goto done;
        m->text=p;m->text_capacity=capacity;m->node.text=p;
    }
    if(length)memmove(m->text+m->text_size,aliased?m->text+offset:text,length);
    m->text_size+=length;m->text[m->text_size]=0;ok=1;
done:MULIB_DOCIR_UNLOCK();return ok;
}
static int mulib_docir_attribute_(mulib_docir_node_t *node,const char *value,unsigned which) {
    if(value&&((which==0&&!mulib_docir_id_(value))||(which==1&&!mulib_docir_uri_(value))))return 0;
    char *copy=value?mulib_docir_copy_(value):NULL;int ok=0;
    if(value&&!copy)return 0;
    MULIB_DOCIR_LOCK();mulib_docir_owned_ *m=mulib_docir_find_(node);
    if(mulib_docir_basic_(m)&&(which!=1||m->kind==mulib_docir_LINK||m->kind==mulib_docir_REFERENCE||m->kind==mulib_docir_CITE)) {
        char **slot=which==0?&m->id:&m->target;MULIB_DOCIR_FREE(*slot);*slot=copy;copy=NULL;ok=1;
    }
    MULIB_DOCIR_UNLOCK();MULIB_DOCIR_FREE(copy);return ok;
}
int mulib_docir_set_id(mulib_docir_node_t *n,const char *id){return mulib_docir_attribute_(n,id,0);}
int mulib_docir_set_target(mulib_docir_node_t *n,const char *uri){return mulib_docir_attribute_(n,uri,1);}
int mulib_docir_set_style(mulib_docir_node_t *node,const mulib_docir_style_t *style) {
    if(!style||!mulib_docir_style_valid_(*style))return 0;
    int ok=0;MULIB_DOCIR_LOCK();mulib_docir_owned_ *m=mulib_docir_find_(node);
    if(mulib_docir_basic_(m)){m->style=*style;ok=1;}
    MULIB_DOCIR_UNLOCK();return ok;
}
int mulib_docir_set_level(mulib_docir_node_t *node,unsigned level) {
    if(level<1||level>6)return 0;
    int ok=0;MULIB_DOCIR_LOCK();mulib_docir_owned_ *m=mulib_docir_find_(node);
    if(mulib_docir_basic_(m)&&(m->kind==mulib_docir_HEADING||m->kind==mulib_docir_SECTION)){m->level=level;ok=1;}
    MULIB_DOCIR_UNLOCK();return ok;
}
int mulib_docir_set_source(mulib_docir_node_t *node,const char *file,size_t begin,size_t end,size_t line,size_t column) {
    if(begin>end||(file&&(!*file||!line||!column))||(!file&&(begin||end||line||column)))return 0;
    char *copy=file?mulib_docir_copy_(file):NULL;int ok=0;if(file&&!copy)return 0;
    MULIB_DOCIR_LOCK();mulib_docir_owned_ *m=mulib_docir_find_(node);
    if(mulib_docir_basic_(m)) {
        MULIB_DOCIR_FREE(m->source);m->source=copy;copy=NULL;
        m->begin=begin;m->end=end;m->line=line;m->column=column;ok=1;
    }
    MULIB_DOCIR_UNLOCK();MULIB_DOCIR_FREE(copy);return ok;
}
void mulib_docir_destroy(mulib_docir_node_t *node) {
    MULIB_DOCIR_LOCK();mulib_docir_owned_ *root=mulib_docir_find_(node);
    if(!root){MULIB_DOCIR_UNLOCK();return;}
    mulib_docir_unlink_(root);mulib_docir_owned_ *m=root;
    /* Destruction follows private ownership links, never mutable public edges. */
    while(m) {
        if(m->first){m=m->first;continue;}
        mulib_docir_owned_ *parent=m->parent;
        if(parent) {
            parent->first=m->next;if(m->next)m->next->previous=NULL;else parent->last=NULL;
        }
        mulib_docir_owned_ **slot=&mulib_docir_registry_[mulib_docir_hash_(&m->node)];
        while(*slot&&*slot!=m)slot=&(*slot)->hash_next;
        if(*slot)*slot=m->hash_next;
        MULIB_DOCIR_FREE(m->text);MULIB_DOCIR_FREE(m->id);MULIB_DOCIR_FREE(m->target);
        MULIB_DOCIR_FREE(m->source);MULIB_DOCIR_FREE(m->children);MULIB_DOCIR_FREE(m);
        m=parent;
    }
    MULIB_DOCIR_UNLOCK();
}
const char *mulib_docir_snapshot_string(const mulib_docir_snapshot_t *s,size_t offset) {
    if(!s||!s->strings.data||offset>=s->strings.size||
       !memchr(s->strings.data+offset,0,s->strings.size-offset))return NULL;
    return (const char*)s->strings.data+offset;
}
void mulib_docir_snapshot_destroy(mulib_docir_snapshot_t *s) {
    if(s){MULIB_DOCIR_FREE(s->records);mulib_typecore_free(&s->strings);memset(s,0,sizeof(*s));}
}
static int mulib_docir_snapshot_copy_(mulib_docir_snapshot_t *s,const char *text,size_t limit,size_t *offset,unsigned *error) {
    *offset=SIZE_MAX;if(!text)return 1;
    size_t length=strlen(text);
    if(s->strings.size>limit||length>=limit-s->strings.size){*error=mulib_docir_E_LIMIT;return 0;}
    *offset=s->strings.size;
    if(!mulib_typecore_append(&s->strings,text,length+1)){*error=mulib_docir_E_MEMORY;return 0;}
    return 1;
}
int mulib_docir_snapshot_make(const mulib_docir_node_t *node,const mulib_docir_limits_t *limits,
                            mulib_docir_snapshot_t *out,mulib_docir_report_t *report) {
    mulib_docir_limits_t budget;mulib_docir_snapshot_t result={0};size_t capacity=0,depth=0;
    unsigned error=mulib_docir_E_OWNERSHIP;
    if(!out||out->records||out->count||out->strings.data||out->strings.size||out->strings.capacity||
       !mulib_docir_limits_(limits,&budget))return mulib_docir_report_(report,mulib_docir_E_ARGUMENT,0);
    MULIB_DOCIR_LOCK();mulib_docir_owned_ *root=mulib_docir_find_(node),*m=root;
    if(!root)goto fail;
    while(m) {
        if(!mulib_docir_basic_(m))goto fail;
        if(m->text&&(memchr(m->text,0,m->text_size)||!mulib_docir_text_valid_(m->text,m->text_size))) {
            error=mulib_docir_E_TEXT;goto fail;
        }
        if(result.count>=budget.max_nodes||depth>budget.max_depth){error=mulib_docir_E_LIMIT;goto fail;}
        size_t children=0;
        for(mulib_docir_owned_ *c=m->first;c;c=c->next) {
            if(children>=m->count||c->parent!=m||m->children[children]!=&c->node)goto fail;
            children++;
        }
        if(children!=m->count)goto fail;
        if(result.count==capacity) {
            size_t next=capacity?capacity*2:16;if(next>budget.max_nodes)next=budget.max_nodes;
            if(next>SIZE_MAX/sizeof(*result.records)){error=mulib_docir_E_LIMIT;goto fail;}
            void *memory=MULIB_DOCIR_REALLOC(result.records,next*sizeof(*result.records));
            if(!memory){error=mulib_docir_E_MEMORY;goto fail;}
            result.records=(mulib_docir_record_t*)memory;capacity=next;
        }
        mulib_docir_record_t r={0};r.kind=m->kind;r.level=m->level;r.depth=depth;r.child_count=m->count;
        r.parent=m==root?SIZE_MAX:m->parent->order;r.style=m->style;
        r.source_begin=m->begin;r.source_end=m->end;r.source_line=m->line;r.source_column=m->column;
        if(!mulib_docir_snapshot_copy_(&result,m->text,budget.max_text_bytes,&r.text,&error)||
           !mulib_docir_snapshot_copy_(&result,m->id,budget.max_text_bytes,&r.id,&error)||
           !mulib_docir_snapshot_copy_(&result,m->target,budget.max_text_bytes,&r.target,&error)||
           !mulib_docir_snapshot_copy_(&result,m->source,budget.max_text_bytes,&r.source,&error)) {
            goto fail;
        }
        m->order=result.count;result.records[result.count++]=r;
        if(m->first){m=m->first;depth++;continue;}
        while(m!=root&&!m->next){m=m->parent;depth--;}
        if(m==root)break;
        m=m->next;
    }
    MULIB_DOCIR_UNLOCK();*out=result;return mulib_docir_report_(report,mulib_docir_OK,SIZE_MAX);
fail:
    MULIB_DOCIR_UNLOCK();size_t at=result.count;mulib_docir_snapshot_destroy(&result);
    return mulib_docir_report_(report,error,at);
}
