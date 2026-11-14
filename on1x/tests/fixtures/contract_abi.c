#include "contract_abi.h"
static int noted;
int contract_add(int a, int b) { return a + b; }
fixture_short contract_narrow(fixture_short x) { return x; }
void contract_note(int x) { noted = x; }
int contract_count(void) { return noted; }
int contract_io(int x) { return x; } /* Metadata round-trip test, not inferred purity. */
