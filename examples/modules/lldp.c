#define MULIB_IMPL_LLDP
#include "mulib-lldp.h"
#include <assert.h>
static int visit(const mulib_lldp_tlv_t *tlv, void *ctx) { (void)ctx; assert(tlv->type == 0); return 0; }
int main(void) { const uint8_t end[] = {0,0}; assert(!mulib_lldp_foreach(end, sizeof end, visit, NULL)); return 0; }
