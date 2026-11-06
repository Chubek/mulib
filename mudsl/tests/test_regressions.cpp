#include "MuDSL.hpp"
#include <cassert>
#include <limits>

int main() {
  static_assert(dsl::pattern<"[a-z]+z">::matches("az"));
  static_assert(dsl::pattern<"^a+$">::matches("aaa"));
  static_assert(!dsl::pattern<"^a+$">::matches(""));
  static_assert(!dsl::pattern<"a+$">::matches("ab"));
  dsl::SourceMap empty;
  assert(empty.line_count() == 1);
  assert((empty.line_column(0) == std::pair<std::size_t,std::size_t>{1,1}));
  assert(empty.line_text(1).empty());
  dsl::SourceMap source("a\nb");
  assert((source.line_column(std::numeric_limits<std::size_t>::max()) == std::pair<std::size_t,std::size_t>{2,2}));
  assert((source.line_column(2) == std::pair<std::size_t,std::size_t>{2,1}));
}
