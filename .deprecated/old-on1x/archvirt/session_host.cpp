#include <mulib-readline.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using std::string;
using std::string_view;
constexpr unsigned packet_limit = 4096;
constexpr unsigned batch_limit = 64;
constexpr unsigned max_sessions = 8;
constexpr unsigned max_records = 64;
constexpr unsigned max_clients = 16;
constexpr unsigned history_bytes = 32 * 1024;
constexpr unsigned total_output = 1024 * 1024;
constexpr auto command_timeout = 5s;
constexpr auto idle_timeout = 5min;
constexpr auto lifetime = 30min;
volatile std::sig_atomic_t interrupted = 0;

void signal_handler(int) { interrupted = 1; }

[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }
[[noreturn]] void system_failure(const char* message) {
  const int error = errno;
  throw std::system_error(error, std::generic_category(), message);
}

class Fd {
public:
  Fd() = default;
  explicit Fd(int fd) : fd_(fd) {}
  ~Fd() { reset(); }
  Fd(Fd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
  Fd& operator=(Fd&& other) noexcept {
    reset();
    fd_ = std::exchange(other.fd_, -1);
    return *this;
  }
  void reset() noexcept { if (fd_ >= 0) ::close(std::exchange(fd_, -1)); }
  int get() const { return fd_; }
  explicit operator bool() const { return fd_ >= 0; }
private:
  int fd_ = -1;
};

std::array<Fd, 2> pipe_pair() {
  int fd[2];
  if (::pipe2(fd, O_CLOEXEC) < 0) system_failure("cannot create session pipe");
  return {Fd(fd[0]), Fd(fd[1])};
}

void nonblocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL);
  if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    system_failure("cannot make session descriptor nonblocking");
}

bool key_ok(string_view key) {
  return key.size() == 64 && key.find_first_not_of("0123456789abcdef") == key.npos;
}

string value(string_view text) {
  if (text.empty() || text.size() > 20) fail("expected a signed decimal i64");
  const auto first_digit = text.front() == '-' || text.front() == '+' ? 1u : 0u;
  if (text.size() == first_digit || text.size() - first_digit > 19 ||
      text.find_first_not_of("0123456789", first_digit) != text.npos)
    fail("expected a signed decimal i64");
  if (text.front() == '+') text.remove_prefix(1);
  std::int64_t parsed;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
    fail("expected a signed decimal i64");
  return std::to_string(parsed) + '\n';
}

string batch(string_view text) {
  string out;
  unsigned count = 0;
  while (!text.empty()) {
    const auto end = text.find('\n');
    if (end == text.npos || ++count > batch_limit) fail("invalid or oversized parameter batch");
    out += value(text.substr(0, end));
    text.remove_prefix(end + 1);
  }
  if (!count) fail("empty parameter batch");
  return out;
}

string options(int argc, char** argv, int begin) {
  string result;
  unsigned count = 0;
  for (int i = begin; i < argc; i += 2) {
    if (string_view(argv[i]) != "--param" || i + 1 == argc || ++count > batch_limit)
      fail("expected at most 64 --param VALUE pairs");
    result += value(argv[i + 1]);
  }
  return result;
}

Fd socket_fd() {
  Fd fd(::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
  if (!fd) system_failure("cannot create local session socket");
  return fd;
}

struct Address {
  explicit Address(const string& name) {
    if (name.size() >= sizeof(addr.sun_path)) fail("local session socket path is too long");
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, name.c_str(), name.size() + 1);
    size = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + name.size() + 1);
  }
  sockaddr_un addr{};
  socklen_t size;
};

string fd_path(int fd) { return "/proc/self/fd/" + std::to_string(fd); }

void check_peer(int fd) {
  ucred peer{};
  socklen_t len = sizeof peer;
  if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &len) < 0 ||
      len != sizeof peer || peer.uid != ::geteuid())
    fail("session socket peer is not the current user");
}

struct stat status(int fd) {
  struct stat st{};
  if (::fstat(fd, &st) < 0) system_failure("cannot inspect local session descriptor");
  return st;
}

constexpr int dir_flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;

Fd session_dir(bool create) {
  Fd tmp(::open("/tmp", dir_flags));
  if (!tmp) system_failure("cannot open /tmp without following symlinks");
  const auto parent = status(tmp.get());
  if (parent.st_uid != 0 || !(parent.st_mode & S_ISVTX))
    fail("local sessions require a root-owned sticky /tmp");
  const string name = "on1x-archvirt-" + std::to_string(::geteuid());
  if (create && ::mkdirat(tmp.get(), name.c_str(), 0700) < 0 && errno != EEXIST)
    system_failure("cannot create private session namespace");
  Fd dir(::openat(tmp.get(), name.c_str(), dir_flags));
  if (!dir && errno != ENOENT) system_failure("cannot safely open local session namespace");
  if (dir) {
    const auto st = status(dir.get());
    if (st.st_uid != ::geteuid() || (st.st_mode & 07777) != 0700)
      fail("local session namespace must be owner-only (0700)");
  }
  return dir;
}

void namespace_lock(int dir) {
  while (::flock(dir, LOCK_EX) < 0) {
    if (errno != EINTR) system_failure("cannot lock local session namespace");
  }
}

// Open the socket inode without traversing its name, then connect through its
// descriptor. Connecting never follows a user-supplied filesystem symlink.
struct Endpoint {
  Fd fd;
  bool stale = false;
};

Endpoint dial(int dir, const string& id) {
  Fd inode(::openat(dir, id.c_str(), O_PATH | O_NOFOLLOW | O_CLOEXEC));
  if (!inode && errno == ENOENT) return {};
  if (!inode) system_failure("cannot open local session endpoint");
  const auto st = status(inode.get());
  if (!S_ISSOCK(st.st_mode) || st.st_uid != ::geteuid() || (st.st_mode & 07777) != 0700)
    fail("local session endpoint is not an owner-only socket");
  auto fd = socket_fd();
  const Address endpoint(fd_path(inode.get()));
  if (::connect(fd.get(), reinterpret_cast<const sockaddr*>(&endpoint.addr), endpoint.size) == 0) {
    check_peer(fd.get());
    return {std::move(fd), false};
  }
  if (errno != ECONNREFUSED) system_failure("cannot connect to local session (possibly busy)");
  return {Fd{}, true};
}

struct CloseDirectory { void operator()(DIR* dir) const { ::closedir(dir); } };

void enforce_quota(int dir) {
  const int fd = ::openat(dir, ".", dir_flags);
  if (fd < 0) system_failure("cannot scan local sessions");
  std::unique_ptr<DIR, CloseDirectory> stream(::fdopendir(fd));
  if (!stream) {
    ::close(fd);
    system_failure("cannot enumerate local sessions");
  }
  unsigned scanned = 0;
  unsigned live = 0;
  unsigned records = 0;
  for (;;) {
    errno = 0;
    const auto* entry = ::readdir(stream.get());
    if (!entry) {
      if (errno) system_failure("cannot read local session directory");
      break;
    }
    if (++scanned > 256) fail("local session discovery limit exceeded");
    if (key_ok(entry->d_name)) {
      if (++records >= max_records) fail("local session record limit reached; stop a stale ID");
      if (dial(dir, entry->d_name).fd && ++live >= max_sessions)
        fail("local session limit reached");
    }
  }
}

bool send_packet(int fd, string_view bytes) {
  for (;;) {
    const auto sent = ::send(fd, bytes.data(), bytes.size(), MSG_NOSIGNAL);
    if (sent >= 0) return static_cast<std::size_t>(sent) == bytes.size();
    if (errno != EINTR) return false;
  }
}

// >0: packet length, 0: disconnected, -1: would block, -2: invalid/failed.
int recv_packet(int fd, std::array<char, packet_limit>& bytes) {
  for (;;) {
    const auto read = ::recv(fd, bytes.data(), bytes.size(), MSG_TRUNC);
    if (read > static_cast<ssize_t>(bytes.size())) return -2;
    if (read >= 0) return static_cast<int>(read);
    if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;
    if (errno != EINTR) return -2;
  }
}

int remaining(Clock::time_point deadline) {
  const auto delta = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
  return delta <= 0ms ? 0 : static_cast<int>(delta.count());
}

int poll_retry(pollfd* fds, nfds_t count, int timeout) {
  const int result = ::poll(fds, count, timeout);
  if (result < 0 && errno != EINTR) system_failure("cannot poll local session");
  return result < 0 ? 0 : result;
}

void write_display(int fd, string_view bytes) {
  if (mulib_termutils_write(fd, bytes.data(), bytes.size(), nullptr) != 0)
    fail("cannot write console output");
}

class Runner {
public:
  ~Runner() { stop(); }
  void start(const string& executable, const string& guest, int guest_fd) {
    auto input = pipe_pair();
    auto output = pipe_pair();
    auto error = pipe_pair();
    auto exec_error = pipe_pair();
    const auto daemon = ::getpid();
    pid_ = ::fork();
    if (pid_ < 0) system_failure("cannot fork ArchVirt runner");
    if (pid_ == 0) {
      if (::prctl(PR_SET_PDEATHSIG, SIGKILL) < 0 || ::getppid() != daemon ||
          ::dup2(input[0].get(), 0) < 0 || ::dup2(output[1].get(), 1) < 0 ||
          ::dup2(error[1].get(), 2) < 0 ||
          (guest_fd >= 0 && ::fcntl(guest_fd, F_SETFD, 0) < 0)) {
        const char failed = 'E';
        (void)::write(exec_error[1].get(), &failed, 1);
        ::_exit(125);
      }
      char* const args[] = {const_cast<char*>(executable.c_str()),
                            const_cast<char*>("--interactive"), const_cast<char*>(guest.c_str()), nullptr};
      ::execv(args[0], args);
      const char failed = 'E';
      (void)::write(exec_error[1].get(), &failed, 1);
      ::_exit(125);
    }
    exec_error[1].reset();
    pollfd setup{exec_error[0].get(), POLLIN, 0};
    auto deadline = Clock::now() + command_timeout;
    while (!interrupted && remaining(deadline) && !poll_retry(&setup, 1, remaining(deadline))) {}
    char byte;
    if (!setup.revents || ::read(exec_error[0].get(), &byte, 1) != 0 || exited())
      fail("cannot start interactive ArchVirt runner");
    in = std::move(input[1]);
    out = std::move(output[0]);
    err = std::move(error[0]);
    nonblocking(in.get());
    nonblocking(out.get());
    nonblocking(err.get());
  }

  bool feed(string_view text) {
    // At most 64 * 21 bytes, less than Linux PIPE_BUF. A full pipe rejects
    // the whole nonblocking write; never retry a partially accepted batch.
    for (;;) {
      const auto n = ::write(in.get(), text.data(), text.size());
      if (n >= 0) return static_cast<std::size_t>(n) == text.size();
      if (errno != EINTR) return false;
    }
  }

  bool exited() {
    if (pid_ <= 0) return true;
    const auto result = ::waitpid(pid_, &state_, WNOHANG);
    if (result == pid_ || (result < 0 && errno == ECHILD)) {
      pid_ = -1;
      return true;
    }
    return false;
  }

  string terminal_message() const {
    if (WIFEXITED(state_)) return "runner exited with status " + std::to_string(WEXITSTATUS(state_));
    if (WIFSIGNALED(state_)) return "runner terminated by signal " + std::to_string(WTERMSIG(state_));
    return "runner is no longer available";
  }

  void stop() noexcept {
    in.reset();
    if (exited()) return;
    ::kill(pid_, SIGTERM);
    const auto deadline = Clock::now() + 250ms;
    while (!exited() && Clock::now() < deadline) (void)::poll(nullptr, 0, 10);
    if (pid_ > 0) {
      ::kill(pid_, SIGKILL);
      while (::waitpid(pid_, &state_, 0) < 0 && errno == EINTR) {}
      pid_ = -1;
    }
  }

  Fd in, out, err;
private:
  pid_t pid_ = -1;
  int state_ = 0;
};

struct Client {
  Fd fd;
  Clock::time_point accepted;
  bool watches = false;
};

class Session {
public:
  explicit Session(Fd listener) : listener_(std::move(listener)) {}

  void run(Runner& runner) {
    auto last_input = Clock::now();
    const auto start = last_input;
    while (!interrupted) {
      const auto now = Clock::now();
      if (now - last_input >= idle_timeout || now - start >= lifetime) {
        end_ = "session time limit reached";
        break;
      }
      std::array<pollfd, 3 + max_clients> fds{};
      fds[0] = {listener_.get(), POLLIN, 0};
      fds[1] = {runner.out.get(), POLLIN, 0};
      fds[2] = {runner.err.get(), POLLIN, 0};
      const auto count = clients_.size();
      for (std::size_t i = 0; i < count; ++i) fds[3 + i] = {clients_[i].fd.get(), POLLIN, 0};
      poll_retry(fds.data(), 3 + count, 100);
      if (fds[1].revents) output(runner.out, 'O');
      if (fds[2].revents) output(runner.err, 'R');
      if (bytes_ > total_output) {
        end_ = "runner output limit reached";
        break;
      }
      if (runner.exited()) {
        // Runner is gone. The pipes can only contain the last bounded output.
        while ((runner.out || runner.err) && bytes_ <= total_output) {
          output(runner.out, 'O', true);
          output(runner.err, 'R', true);
        }
        end_ = bytes_ > total_output ? "runner output limit reached" : runner.terminal_message();
        break;
      }
      if (fds[0].revents & POLLIN) accept_one();
      for (std::size_t i = 0; i < count && !stopper_; ++i) {
        auto& client = clients_[i];
        if (!client.fd) continue;
        if (fds[3 + i].revents & POLLIN) request(client, runner, last_input);
        if ((fds[3 + i].revents & (POLLERR | POLLHUP | POLLNVAL)) ||
            (!client.watches && Clock::now() - client.accepted > command_timeout)) client.fd.reset();
      }
      std::erase_if(clients_, [](const Client& client) { return !client.fd; });
      if (stopper_) {
        end_ = "session stopped";
        break;
      }
    }
  }

  void finish(bool cleaned) {
    for (const auto& client : clients_) {
      if (client.watches) send_packet(client.fd.get(), "D" + end_);
    }
    if (stopper_) send_packet(stopper_.get(), cleaned ? "A" : "Ecannot remove stopped session endpoint");
  }
  bool explicitly_stopped() const { return static_cast<bool>(stopper_); }

private:
  void accept_one() {
    Fd fd(::accept4(listener_.get(), nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC));
    if (!fd) return;
    try { check_peer(fd.get()); } catch (const std::exception&) { return; }
    if (clients_.size() >= max_clients) {
      send_packet(fd.get(), "Esession client limit reached");
      return;
    }
    clients_.push_back({std::move(fd), Clock::now()});
  }

  void output(Fd& fd, char kind, bool closing = false) {
    if (!fd) return;
    std::array<char, packet_limit> packet;
    packet[0] = kind;
    const auto n = ::read(fd.get(), packet.data() + 1, packet.size() - 1);
    if (n <= 0) {
      if (n == 0 || closing || (errno != EAGAIN && errno != EINTR)) fd.reset();
      return;
    }
    bytes_ += static_cast<unsigned>(n);
    if (bytes_ > total_output) return;
    const string_view bytes(packet.data(), static_cast<std::size_t>(n) + 1);
    for (auto& client : clients_) {
      if (client.fd && client.watches && !send_packet(client.fd.get(), bytes)) client.fd.reset();
    }
    history_.emplace_back(bytes);
    history_size_ += bytes.size();
    while (history_size_ > history_bytes) {
      history_size_ -= history_.front().size();
      history_.pop_front();
    }
  }

  void request(Client& client, Runner& runner, Clock::time_point& last_input) {
    std::array<char, packet_limit> bytes;
    const auto n = recv_packet(client.fd.get(), bytes);
    if (n == -1) return;
    if (n <= 0) {
      if (n == -2) send_packet(client.fd.get(), "Einvalid session packet");
      client.fd.reset();
      return;
    }
    const string_view msg(bytes.data(), static_cast<std::size_t>(n));
    string response = "A";
    if (msg == "W" && !client.watches) {
      for (const auto& frame : history_) {
        if (!send_packet(client.fd.get(), frame)) { client.fd.reset(); return; }
      }
      client.watches = true;
    } else if (msg == "P") {
      // A probe acknowledges only a functioning supervisor; it cannot keep
      // a VM alive or update its input-idle deadline.
    } else if (msg == "Q" && !client.watches) {
      stopper_ = std::move(client.fd);
      return;
    } else if (msg.front() == 'S') {
      try {
        if (!runner.feed(batch(msg.substr(1)))) fail("runner input pipe is full or closed");
        last_input = Clock::now();
      } catch (const std::exception& e) { response = "E" + string(e.what()); }
    } else {
      response = "Einvalid session request";
    }
    if (!send_packet(client.fd.get(), response)) client.fd.reset();
  }

  Fd listener_;
  Fd stopper_;
  std::vector<Client> clients_;
  std::deque<string> history_;
  std::size_t history_size_ = 0;
  unsigned bytes_ = 0;
  string end_ = "session interrupted";
};

// Keep only stdio plus explicit ownership descriptors, even if the coordinator
// inherited unrelated file/socket handles. Linux close_range is required.
void close_except(std::array<int, 4> keep) {
  std::sort(keep.begin(), keep.end());
  unsigned next = 3;
  for (const int entry : keep) {
    if (entry < 3) continue;
    const auto fd = static_cast<unsigned>(entry);
    if (next < fd && ::syscall(SYS_close_range, next, fd - 1, 0u) < 0)
      system_failure("cannot close unrelated detached descriptors");
    next = fd + 1;
  }
  if (::syscall(SYS_close_range, next, ~0u, 0u) < 0)
    system_failure("cannot close unrelated detached descriptors");
}

Fd pin_guest(const string& guest) {
  constexpr string_view prefix = "/proc/self/fd/";
  Fd image;
  if (string_view(guest).starts_with(prefix)) {
    const auto text = string_view(guest).substr(prefix.size());
    int fd;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), fd);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || fd < 3)
      fail("invalid inherited guest file descriptor");
    image = Fd(::fcntl(fd, F_DUPFD_CLOEXEC, 3));
  } else {
    image = Fd(::open(guest.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW));
  }
  if (!image) system_failure("cannot pin verified guest artifact");
  if (!S_ISREG(status(image.get()).st_mode)) fail("guest artifact is not a regular file");
  return image;
}

void remove_socket(int dir, const string& id) {
  if (::unlinkat(dir, id.c_str(), 0) < 0 && errno != ENOENT)
    system_failure("cannot remove session endpoint");
}

[[noreturn]] void supervise(Fd dir, Fd listener, Fd setup, const string& id,
                            const string& executable, const string& guest, int guest_fd,
                            const string& initial) {
  Runner runner;
  Session session(Fd{});
  bool ready = false;
  int result = 0;
  string startup_error;
  try {
    close_except({dir.get(), listener.get(), setup.get(), guest_fd});
    Fd null(::open("/dev/null", O_RDWR | O_CLOEXEC | O_NOFOLLOW));
    if (::setsid() < 0 || !null || ::dup2(null.get(), 0) < 0 ||
        ::dup2(null.get(), 1) < 0 || ::dup2(null.get(), 2) < 0)
      system_failure("cannot detach session supervisor");
    runner.start(executable, guest, guest_fd);
    if (guest_fd >= 0) ::close(guest_fd);
    if (!initial.empty() && !runner.feed(initial)) fail("cannot send initial guest parameters");
    session = Session(std::move(listener));
    if (send_packet(setup.get(), "A")) {
      // Do not retain a guest when the client times out or is interrupted
      // during startup. A second acknowledgment confirms it got readiness.
      const auto deadline = Clock::now() + command_timeout;
      pollfd status{setup.get(), POLLIN, 0};
      while (!interrupted && remaining(deadline) &&
             !poll_retry(&status, 1, remaining(deadline))) {}
      std::array<char, packet_limit> packet;
      ready = (status.revents & POLLIN) && recv_packet(setup.get(), packet) == 1 && packet[0] == 'A';
    }
    if (ready) {
      setup.reset();
      session.run(runner);
    }
  } catch (const std::exception& e) {
    if (!ready) startup_error = "E" + string(e.what());
    result = 125;
  }
  runner.stop();
  // A previously ready session leaves an owner-only stale endpoint when it
  // dies. Only explicit stop clears that status; subsequent sends/attaches
  // must never silently boot another guest in place of this one.
  if (!ready || session.explicitly_stopped()) {
    try {
      namespace_lock(dir.get());
      remove_socket(dir.get(), id);
      dir.reset();
    } catch (const std::exception&) { result = 125; }
  }
  if (!startup_error.empty()) send_packet(setup.get(), startup_error);
  if (ready) session.finish(result == 0);
  ::_exit(result);
}

template <class Output>
void await_ack(int fd, Output output) {
  const auto deadline = Clock::now() + command_timeout;
  while (!interrupted && remaining(deadline)) {
    pollfd fds{fd, POLLIN, 0};
    if (!poll_retry(&fds, 1, remaining(deadline))) continue;
    std::array<char, packet_limit> buffer;
    const auto n = recv_packet(fd, buffer);
    if (n == -1) continue;
    if (n <= 0) fail("session no longer live; request was not acknowledged");
    const string_view packet(buffer.data(), static_cast<std::size_t>(n));
    if (packet == "A") return;
    if (packet.front() == 'E') throw std::runtime_error(string(packet.substr(1)));
    output(packet);
  }
  fail("session request timed out or was interrupted; delivery unknown, not retried");
}

void no_output(string_view) { fail("unexpected session response"); }

void command(int fd, const string& bytes) {
  if (!send_packet(fd, bytes)) fail("cannot send session request; not retried");
}

Fd create_session(const string& id, const string& executable, const string& guest,
                  const string& initial, bool attach = false) {
  if (executable.empty() || executable.front() != '/' || guest.empty() || guest.front() != '/')
    fail("runner and guest artifact require absolute paths");
  auto dir = session_dir(true);
  namespace_lock(dir.get());
  auto existing = dial(dir.get(), id);
  if (existing.fd) {
    if (attach) return std::move(existing.fd);
    fail("session already live; use connect or send");
  }
  if (existing.stale) fail("session has ended; stop this ID explicitly before restarting");
  const auto image = pin_guest(guest);
  const auto pinned = fd_path(image.get());
  enforce_quota(dir.get());
  auto listener = socket_fd();
  Fd daemon_dir(::openat(dir.get(), ".", dir_flags)); // Independent lock description.
  if (!daemon_dir) system_failure("cannot open namespace for supervisor");
  int setup[2];
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, setup) < 0)
    system_failure("cannot create supervisor status channel");
  Fd report(setup[0]), notify(setup[1]);
  const Address endpoint(fd_path(dir.get()) + "/" + id);
  if (::bind(listener.get(), reinterpret_cast<const sockaddr*>(&endpoint.addr), endpoint.size) < 0)
    system_failure("cannot bind private session socket");
  if (::listen(listener.get(), max_clients) < 0) {
    const int error = errno;
    remove_socket(dir.get(), id);
    errno = error;
    system_failure("cannot listen on private session socket");
  }
  Fd client;
  try {
    if (attach) {
      client = dial(dir.get(), id).fd;
      if (!client) fail("cannot connect to newly created session");
    }
  } catch (...) {
    remove_socket(dir.get(), id);
    throw;
  }
  const auto launcher = ::fork();
  if (launcher < 0) {
    const int error = errno;
    remove_socket(dir.get(), id);
    errno = error;
    system_failure("cannot fork session supervisor");
  }
  if (launcher == 0) {
    const auto daemon = ::fork();
    if (daemon < 0) ::_exit(125);
    // The short-lived launcher is reaped by the client. The supervisor
    // outlives that client without leaving a zombie in a long-lived REPL.
    if (daemon > 0) ::_exit(0);
    dir.reset();
    supervise(std::move(daemon_dir), std::move(listener), std::move(notify), id,
              executable, pinned, image.get(), initial);
  }
  daemon_dir.reset();
  listener.reset();
  notify.reset();
  int state;
  pid_t waited;
  do { waited = ::waitpid(launcher, &state, 0); } while (waited < 0 && errno == EINTR);
  if (waited != launcher || !WIFEXITED(state) || WEXITSTATUS(state) != 0) {
    remove_socket(dir.get(), id);
    fail("cannot start detached session supervisor");
  }
  dir.reset();
  try {
    await_ack(report.get(), no_output);
    command(report.get(), "A");
  } catch (...) {
    // Closing the private handshake when the client fails prevents an
    // unacknowledged supervisor from publishing or keeping a runner.
    report.reset();
    throw;
  }
  return client;
}

Fd open_session(const string& id) {
  const auto dir = session_dir(false);
  if (dir) {
    auto endpoint = dial(dir.get(), id);
    if (endpoint.fd) return std::move(endpoint.fd);
    if (endpoint.stale) fail("session has ended; stop this ID explicitly before restarting");
  }
  fail("no live session for this ID and no verified guest artifact supplied");
}

void stop_session(const string& id) {
  auto dir = session_dir(false);
  if (!dir) return;
  namespace_lock(dir.get());
  auto endpoint = dial(dir.get(), id);
  if (endpoint.stale) remove_socket(dir.get(), id);
  dir.reset();
  if (endpoint.fd) {
    command(endpoint.fd.get(), "Q");
    await_ack(endpoint.fd.get(), no_output);
  }
}

class Terminal {
public:
  explicit Terminal(bool tty) {
    if (!tty) return;
    if (mulib_termutils_raw_begin(&state_, STDIN_FILENO) != 0) fail("cannot start terminal line editing");
    constexpr string_view enable = "\x1b[?2004h";
    if (mulib_termutils_write(1, enable.data(), enable.size(), nullptr) != 0) {
      (void)mulib_termutils_raw_end(&state_);
      fail("cannot enable bracketed paste");
    }
  }
  ~Terminal() {
    if (state_.saved) {
      constexpr string_view disable = "\x1b[?2004l";
      (void)mulib_termutils_write(1, disable.data(), disable.size(), nullptr);
      (void)mulib_termutils_raw_end(&state_);
    }
  }
private:
  mulib_termutils_raw_t state_ = MULIB_TERMUTILS_RAW_INIT;
};

class Console {
public:
  explicit Console(int fd)
      : fd_(fd), tty_(::isatty(0) && ::isatty(1)), terminal_(tty_),
        editor_(mulib_readline_create(&limits_), mulib_readline_destroy) {
    if (!editor_) fail("cannot allocate session line editor");
    if (mulib_readline_completion(editor_.get(), "send", "send one signed decimal i64") ||
        mulib_readline_completion(editor_.get(), ":quit", "disconnect without stopping the guest"))
      fail("cannot configure session line editor");
  }

  void run(const string& initial) {
    command(fd_, "W");
    await_ack(fd_, [this](string_view packet) { frame(packet); });
    if (!initial.empty()) submit(initial);
    render();
    mulib_termkey_t decoder;
    mulib_termkey_init(&decoder);
    while (!quit_ && !interrupted) {
      pollfd fds[] = {{fd_, POLLIN, 0}, {STDIN_FILENO, POLLIN, 0}};
      const auto active = poll_retry(fds, 2, tty_ && decoder.length ? 60 : -1);
      if (!active) {
        if (tty_ && !interrupted) edit(decoder, true);
        continue;
      }
      if (fds[0].revents) {
        std::array<char, packet_limit> bytes;
        const auto n = recv_packet(fd_, bytes);
        if (n == 0 || n == -2) fail("session no longer live");
        if (n > 0) frame(string_view(bytes.data(), static_cast<std::size_t>(n)));
      }
      if (fds[1].revents && !quit_) {
        char c;
        const auto n = ::read(0, &c, 1);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) system_failure("cannot read session console input");
        if (n == 0) {
          if (!tty_ && oversize_) write_display(2, "On1x ArchVirt session: console line too long\n");
          else if (!tty_ && !plain_.empty()) line(plain_);
          break;
        }
        if (tty_) {
          if (!mulib_termkey_feed(&decoder, &c, 1)) fail("terminal key buffer exhausted");
          edit(decoder, false);
        } else if (c == '\n') {
          if (!oversize_) line(plain_);
          else write_display(2, "On1x ArchVirt session: console line too long\n");
          plain_.clear();
          oversize_ = false;
        } else if (plain_.size() < limits_.line_bytes) {
          plain_ += c;
        } else {
          oversize_ = true;
        }
      }
    }
    if (tty_) write_display(1, "\r\x1b[K\r\n");
  }

private:
  void edit(mulib_termkey_t& decoder, bool flush) {
    mulib_termkey_event_t event;
    while (!quit_ && mulib_termkey_next(&decoder, &event, flush) > 0) {
      const auto result = mulib_readline_edit(editor_.get(), &event);
      if (result == MULIB_READLINE_LINE) {
        const string current = mulib_readline_text(editor_.get(), nullptr, nullptr);
        if ((!oversize_ && mulib_readline_history_add(editor_.get(), current.c_str())) ||
            mulib_readline_set(editor_.get(), "", 0)) fail("cannot update line editor history");
        write_display(1, "\r\n");
        if (!oversize_) line(current);
        else write_display(2, "On1x ArchVirt session: console line too long\r\n");
        oversize_ = false;
      } else if (result == MULIB_READLINE_EOF || result == MULIB_READLINE_INTERRUPT) {
        quit_ = true;
      } else if (result == MULIB_READLINE_LIMIT) {
        oversize_ = true;
      } else if (result < 0) {
        fail("session line editor failed");
      }
      if (!quit_) render();
    }
  }

  void line(string_view input) {
    if (input.ends_with('\r')) input.remove_suffix(1);
    const auto first = input.find_first_not_of(" \t");
    if (first == input.npos) return;
    input = input.substr(first, input.find_last_not_of(" \t") - first + 1);
    if (input == ":quit") { quit_ = true; return; }
    if (input == ":help") {
      write_display(1, tty_ ? "send VALUE | :quit (detach) | :help\r\n"
                            : "send VALUE | :quit (detach) | :help\n");
      return;
    }
    string payload;
    try {
      if (!input.starts_with("send ")) fail("expected send VALUE or :quit");
      input.remove_prefix(5);
      const auto at = input.find_first_not_of(" \t");
      if (at == input.npos) fail("expected a signed decimal i64");
      payload = value(input.substr(at));
    } catch (const std::exception& e) {
      write_display(2, "On1x ArchVirt session: " + string(e.what()) + (tty_ ? "\r\n" : "\n"));
      return;
    }
    submit(payload);
  }

  void submit(const string& payload) {
    command(fd_, "S" + payload);
    await_ack(fd_, [this](string_view packet) { frame(packet); });
  }

  void render() {
    if (!tty_) return;
    unsigned columns = 80, rows = 0;
    (void)mulib_termutils_size(1, &columns, &rows);
    if (!columns) columns = 80; // PTYs may report an unset window size.
    char* rendered = nullptr;
    if (mulib_readline_render(editor_.get(), "vmm> ", std::max(9u, columns), &rendered) != 0)
      fail("cannot render session console");
    const std::unique_ptr<char, decltype(&std::free)> text(rendered, std::free);
    write_display(1, text.get());
  }

  void frame(string_view packet) {
    const char kind = packet.front();
    packet.remove_prefix(1);
    if (kind == 'D') throw std::runtime_error(string(packet));
    if (kind != 'O' && kind != 'R') fail("invalid response from local session");
    const int output = kind == 'O' ? 1 : 2;
    if (!tty_ && !::isatty(output)) { write_display(output, packet); return; }
    if (tty_) write_display(1, "\r\x1b[K");
    string escaped;
    for (const unsigned char c : packet) {
      constexpr char hex[] = "0123456789abcdef";
      if (c == '\n') escaped += tty_ ? "\r\n" : "\n";
      else if (c >= 32 && c < 127) escaped += static_cast<char>(c);
      else { escaped += "\\x"; escaped += hex[c >> 4]; escaped += hex[c & 15]; }
    }
    write_display(output, escaped);
    if (tty_) {
      if (!packet.ends_with('\n')) write_display(1, "\r\n");
      render();
    }
  }

  const mulib_readline_limits_t limits_{256, 64, 8192, 16, 1024};
  int fd_;
  bool tty_;
  Terminal terminal_;
  std::unique_ptr<mulib_readline_t, decltype(&mulib_readline_destroy)> editor_;
  bool quit_ = false, oversize_ = false;
  string plain_;
};
}

int main(int argc, char** argv) {
  try {
    if (::getuid() != ::geteuid() || ::getgid() != ::getegid())
      fail("local sessions require a non-set-id process");
    if (argc < 3 || !key_ok(argv[2]))
      fail("usage: on1x-archvirt-session start ID RUNNER GUEST_ELF [--param VALUE ...]\n"
           "       on1x-archvirt-session connect|detach ID [RUNNER GUEST_ELF] [--param VALUE ...]\n"
           "       on1x-archvirt-session send ID --param VALUE ... | stop ID\n"
           "       ID must be 64 lowercase hexadecimal characters");
    for (int stdio = 0; stdio < 3; ++stdio) {
      if (::fcntl(stdio, F_GETFD) < 0 && ::open("/dev/null", O_RDWR | O_NOFOLLOW) != stdio)
        fail("cannot initialize session standard descriptors");
    }
    // Socket mode is 0700 on Linux; the directory is also always 0700.
    ::umask(0077);
    struct sigaction action{};
    action.sa_handler = signal_handler;
    ::sigemptyset(&action.sa_mask);
    for (const int sig : {SIGINT, SIGHUP, SIGTERM}) ::sigaction(sig, &action, nullptr);
    action.sa_handler = SIG_IGN;
    ::sigaction(SIGPIPE, &action, nullptr);
    const string_view op(argv[1]);
    const string id(argv[2]);
    if (op == "start" && argc >= 5) {
      create_session(id, argv[3], argv[4], options(argc, argv, 5));
    } else if (op == "send" && argc >= 5) {
      const auto params = options(argc, argv, 3);
      const auto fd = open_session(id);
      command(fd.get(), "S" + params);
      await_ack(fd.get(), no_output);
    } else if (op == "connect" || op == "detach") {
      const bool image = argc >= 5 && string_view(argv[3]) != "--param";
      const auto params = options(argc, argv, image ? 5 : 3);
      const auto fd = image ? create_session(id, argv[3], argv[4], {}, true) : open_session(id);
      if (op == "connect") {
        Console(fd.get()).run(params);
      } else {
        command(fd.get(), params.empty() ? "P" : "S" + params);
        await_ack(fd.get(), no_output);
      }
    } else if (op == "stop" && argc == 3) {
      stop_session(id);
    } else {
      fail("invalid local session command or arguments");
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "On1x ArchVirt session: " << e.what() << '\n';
    return 1;
  }
}
