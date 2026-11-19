#ifndef MULIB_JSON_H
#define MULIB_JSON_H
#include <stddef.h>
typedef enum mulib_json_type {MULIB_JSON_NULL,MULIB_JSON_BOOL,MULIB_JSON_NUMBER,
MULIB_JSON_STRING,MULIB_JSON_ARRAY,MULIB_JSON_OBJECT} mulib_json_type;
typedef struct mulib_json mulib_json_t;
mulib_json_t *mulib_json_new(mulib_json_type);
mulib_json_t *mulib_json_string(const char*);
void mulib_json_destroy(mulib_json_t*);
mulib_json_type mulib_json_typeof(const mulib_json_t*);
size_t mulib_json_size(const mulib_json_t*);
const char *mulib_json_text(const mulib_json_t*,size_t*);
double mulib_json_numeric(const mulib_json_t*);
mulib_json_t *mulib_json_at(const mulib_json_t*,size_t);
mulib_json_t *mulib_json_get(const mulib_json_t*,const char*);
int mulib_json_set(mulib_json_t*,const char*,mulib_json_t*);
mulib_json_t *mulib_json_parse(const char*,size_t,void*);
char *mulib_json_stringify(const mulib_json_t*,int);
#endif
