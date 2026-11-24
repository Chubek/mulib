/* Separate translation unit: no implementation headers, no shared oracle. */
#include <inttypes.h>
#include <stdio.h>

int64_t probe_add(int64_t, int64_t);
int64_t probe_sub(int64_t, int64_t);
int64_t probe_interval(int64_t, int64_t, int64_t);

int main(void) {
    int64_t a, b, c;
    int scanned;
    while ((scanned = scanf("%" SCNd64 " %" SCNd64 " %" SCNd64, &a, &b, &c)) == 3) {
        if (b > c)
            return 2;
        printf("%" PRId64 " %" PRId64 " %" PRId64 "\n",
               probe_add(a, b), probe_sub(a, b), probe_interval(a, b, c));
    }
    return scanned == EOF && !ferror(stdin) && !ferror(stdout) ? 0 : 3;
}
