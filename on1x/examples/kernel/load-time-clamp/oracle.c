/* Standalone reference: no MuLIB headers, generated code, or On1x runtime. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GCC and Clang on AMD64 support a signed type wide enough for any
 * sum or difference of two int64_t values. Widen before arithmetic. */
__extension__ typedef __int128 wide_int;

static int64_t saturate(wide_int v) {
  if (v < INT64_MIN) return INT64_MIN;
  if (v > INT64_MAX) return INT64_MAX;
  return (int64_t)v;
}

static int clamp(int64_t value, int64_t low, int64_t high, int64_t *result) {
  if (high < low) return 0;
  if (value > high) value = high;
  if (value < low) value = low;
  *result = value;
  return 1;
}

static int print_clamp(int64_t value, int64_t low, int64_t high) {
  int64_t result;
  if (!clamp(value, low, high, &result)) {
    fputs("clamp: reversed bounds\n", stderr);
    return 70;
  }
  printf("%" PRId64 "\n", result);
  return 0;
}

int main(int argc, char **argv) {
  const int64_t low = INT64_MIN;
  const int64_t high = INT64_MAX;
  if (argc == 2 && strcmp(argv[1], "guard") == 0)
    return print_clamp(5, 9, 1);
  if (argc != 1) return EXIT_FAILURE;

  const int64_t clamps[][3] = {
    {27, 0, 10}, {-1, 0, 10}, {4, 0, 10}, {0, 0, 10}, {10, 0, 10},
    {20, -7, -7}, {low, low, high}, {high, low, high}
  };
  const int64_t adds[][2] = {
    {40, 2}, {high, 1}, {low, -1}, {high - 1, 1}, {low + 1, -1},
    {low, high}, {high, high}, {low, low}, {low, 0}
  };
  const int64_t subs[][2] = {
    {4, -2}, {high, -1}, {low, 1}, {high - 1, -1}, {low + 1, 1},
    {high, low}, {low, high}, {low, low}, {high, 0}
  };
  for (size_t i = 0; i < sizeof clamps / sizeof *clamps; ++i) {
    int status = print_clamp(clamps[i][0], clamps[i][1], clamps[i][2]);
    if (status) return status;
  }
  for (size_t i = 0; i < sizeof adds / sizeof *adds; ++i)
    printf("%" PRId64 "\n", saturate((wide_int)adds[i][0] + adds[i][1]));
  for (size_t i = 0; i < sizeof subs / sizeof *subs; ++i)
    printf("%" PRId64 "\n", saturate((wide_int)subs[i][0] - subs[i][1]));
  return 0;
}
