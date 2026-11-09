/* No probe is transmitted without a valid destination/TTL. */
#define MULIB_IMPL_TRACEPATH
#include "mulib-tracepath.h"
#include <assert.h>
int main(void) { uint8_t ip[4] = {127,0,0,1}; int type, code; assert(mulib_tracepath_probe(ip, 33434, 0, 100, &type, &code) == -EINVAL); return 0; }
