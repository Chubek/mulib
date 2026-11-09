#pragma once
#include "../../MuDSL.hpp"
#include <cstdint>

namespace mull::mu3ac {
using Handle = std::uint64_t;
struct View { const char *data; std::uint64_t size; };
struct ForeignEntry { View name; void *address; std::uint64_t arity; };
struct Config {
  std::uint64_t version;
  View source;
  View mullierRules;
  std::uint64_t maxSteps, maxCalls, maxFrames, maxSource, maxValues;
  std::uint64_t pathCount, preloadCount, foreignCount;
  const View *paths, *preloads;
  const ForeignEntry *foreign;
  std::uint64_t flags;
};
static_assert(sizeof(View) == 16 && sizeof(ForeignEntry) == 32 && sizeof(Config) == 136,
              "Mu3AC native ABI requires 64-bit pointers");
using Entry = Handle (*)(void *, void *);
int nativeMain(const Config &, Entry, int, char **);
}
