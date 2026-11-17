/* Deliberately raw, exact flattened scalar C ABI, not safe foreign admission. */
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
int64_t defer_foreign(int64_t x, bool enabled, const char *text, int64_t extra) {
    int64_t result = enabled && strcmp(text, "ok") == 0 ? x + extra : 0;
    printf("%" PRId64 "\n", result);
    return result;
}
