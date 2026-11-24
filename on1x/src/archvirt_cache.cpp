#include <on1x/archvirt_cache.hpp>
#include <mulib-sha.h>

#include <stdexcept>

#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace on1x::archvirt_cache {
namespace {
[[noreturn]] void fail(const char* text) {
  throw std::runtime_error(std::string("ArchVirt cache: ") + text);
}

class Sha256 {
public:
  Sha256() {
    if (!mulib_sha_init(&ctx_, MULIB_SHA256)) fail("SHA-256 initialization failed");
  }
  void add(std::string_view bytes) {
    if (!bytes.empty() && !mulib_sha_update(&ctx_, bytes.data(), bytes.size()))
      fail("SHA-256 update failed");
  }
  std::string finish() {
    unsigned char digest[32];
    if (!mulib_sha_final(&ctx_, digest, sizeof digest)) fail("SHA-256 finalization failed");
    constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (auto b : digest) {
      out += hex[b >> 4];
      out += hex[b & 15];
    }
    return out;
  }

private:
  mulib_sha_ctx ctx_;
};
}

std::string sha256(std::string_view bytes) {
  Sha256 hash;
  hash.add(bytes);
  return hash.finish();
}

bool valid_key(std::string_view key) noexcept {
  return key.size() == 64 && key.find_first_not_of("0123456789abcdef") == key.npos;
}

#if defined(__unix__) || defined(__APPLE__)
namespace {
constexpr std::size_t max_elf_size = 64 * 1024 * 1024;
constexpr char pending[] = ".pending";

void validate_identity(const Identity& identity) {
  if (identity.profile.empty() || identity.profile.size() > 256 ||
      identity.profile.find('\0') != std::string::npos || !valid_key(identity.archive_sha256))
    fail("invalid guest profile or archive digest");
}

std::string key_for(const Identity& identity, std::string_view elf) {
  Sha256 hash;
  hash.add("on1x.archvirt.guest/v1");
  hash.add(std::string_view("\0", 1));
  hash.add(identity.profile);
  hash.add(std::string_view("\0", 1));
  hash.add(identity.archive_sha256); // Fixed width; the remainder is exactly the ELF.
  hash.add(elf);
  return hash.finish();
}

[[noreturn]] void sysfail(const char* action) {
  const int error = errno;
  throw std::system_error(error, std::generic_category(), std::string("ArchVirt cache: ") + action);
}

class Fd {
public:
  explicit Fd(int fd = -1) noexcept : fd_(fd) {}
  ~Fd() { if (fd_ >= 0) ::close(fd_); }
  Fd(Fd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
  Fd& operator=(Fd&& other) noexcept {
    Fd old(std::exchange(fd_, std::exchange(other.fd_, -1)));
    return *this;
  }
  int get() const noexcept { return fd_; }
  int release() noexcept { return std::exchange(fd_, -1); }
  explicit operator bool() const noexcept { return fd_ >= 0; }

private:
  int fd_;
};

struct stat status(int fd) {
  struct stat st{};
  if (::fstat(fd, &st) < 0) sysfail("cannot inspect open cache path");
  return st;
}

constexpr int directory_flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;
Fd directory_at(int parent, const char* name, bool create) {
  Fd dir(::openat(parent, name, directory_flags));
  if (dir) return dir;
  if (errno != ENOENT) sysfail("cannot open cache directory without following symlinks");
  if (!create) return dir;
  if (::mkdirat(parent, name, 0700) < 0 && errno != EEXIST) sysfail("cannot create cache directory");
  Fd created(::openat(parent, name, directory_flags));
  if (!created) sysfail("cannot open created cache directory without following symlinks");
  return created;
}

void check_directory(int fd, bool ancestor, bool private_directory = false) {
  const auto st = status(fd);
  const auto user = ::geteuid();
  if (st.st_uid != user && !(ancestor && st.st_uid == 0))
    fail("cache path has an untrusted owner");
  // A sticky ancestor such as /tmp protects the next component against
  // renaming by other users; that component must have a trusted owner too.
  if ((st.st_mode & 0022) && !(ancestor && (st.st_mode & S_ISVTX)))
    fail("cache path is writable by other users");
  if (private_directory && (st.st_mode & 07777) != 0700)
    fail("ArchVirt namespace must have mode 0700");
}

// Do not normalize away '.' or '..'; each actual directory component must be
// inspected via a descriptor starting at /, without following symlinks.
Fd base_directory(std::string_view path, bool create) {
  std::vector<std::string> components;
  for (std::size_t at = 1; at < path.size();) {
    const auto next = path.find('/', at);
    auto component = path.substr(at, next == path.npos ? next : next - at);
    if (component == "." || component == "..") fail("cache path has a dot component");
    if (!component.empty()) components.emplace_back(component);
    if (next == path.npos) break;
    at = next + 1;
  }
  if (components.empty()) fail("cache base must not be the filesystem root");
  Fd dir(::open("/", directory_flags));
  if (!dir) sysfail("cannot open filesystem root");
  check_directory(dir.get(), true);
  for (std::size_t i = 0; i < components.size(); ++i) {
    dir = directory_at(dir.get(), components[i].c_str(), create);
    if (!dir) return dir;
    check_directory(dir.get(), i + 1 < components.size());
  }
  return dir;
}

Fd cache_directory(bool create) {
  if (::getuid() != ::geteuid() || ::getgid() != ::getegid())
    fail("unavailable in a set-id process");
  const char* xdg = std::getenv("XDG_CACHE_HOME");
  Fd base;
  if (xdg && xdg[0] == '/') {
    base = base_directory(xdg, create);
  } else {
    const char* home = std::getenv("HOME");
    if (!home || home[0] != '/') fail("set an absolute XDG_CACHE_HOME or HOME");
    base = base_directory(home, false);
    if (!base) {
      if (create) fail("HOME directory does not exist");
      return base;
    }
    base = directory_at(base.get(), ".cache", create);
    if (base) check_directory(base.get(), false);
  }
  if (!base) return base;
  auto on1x = directory_at(base.get(), "on1x", create);
  if (!on1x) return on1x;
  check_directory(on1x.get(), false);
  auto archvirt = directory_at(on1x.get(), "archvirt", create);
  if (archvirt) check_directory(archvirt.get(), false, true);
  return archvirt;
}

void lock(int dir, int mode) {
  while (::flock(dir, mode) < 0) {
    if (errno != EINTR) sysfail("cannot lock ArchVirt namespace");
  }
}

std::size_t read_some(int fd, char* out, std::size_t size) {
  for (;;) {
    const auto n = ::read(fd, out, size);
    if (n >= 0) return static_cast<std::size_t>(n);
    if (errno != EINTR) sysfail("cannot read guest artifact");
  }
}

void check_entry(const struct stat& st) {
  if (!S_ISREG(st.st_mode) || (st.st_mode & 07777) != 0600 ||
      st.st_uid != ::geteuid() || st.st_nlink != 1)
    fail("guest artifact must be an owned, single-link, mode-0600 regular file");
  if (st.st_size <= 0 || st.st_size > static_cast<off_t>(max_elf_size))
    fail("guest artifact exceeds size limits or is empty");
}

std::optional<std::string> read_entry(int dir, const char* key) {
  struct stat before{};
  if (::fstatat(dir, key, &before, AT_SYMLINK_NOFOLLOW) < 0) {
    if (errno == ENOENT) return std::nullopt;
    sysfail("cannot inspect guest artifact without following symlinks");
  }
  check_entry(before);
  Fd fd(::openat(dir, key, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
  if (!fd) sysfail("cannot open guest artifact without following symlinks");
  const auto opened = status(fd.get());
  check_entry(opened);
  if (before.st_dev != opened.st_dev || before.st_ino != opened.st_ino)
    fail("guest artifact replaced during lookup");
  std::string out(static_cast<std::size_t>(opened.st_size), '\0');
  for (std::size_t at = 0; at < out.size();) {
    const auto n = read_some(fd.get(), out.data() + at, out.size() - at);
    if (!n) fail("guest artifact truncated during read");
    at += n;
  }
  char extra;
  if (read_some(fd.get(), &extra, 1)) fail("guest artifact grew during read");
  return out;
}

void write_elf(int fd, std::string_view elf) {
  while (!elf.empty()) {
    const auto n = ::write(fd, elf.data(), elf.size());
    if (n > 0) elf.remove_prefix(static_cast<std::size_t>(n));
    else if (n == 0) fail("cannot finish writing guest artifact");
    else if (errno != EINTR) sysfail("cannot write guest artifact");
  }
}

void sync(int fd) {
  while (::fsync(fd) < 0) {
    if (errno != EINTR) sysfail("cannot sync cache changes");
  }
}

void remove_entry(int dir, const char* name) {
  if (::unlinkat(dir, name, 0) < 0 && errno != ENOENT)
    sysfail("cannot remove ArchVirt entry (directories are never traversed)");
}

struct Pending {
  int dir;
  bool active = true;
  ~Pending() { if (active) ::unlinkat(dir, pending, 0); }
};

struct CloseDirectory {
  void operator()(DIR* stream) const noexcept { ::closedir(stream); }
};
}

std::string identify(const Identity& identity, std::string_view elf) {
  validate_identity(identity);
  if (elf.empty() || elf.size() > max_elf_size) fail("guest artifact exceeds size limits or is empty");
  return key_for(identity, elf);
}

std::string publish(const Identity& identity, std::string_view elf) {
  auto key = identify(identity, elf);
  auto dir = cache_directory(true);
  lock(dir.get(), LOCK_EX);
  if (const auto existing = read_entry(dir.get(), key.c_str())) {
    if (*existing != elf) fail("existing guest artifact does not match its key; clear the cache");
    return key;
  }
  // The directory lock serializes publication and clear, so only an abandoned
  // stage can occupy this name. unlinkat never follows its target.
  remove_entry(dir.get(), pending);
  Fd fd(::openat(dir.get(), pending, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
  if (!fd) sysfail("cannot create publication stage");
  Pending cleanup{dir.get()};
  if (::fchmod(fd.get(), 0600) < 0) sysfail("cannot set guest artifact permissions");
  write_elf(fd.get(), elf);
  sync(fd.get());
  if (::renameat(dir.get(), pending, dir.get(), key.c_str()) < 0)
    sysfail("cannot atomically publish guest artifact");
  cleanup.active = false;
  sync(dir.get());
  return key;
}

std::optional<std::string> find(const Identity& identity, std::string_view key) {
  if (!valid_key(key)) fail("invalid key: expected 64 lowercase hexadecimal characters");
  validate_identity(identity);
  auto dir = cache_directory(false);
  if (!dir) return std::nullopt;
  lock(dir.get(), LOCK_SH);
  auto elf = read_entry(dir.get(), std::string(key).c_str());
  if (elf && key_for(identity, *elf) != key)
    fail("guest artifact does not match its key or the current guest profile/archive");
  return elf;
}

std::string load(const Identity& identity, std::string_view key) {
  auto elf = find(identity, key);
  if (!elf) fail("guest artifact not found");
  return std::move(*elf);
}

void clear() {
  auto dir = cache_directory(false);
  if (!dir) return;
  lock(dir.get(), LOCK_EX);
  // Keep the namespace directory (the lock inode) so cooperating processes
  // cannot end up locking different inodes across a clear.
  Fd scan(::openat(dir.get(), ".", directory_flags));
  if (!scan) sysfail("cannot open namespace for enumeration");
  std::unique_ptr<DIR, CloseDirectory> stream(::fdopendir(scan.get()));
  if (!stream) sysfail("cannot enumerate ArchVirt namespace");
  scan.release(); // The directory stream owns this descriptor now.
  std::vector<std::string> names;
  for (;;) {
    errno = 0;
    const auto* entry = ::readdir(stream.get());
    if (!entry) {
      if (errno) sysfail("cannot read ArchVirt namespace");
      break;
    }
    if (const std::string_view name(entry->d_name); valid_key(name) || name == pending)
      names.emplace_back(name);
  }
  for (const auto& name : names) remove_entry(dir.get(), name.c_str());
  if (!names.empty()) sync(dir.get());
}
#else
std::string identify(const Identity&, std::string_view) { fail("requires POSIX file descriptors and locks"); }
std::string publish(const Identity&, std::string_view) { fail("requires POSIX file descriptors and locks"); }
std::string load(const Identity&, std::string_view) { fail("requires POSIX file descriptors and locks"); }
std::optional<std::string> find(const Identity&, std::string_view) { fail("requires POSIX file descriptors and locks"); }
void clear() { fail("requires POSIX file descriptors and locks"); }
#endif
}
