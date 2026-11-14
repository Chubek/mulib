#include <stdint.h>
#include <stdbool.h>
#include <string.h>
int64_t flattened(int64_t x, bool enabled, const char *text, int64_t extra) {
    return enabled && strcmp(text, "ok") == 0 ? x + extra : 0;
}
