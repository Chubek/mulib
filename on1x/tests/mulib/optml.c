#include "mulib-optml.h"
#include <stdint.h>
#include <string.h>
int64_t fixture_optml_count(const char *source) {
  mulib_optml_rules_t rules={0};size_t offset=0;
  mulib_mir_status_t status=mulib_optml_parse(source,strlen(source),65536,&rules,&offset);
  int64_t count=status==MULIB_MIR_OK?(int64_t)rules.count:-1;
  mulib_optml_rules_free(&rules);
  return count;
}
