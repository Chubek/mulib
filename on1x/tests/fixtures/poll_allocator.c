#include <stdlib.h>
#include <stddef.h>
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t count,size_t bytes){
 static unsigned long calls;
 const char *text=getenv("ON1X_POLL_FAIL_AT");
 unsigned long fail=text?strtoul(text,NULL,10):0;
 if(++calls==fail)return NULL;
 return __real_calloc(count,bytes);
}
