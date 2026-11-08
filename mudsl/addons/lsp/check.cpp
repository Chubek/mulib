// Parse only: editor diagnostics must never evaluate a document or load packages.
#include "MuDSL.hpp"
#include <iostream>
#include <iterator>

static void quote(std::string_view text) {
  std::cout << '"';
  for (unsigned char c : text) {
    if (c == '"' || c == '\\') std::cout << '\\' << c;
    else if (c < 32 || c >= 127) {
      constexpr char hex[] = "0123456789abcdef";
      std::cout << "\\u00" << hex[c >> 4] << hex[c & 15];
    } else std::cout << c;
  }
  std::cout << '"';
}

int main() {
  std::string source;
  char buffer[8192];
  while (std::cin.read(buffer, sizeof buffer) || std::cin.gcount()) {
    source.append(buffer, static_cast<std::size_t>(std::cin.gcount()));
    if (source.size() > 4194304) {
      std::cerr << "source exceeds 4 MiB\n";
      return 2;
    }
  }
  try {
    mull::detail::Parser(source).parse();
    std::cout << "null\n";
  } catch (const mull::Error &error) {
    std::cout << "{\"line\":" << error.line << ",\"column\":" << error.column << ",\"message\":";
    quote(error.what());
    std::cout << "}\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 2;
  }
}
