/* Private shared Mune/XML bridge. Implementations include their dependencies.
 * No implementation switches are defined here. Not a public module. */
#ifndef MULIB_MUNE_MARKUP_SUPPORT_
#define MULIB_MUNE_MARKUP_SUPPORT_
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static inline int mulib_munexml_get_(mulib_mune_state_t *s,mulib_mune_value_t t,const char *name,mulib_mune_value_t *v){mulib_mune_value_t k;return !mulib_mune_make_string(s,name,strlen(name),&k)&&!mulib_mune_raw_get(s,t,k,v);}
static inline int mulib_munexml_put_(mulib_mune_state_t *s,mulib_mune_value_t t,const char *name,mulib_mune_value_t v){mulib_mune_value_t k;return !mulib_mune_make_string(s,name,strlen(name),&k)&&!mulib_mune_raw_set(s,t,k,v);}
static inline int mulib_munexml_puts_(mulib_mune_state_t *s,mulib_mune_value_t t,const char *key,const char *text){mulib_mune_value_t v;return !mulib_mune_make_string(s,text,strlen(text),&v)&&mulib_munexml_put_(s,t,key,v);}
static inline const char *mulib_munexml_string_(mulib_mune_value_t v){size_t n;const char *s=mulib_mune_value_string(v,&n);return s&&!memchr(s,0,n)&&mulib_xml_valid_text(s,n)?s:NULL;}
static inline int mulib_munexml_number_(char out[128],double v){
    int n;const char *decimal;char *at;
    if(!isfinite(v)||fabs(v)>1e100)return 0;
    n=snprintf(out,128,"%.17g",v);if(n<0||n>=128)return 0;
    decimal=localeconv()->decimal_point;at=decimal&&*decimal?strstr(out,decimal):NULL;
    if(at&&strcmp(decimal,".")){size_t len=strlen(decimal);*at='.';memmove(at+1,at+len,strlen(at+len)+1);}return 1;
}
static inline mulib_mune_status_t mulib_munexml_number_native_(mulib_mune_state_t *s,const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *r,void *u){
    char text[128];mulib_mune_value_t v;(void)u;
    if(n!=1||(a[0].type!=MULIB_MUNE_INTEGER&&a[0].type!=MULIB_MUNE_NUMBER)||!mulib_munexml_number_(text,a[0].type==MULIB_MUNE_INTEGER?(double)a[0].as.integer:a[0].as.number))return MULIB_MUNE_INVALID;
    return !mulib_mune_make_string(s,text,strlen(text),&v)&&mulib_mune_result_push(r,v)?MULIB_MUNE_OK:MULIB_MUNE_MEMORY;
}
static inline mulib_mune_status_t mulib_munexml_require_(mulib_mune_state_t *s,const mulib_mune_value_t *a,size_t n,mulib_mune_result_t *r,void *u){
    const char *name=n==1?mulib_munexml_string_(a[0]):NULL;mulib_mune_value_t v;
    if(!name||strcmp(name,(const char *)u)||mulib_mune_get_global(s,name,&v))return MULIB_MUNE_NAME;
    return mulib_mune_result_push(r,v)?MULIB_MUNE_OK:MULIB_MUNE_MEMORY;
}
static inline mulib_mune_state_t *mulib_munexml_state_(const mulib_mune_runtime_options_t *options){
    mulib_mune_state_t *s=mulib_mune_state_create(options);size_t i;mulib_mune_value_t null;
    static const char *const disabled[]={"io","os","debug","package","load","loadfile","dofile","require","print","warn"};
    if(!s)return NULL;
    for(i=0;i<sizeof disabled/sizeof disabled[0];++i)if(mulib_mune_set_global(s,disabled[i],mulib_mune_nil()))goto fail;
    if(mulib_mune_make_table(s,&null)||mulib_mune_set_global(s,"__mulib_null",null)||mulib_mune_bind(s,"__mulib_number",mulib_munexml_number_native_,NULL))goto fail;
    return s;
fail:mulib_mune_state_destroy(s);return NULL;
}
static inline int mulib_munexml_bootstrap_(mulib_mune_state_t *s,const char *const *parts,size_t count,const char *name,mulib_mune_error_t *error){
    mulib_strbuf_t b={0};mulib_mune_result_t *r=NULL;size_t i;int ok=0;
    for(i=0;i<count;++i)if(!mulib_strbuf_append(&b,parts[i]))goto done;
    if(mulib_mune_run_source(s,b.data,b.length,&r,error))goto done;
    if(mulib_mune_set_global(s,name,mulib_mune_result_get(r,0))||mulib_mune_bind(s,"require",mulib_munexml_require_,(void *)name))goto done;
    ok=1;
done:mulib_strbuf_free(&b);mulib_mune_result_destroy(r);return ok;
}
static inline int mulib_munexml_json_(mulib_mune_state_t *s,const mulib_json_t *j,mulib_mune_value_t *out,unsigned depth,size_t *budget){
    mulib_mune_value_t table,k,v;size_t i,n;const char *text;
    if(!*budget||depth>64)return 0;
    --*budget;
    switch(mulib_json_typeof(j)){
    case MULIB_JSON_NULL:return !mulib_mune_get_global(s,"__mulib_null",out);
    case MULIB_JSON_BOOL:*out=mulib_mune_boolean(mulib_json_boolean(j));return 1;
    case MULIB_JSON_NUMBER:*out=mulib_mune_number(mulib_json_numeric(j));return 1;
    case MULIB_JSON_STRING:text=mulib_json_text(j,&n);return n<=1024*1024&&!mulib_mune_make_string(s,text,n,out);
    case MULIB_JSON_ARRAY:case MULIB_JSON_OBJECT:
        if(mulib_mune_make_table(s,&table))return 0;
        for(i=0;i<mulib_json_size(j);++i){if(mulib_json_typeof(j)==MULIB_JSON_ARRAY)k=mulib_mune_integer((int64_t)i+1);else{text=mulib_json_key(j,i,&n);if(mulib_mune_make_string(s,text,n,&k))return 0;}
            if(!mulib_munexml_json_(s,mulib_json_at(j,i),&v,depth+1,budget)||mulib_mune_raw_set(s,table,k,v))return 0;}
        *out=table;return 1;
    }return 0;
}
static inline int mulib_munexml_array_(mulib_mune_state_t *s,mulib_mune_value_t t,size_t *out,size_t limit){size_t cursor=0,count=0,max=0;mulib_mune_value_t k,v;int next;
    if(t.type!=MULIB_MUNE_TABLE)return 0;
    while((next=mulib_mune_table_next(s,t,&cursor,&k,&v))==1){if(++count>limit||k.type!=MULIB_MUNE_INTEGER||k.as.integer<1||(uint64_t)k.as.integer>limit)return 0;if((size_t)k.as.integer>max)max=(size_t)k.as.integer;}
    if(next<0||count!=max)return 0;
    *out=count;return 1;
}
typedef struct mulib_munexml_budget_ {size_t nodes,bytes;} mulib_munexml_budget_;
static inline int mulib_munexml_bytes_(mulib_munexml_budget_ *b,const char *s){size_t n=strlen(s);if(n>b->bytes)return 0;b->bytes-=n;return 1;}
static inline int mulib_munexml_svg_tag_(const char *tag){
    static const char *const names[]={"svg","g","defs","path","rect","circle","ellipse","line","polyline","polygon","text","tspan","title","desc","clipPath","linearGradient","radialGradient","stop","pattern","mask","symbol","use","font","font-face","glyph","missing-glyph"};
    size_t i;for(i=0;i<sizeof names/sizeof names[0];++i)if(!strcmp(tag,names[i]))return 1;return 0;
}
static inline mulib_xml_node_t *mulib_munexml_from_(mulib_mune_state_t *s,mulib_mune_value_t t,unsigned depth,mulib_munexml_budget_ *budget){
    mulib_mune_value_t v,attrs,k,list;const char *tag,*key,*value;mulib_xml_node_t *node=NULL,*child;size_t cursor=0,count=0,i;char number[128];int next;
    if(depth>64||!budget->nodes)return NULL;
    --budget->nodes;
    if(t.type==MULIB_MUNE_STRING){value=mulib_munexml_string_(t);return value&&mulib_munexml_bytes_(budget,value)?mulib_xml_new(MULIB_XML_TEXT,value):NULL;}
    if(!mulib_munexml_get_(s,t,"tag",&v)||!(tag=mulib_munexml_string_(v))||!mulib_munexml_svg_tag_(tag)||!mulib_munexml_bytes_(budget,tag))return NULL;
    node=mulib_xml_new(MULIB_XML_ELEMENT,tag);if(!node)return NULL;
    if(!mulib_munexml_get_(s,t,"attrs",&attrs))goto fail;
    if(attrs.type!=MULIB_MUNE_NIL){if(attrs.type!=MULIB_MUNE_TABLE)goto fail;
        while((next=mulib_mune_table_next(s,attrs,&cursor,&k,&v))==1){
            if(++count>64||!(key=mulib_munexml_string_(k))||!mulib_xml_valid_name(key)||!strncmp(key,"on",2))goto fail;
            if(strchr(key,':')&&strcmp(key,"xlink:href")&&strcmp(key,"xmlns:xlink"))goto fail;
            if(v.type==MULIB_MUNE_INTEGER||v.type==MULIB_MUNE_NUMBER){if(!mulib_munexml_number_(number,v.type==MULIB_MUNE_INTEGER?(double)v.as.integer:v.as.number))goto fail;value=number;}
            else value=mulib_munexml_string_(v);
            if(value&&((!strcmp(key,"xmlns:xlink")&&strcmp(value,"http://www.w3.org/1999/xlink"))||(!strcmp(key,"xmlns")&&strcmp(value,"http://www.w3.org/2000/svg"))))goto fail;
            if(!value||((!strcmp(key,"href")||!strcmp(key,"xlink:href"))&&*value!='#')||!mulib_munexml_bytes_(budget,key)||!mulib_munexml_bytes_(budget,value)||!mulib_xml_set_attr(node,key,value))goto fail;
        }if(next<0)goto fail;
    }
    if(!mulib_munexml_get_(s,t,"children",&list))goto fail;
    if(list.type!=MULIB_MUNE_NIL){if(!mulib_munexml_array_(s,list,&count,100000))goto fail;for(i=1;i<=count;++i){if(mulib_mune_raw_get(s,list,mulib_mune_integer((int64_t)i),&v))goto fail;child=mulib_munexml_from_(s,v,depth+1,budget);if(!child)goto fail;if(!mulib_xml_append(node,child)){mulib_xml_destroy(child);goto fail;}}}
    return node;
fail:mulib_xml_destroy(node);return NULL;
}
static inline mulib_xml_node_t *mulib_munexml_svg_(mulib_mune_state_t *s,mulib_mune_value_t value){mulib_munexml_budget_ b={100000,8*1024*1024};mulib_xml_node_t *n=mulib_munexml_from_(s,value,0,&b);const char *ns=mulib_xml_attr(n,"xmlns");if(!n||!mulib_xml_name(n)||strcmp(mulib_xml_name(n),"svg")||(ns&&strcmp(ns,"http://www.w3.org/2000/svg"))||!mulib_xml_set_attr(n,"xmlns","http://www.w3.org/2000/svg")||!mulib_xml_set_attr(n,"xmlns:xlink","http://www.w3.org/1999/xlink")){mulib_xml_destroy(n);return NULL;}return n;}
#endif
