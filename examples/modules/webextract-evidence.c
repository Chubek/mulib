/* Complete, offline webextract-evidence example. */
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_WEBEXTRACT_EVIDENCE
#include "mulib-webextract-evidence.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_webextract_evidence_t evidence;
  assert (mulib_webextract_evidence_make ("price=12", 8, 6, 2, "price",
                                          &evidence));
  assert (mulib_webextract_evidence_verify ("price=12", 8, &evidence));
  return 0;
}
