#include "contract_strings.h"
#include <string.h>
#include <stdlib.h>
long long contract_text_size(const char *text) { return (long long)strlen(text); }
const char *contract_static_text(int which) {
  if (which == 0) return "";
  if (which == 2) return "line\nnext";
  return "static text";
}
void contract_text_note(const char *first, const char *second) {
  if (strcmp(first, second)) abort();
}
