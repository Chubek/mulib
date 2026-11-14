/* Only compiled when JSON kernel APIs are used. Implementation macros belong
 * in the generated on1x_mulib_impl.c, never here. */
#include "on1x_runtime.h"
#include <mulib-json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
static void json_error(const char *detail){fprintf(stderr,"On1x JSON error: %s\n",detail);exit(71);}
on1x_json *on1x_json_parse_or_die(const char *text){
 mulib_json_error_t error;mulib_json_t *doc;
 if(!text)json_error("NULL input");
 doc=mulib_json_parse(text,strlen(text),&error);
 if(!doc){fprintf(stderr,"On1x JSON parse failure at byte %zu: %s\n",error.offset,error.message?error.message:"unknown");exit(71);}
 return doc;
}
const on1x_json *on1x_json_get_or_die(const on1x_json *root,const char *key){
 const mulib_json_t *value;
 if(!root||!key)json_error("invalid lookup");
 value=mulib_json_get(root,key);
 if(!value)json_error("missing member");
 return value;
}
int64_t on1x_json_size(const on1x_json *v){
 size_t n=mulib_json_size(v);
 if((uintmax_t)n>(uintmax_t)INT64_MAX)json_error("size overflow");
 return (int64_t)n;
}
int64_t on1x_json_number(const on1x_json *v){
 double n;if(!v||mulib_json_typeof(v)!=MULIB_JSON_NUMBER)json_error("not a number");
 n=mulib_json_numeric(v);
 if(!isfinite(n)||n<-0x1p63||n>=0x1p63||floor(n)!=n)json_error("number outside exact i64 value range");
 return (int64_t)n;
}
void on1x_json_print(const on1x_json *v){
 char *text=mulib_json_stringify(v,0);
 if(!text)json_error("stringification failed");
 puts(text);free(text);
}
void on1x_json_drop(on1x_json *v){mulib_json_destroy(v);}
