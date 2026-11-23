#include "../runtime.h"
#include <cstring>
#include <type_traits>

using Exchange = const char *(*)(const char *, int64_t);
static_assert(std::is_same_v<decltype(&on1x_http_exchange_v1), Exchange>);

int main()
{
  // Invalid selector and invalid URL must be rejected before host policy/I/O.
  return std::strcmp(on1x_http_exchange_v1("http://127.0.0.1:1", 2), "-1") ||
         std::strcmp(on1x_http_exchange_v1(nullptr, 0), "-1");
}
