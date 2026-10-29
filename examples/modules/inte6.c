#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#include "mulib-inte6.h"
#include <assert.h>
#include <string.h>
int main(void) {
    mulib_inet6_t address; char text[40];
    assert(mulib_inet6_parse("2001:DB8::1",11,&address));
    assert(mulib_inet6_format(&address,text,sizeof(text)) && !strcmp(text,"2001:db8::1"));
    return 0;
}
