#include "runtime.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
  const char input[] = {'<', 'p', '>', 'x', '\0', 'y', '\0'};
  assert(!strcmp(on1x_html_parse_v1(input, sizeof input - 1, 0, 0), "-1"));
  assert(!strcmp(on1x_html_parse_v1(input, INT64_MIN, 0, 0), "-1"));
  assert(!strcmp(on1x_html_parse_v1(input, INT64_MAX, 0, 0), "-2"));
  assert(!strcmp(on1x_html_parse_v1(NULL, 0, 0, 0), "-1"));
  assert(!strcmp(on1x_html_parse_v1("", 0, INT64_MAX, 0), "-1"));
  assert(!strcmp(on1x_html_parse_v1("", 0, 0, INT64_MAX), "-1"));
  return 0;
}
