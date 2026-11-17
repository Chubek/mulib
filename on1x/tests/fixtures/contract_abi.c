#include "contract_abi.h"
static int noted;
int contract_add(int a, int b) { return a + b; }
fixture_short contract_narrow(fixture_short x) { return x; }
void contract_note(int x) { noted = x; }
int contract_count(void) { return noted; }
int contract_io(int x) { return x; } /* Metadata round-trip test, not inferred purity. */
fixture_bool contract_bool(fixture_bool x) { return x; }
fixture_double contract_double(fixture_double x) { return x * 2.0; }
fixture_double contract_mix(fixture_bool flag, fixture_double x, int n) { return flag ? x + n : -x; }
fixture_float contract_float(fixture_float x) { return x * 2.0f; }
fixture_u8 contract_u8(fixture_u8 x) { return x; }
fixture_u16 contract_u16(fixture_u16 x) { return x; }
fixture_u64 contract_u64(fixture_u64 x) { return x; }
unsigned int contract_unsigned(unsigned int x) { return x; }
