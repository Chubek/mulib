// Generic embedding services only. Benchmark policy is authored in MuLL.
#include "MuDSL.hpp"
#include "mull/stdlib/Stdlib.hpp"
extern "C" {
#include "bench_host.h"
}
#include <fstream>
#include <iostream>
#include <sys/utsname.h>
#include <ctime>
#include <cstring>
#include <cerrno>

using V = mull::Value;
namespace fs = std::filesystem;
static constexpr std::size_t byte_limit = 16 * 1024 * 1024;
static std::string path(const V &v) {
  auto s = v.string();
  if (s.empty() || s.find('\0') != std::string::npos) throw mull::Error("invalid host path");
  return s;
}
int main(int argc, char **argv) try {
  mull::Runtime vm({fs::path(BENCH_ROOT) / "lib", fs::path(MULIB_ROOT) / "mudsl/mull/stdlib"});
  mull::stdlib::install(vm);
  vm.maxSteps = 50000000;
  vm.maxFrames = 3000000;
  vm.stdoutSink = [](std::string_view s) { std::cout << s; };
  V::Object host;
  V::List args;
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  host["Args"] = args;
  host["Root"] = MULIB_ROOT;
  host["Keys"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 1); V::List keys;
    for (const auto &[key, value] : a[0].object()) { (void)value; keys.emplace_back(key); }
    return keys;
  });
  host["Record"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 1); V::Object result;
    for (const auto &pair : a[0].list()) {
      const auto &p = pair.list();
      if (p.size() != 2 || !result.emplace(p[0].string(), p[1]).second)
        throw mull::Error("invalid or duplicate record field");
    }
    return result;
  });
  host["Byte"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 1);
    if (a[0].string().size() != 1) throw mull::Error("Byte expects one byte");
    return int(static_cast<unsigned char>(a[0].string()[0]));
  });
  host["Char"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 1); double n = a[0].number();
    if (n < 0 || n > 255 || std::floor(n) != n) throw mull::Error("invalid byte");
    return std::string(1, static_cast<char>(n));
  });
  host["Read"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 1); auto p = path(a[0]);
    if (fs::file_size(p) > byte_limit) throw mull::Error("read byte budget exceeded");
    std::ifstream in(p, std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(in)), {});
    if (!in || s.size() > byte_limit) throw mull::Error("read failed: " + p);
    return s;
  });
  host["Write"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 2); auto p = path(a[0]); const auto &s = a[1].string();
    if (s.size() > byte_limit) throw mull::Error("write byte budget exceeded");
    std::ofstream out(p + ".tmp", std::ios::binary);
    out.write(s.data(), static_cast<std::streamsize>(s.size())); out.close();
    if (!out) throw mull::Error("write failed: " + p);
    fs::rename(p + ".tmp", p); return {};
  });
  host["Mkdir"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 1); fs::create_directories(path(a[0])); return {};
  });
  host["Exists"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 1); return fs::exists(path(a[0]));
  });
  host["Size"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 1); return double(fs::file_size(path(a[0])));
  });
  host["Copy"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 2); fs::copy_file(path(a[0]), path(a[1])); return {};
  });
  host["Run"] = V::Native([](const auto &a) -> V {
    mull::arity(a, 4); std::vector<std::string> storage;
    for (const auto &arg : a[0].list()) {
      const auto &s = arg.string();
      if (s.find('\0') != std::string::npos) throw mull::Error("NUL in argv");
      storage.push_back(s);
    }
    if (storage.empty() || storage.size() > 256) throw mull::Error("invalid argv count");
    std::vector<const char *> argv;
    for (const auto &s : storage) argv.push_back(s.c_str());
    argv.push_back(nullptr);
    double timeout = a[3].number();
    if (timeout < 1 || timeout > 3600000 || std::floor(timeout) != timeout)
      throw mull::Error("invalid timeout");
    on1x_bench_result r{};
    std::string error;
    if (on1x_bench_run(argv.data(), path(a[1]).c_str(), path(a[2]).c_str(),
                       static_cast<unsigned>(timeout), &r) != 0) {
      error = std::strerror(errno);
      if (!fs::exists(path(a[1]))) std::ofstream(path(a[1]), std::ios::binary);
      std::ofstream(path(a[2]), std::ios::app) << "host: " << error << '\n';
      r.exit_code = -1;
    }
    return V::Object{{"exit_code", r.exit_code}, {"signal", r.signal_number},
                     {"timed_out", bool(r.timed_out)}, {"elapsed_ns", r.elapsed_ns},
                     {"host_error", error}};
  });
  struct utsname u{};
  if (uname(&u) != 0) throw mull::Error("uname failed");
  host["Machine"] = V::Object{{"system", u.sysname}, {"release", u.release},
                              {"machine", u.machine}, {"hostname", u.nodename}};
  host["EpochSeconds"] = double(std::time(nullptr));
  vm.registerLibrary("bench-host", host);
  auto result = vm.runFile(fs::path(BENCH_ROOT) / "main.mull");
  return static_cast<int>(result.number());
} catch (const std::exception &e) {
  std::cerr << "on1x-benchmark: " << e.what() << '\n';
  return 1;
}
