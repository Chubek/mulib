#include <on1x/archvirt_cache.hpp>

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;
namespace cache = on1x::archvirt_cache;

namespace {
void check(bool expected, const char* reason) {
  if (!expected) throw std::runtime_error(reason);
}

template <class F>
void rejects(F operation, std::string_view diagnostic) {
  try {
    operation();
  } catch (const std::exception& e) {
    if (std::string_view(e.what()).find(diagnostic) != std::string_view::npos) return;
    throw std::runtime_error("wrong rejection diagnostic: " + std::string(e.what()));
  }
  throw std::runtime_error("unexpectedly accepted input: " + std::string(diagnostic));
}

void env(const char* variable, const fs::path& value) {
  check(::setenv(variable, value.c_str(), 1) == 0, "cannot set fixture environment");
}

void write_file(const fs::path& name, std::string_view bytes) {
  std::ofstream file(name, std::ios::binary | std::ios::trunc);
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  file.close();
  check(static_cast<bool>(file), "cannot write fixture file");
}

mode_t mode(const fs::path& name) {
  struct stat st{};
  check(::lstat(name.c_str(), &st) == 0, "cannot lstat fixture file");
  return st.st_mode & 07777;
}

struct Scratch {
  explicit Scratch(const fs::path& dir) {
    auto pattern = (dir / ".on1x-archvirt-cache-test-XXXXXX").string();
    check(::mkdtemp(pattern.data()) != nullptr, "cannot create fixture directory");
    path = pattern;
  }
  ~Scratch() {
    std::error_code ignored;
    fs::remove_all(path, ignored);
  }
  fs::path path;
};

void races(const cache::Identity& identity, const std::string& elf, const std::string& key) {
  int gate[2];
  check(::pipe(gate) == 0, "cannot create process start gate");
  std::vector<pid_t> children;
  constexpr int jobs = 8;
  for (int i = 0; i < jobs; ++i) {
    const auto child = ::fork();
    check(child >= 0, "cannot fork cache test worker");
    if (child == 0) {
      ::close(gate[1]);
      char signal;
      while (::read(gate[0], &signal, 1) < 0) {
        if (errno != EINTR) ::_exit(2);
      }
      ::close(gate[0]);
      try {
        const auto other = elf + static_cast<char>(i);
        for (int n = 0; n < 32; ++n) {
          if (i < 4) {
            check(cache::publish(identity, elf) == key, "unstable simultaneous publication");
            check(cache::publish(identity, other) != key, "different bytes shared a key");
          } else if (i < 7) {
            std::string snapshot;
            try {
              snapshot = cache::load(identity, key);
            } catch (const std::exception& e) {
              // Clear is a valid concurrent outcome; any other error is not.
              if (std::string_view(e.what()).find("not found") == std::string_view::npos) throw;
              continue;
            }
            check(snapshot == elf, "concurrent load returned partial or incorrect bytes");
          } else {
            cache::clear();
          }
        }
      } catch (const std::exception& e) {
        std::cerr << "cache worker: " << e.what() << '\n';
        ::_exit(1);
      }
      ::_exit(0);
    }
    children.push_back(child);
  }
  ::close(gate[0]);
  ::close(gate[1]);
  for (const auto child : children) {
    int state;
    pid_t reaped;
    do { reaped = ::waitpid(child, &state, 0); } while (reaped < 0 && errno == EINTR);
    check(reaped == child && WIFEXITED(state) && WEXITSTATUS(state) == 0, "cache worker failed");
  }
}
}

int main(int argc, char** argv) {
  try {
    check(argc == 2, "usage: test_archvirt_cache ABSOLUTE_PRIVATE_SCRATCH_PARENT");
    Scratch scratch{fs::path(argv[1])};
    const auto root = scratch.path / "xdg";
    const auto on1x = root / "on1x";
    const auto dir = on1x / "archvirt";
    const auto outside = scratch.path / "outside";
    env("XDG_CACHE_HOME", root);
    const auto old_mask = ::umask(0);
    const cache::Identity identity{"riscv64/test-boot-v1", cache::sha256("first archive")};
    const cache::Identity other_archive{identity.profile, cache::sha256("second archive")};
    const cache::Identity other_profile{"riscv64/test-boot-v2", identity.archive_sha256};
    const auto elf = std::string("\x7f" "ELF\0fixture", 12) + std::string(128 * 1024, '\xA7');
    const auto expected_key = cache::sha256(std::string("on1x.archvirt.guest/v1") + '\0' +
                                            identity.profile + '\0' + identity.archive_sha256 + elf);

    check(cache::sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA-256 abc known answer mismatch");
    check(cache::sha256({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA-256 empty known answer mismatch");
    cache::clear();
    rejects([&] { cache::load(identity, expected_key); }, "not found");
    rejects([&] { cache::publish(identity, {}); }, "empty");
    rejects([&] { cache::publish({identity.profile, "not-a-digest"}, elf); }, "invalid guest profile");
    rejects([&] { cache::load({{}, identity.archive_sha256}, expected_key); }, "invalid guest profile");
    rejects([&] { cache::load({std::string("r\0v", 3), identity.archive_sha256}, expected_key); },
            "invalid guest profile");
    for (const auto& invalid : {std::string("../outside"), std::string(64, 'A'),
                                std::string(63, 'a'), std::string(64, '\0'),
                                std::string(62, 'a') + "/b"})
      rejects([&] { cache::load(identity, invalid); }, "invalid key");
    check(!fs::exists(root), "clear, miss or invalid input created cache directories");

    const auto key = cache::publish(identity, elf);
    check(key == expected_key && cache::valid_key(key), "build content/key mismatch");
    check(cache::publish(identity, elf) == key, "same build did not reuse its key");
    check(mode(dir) == 0700 && mode(dir / key) == 0600, "namespace or artifact not private");
    check(cache::load(identity, key) == elf, "round trip changed binary bytes");
    check(cache::publish(other_archive, elf) != key, "archive bytes did not change the key");
    check(cache::publish(other_profile, elf) != key, "guest profile did not change the key");
    check(cache::publish(identity, elf + 'x') != key, "ELF bytes did not change the key");
    rejects([&] { cache::load(other_archive, key); }, "current guest profile/archive");
    rejects([&] { cache::load(other_profile, key); }, "current guest profile/archive");

    check(::chmod(dir.c_str(), 0755) == 0, "cannot change namespace permissions");
    rejects([&] { cache::load(identity, key); }, "mode 0700");
    rejects([] { cache::clear(); }, "mode 0700");
    check(::chmod(dir.c_str(), 0700) == 0, "cannot restore namespace permissions");
    check(::chmod((dir / key).c_str(), 0644) == 0, "cannot change artifact permissions");
    rejects([&] { cache::load(identity, key); }, "mode-0600");
    check(::chmod((dir / key).c_str(), 0600) == 0, "cannot restore artifact permissions");
    write_file(dir / key, elf + 'x');
    rejects([&] { cache::load(identity, key); }, "does not match its key");
    rejects([&] { cache::publish(identity, elf); }, "does not match its key");

    fs::create_directories(on1x / "other-backend");
    write_file(on1x / "other-backend" / "keep", "another namespace");
    write_file(outside, "outside target");
    check(::chmod(outside.c_str(), 0600) == 0, "cannot set outside file permissions");
    fs::remove(dir / key);
    fs::create_hard_link(outside, dir / key);
    rejects([&] { cache::load(identity, key); }, "single-link");
    fs::remove(dir / key);
    check(::mkfifo((dir / key).c_str(), 0600) == 0, "cannot make FIFO fixture");
    rejects([&] { cache::load(identity, key); }, "regular file");
    fs::remove(dir / key);
    write_file(dir / key, elf);
    check(::chmod((dir / key).c_str(), 0600) == 0, "cannot set oversized fixture permissions");
    fs::resize_file(dir / key, 64 * 1024 * 1024 + 1);
    rejects([&] { cache::load(identity, key); }, "size limits");
    fs::remove(dir / key);
    fs::create_symlink(outside, dir / key);
    fs::create_symlink(outside, dir / ".pending");
    write_file(dir / "not-a-cache-entry", "keep");
    rejects([&] { cache::load(identity, key); }, "regular file");
    rejects([&] { cache::publish(identity, elf); }, "regular file");
    cache::clear();
    check(!fs::is_symlink(dir / key) && !fs::is_symlink(dir / ".pending"),
          "clear left cache symlink names behind");
    check(fs::file_size(outside) == 14 && fs::exists(on1x / "other-backend" / "keep") &&
          fs::exists(dir / "not-a-cache-entry"), "clear affected unrelated state");
    fs::create_directory(dir / key);
    write_file(dir / key / "keep", "not traversed");
    rejects([] { cache::clear(); }, "never traversed");
    check(fs::exists(dir / key / "keep"), "clear recursively removed contents of a directory");
    fs::remove(dir / key / "keep");
    fs::remove(dir / key);

    fs::create_symlink(outside, dir / ".pending");
    check(cache::publish(identity, elf) == key && !fs::is_symlink(dir / ".pending") &&
          fs::file_size(outside) == 14, "publication traversed an abandoned stage");
    const auto snapshot = cache::load(identity, key);
    races(identity, elf, key);
    cache::clear();
    rejects([&] { cache::load(identity, key); }, "not found");
    check(snapshot == elf, "clear invalidated an existing replay snapshot");
    check(cache::publish(identity, elf) == key && cache::load(identity, key) == elf,
          "cache unusable after concurrent clear");

    fs::rename(dir, on1x / "actual-archvirt");
    fs::create_directory_symlink(on1x / "actual-archvirt", dir);
    rejects([&] { cache::load(identity, key); }, "without following symlinks");
    rejects([&] { cache::publish(identity, elf); }, "without following symlinks");
    rejects([] { cache::clear(); }, "without following symlinks");
    check(fs::exists(on1x / "actual-archvirt" / key), "clear followed namespace symlink");
    fs::create_directory_symlink(root, scratch.path / "redirect");
    env("XDG_CACHE_HOME", scratch.path / "redirect" / "nested");
    rejects([&] { cache::publish(identity, elf); }, "without following symlinks");
    check(!fs::exists(root / "nested"), "publication followed ancestor symlink");
    env("XDG_CACHE_HOME", root / ".." / "elsewhere");
    rejects([&] { cache::publish(identity, elf); }, "dot component");
    fs::create_directory(scratch.path / "shared");
    check(::chmod((scratch.path / "shared").c_str(), 0777) == 0, "cannot make writable fixture path");
    env("XDG_CACHE_HOME", scratch.path / "shared" / "nested");
    rejects([&] { cache::publish(identity, elf); }, "writable by other users");
    check(!fs::exists(scratch.path / "shared" / "nested"), "written below untrusted ancestor");

    env("XDG_CACHE_HOME", "relative-xdg-ignored");
    env("HOME", scratch.path);
    check(cache::publish(identity, elf) == key &&
          fs::exists(scratch.path / ".cache" / "on1x" / "archvirt" / key),
          "absolute HOME fallback was not used");
    env("HOME", "relative-home");
    rejects([] { cache::clear(); }, "absolute XDG_CACHE_HOME or HOME");
    ::umask(old_mask);
    std::cout << "ArchVirt cache tests passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
