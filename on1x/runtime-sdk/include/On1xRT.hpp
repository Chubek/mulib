#ifndef ON1X_RT_HPP
#define ON1X_RT_HPP
#include "On1xRT.h"
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace on1x::rt {
class error : public std::runtime_error {
public:
  const On1xRT_Status status;
  const std::uint32_t code;
  error(On1xRT_Status s, std::uint32_t c, std::string message)
      : std::runtime_error(std::move(message)), status(s), code(c) {}
};

namespace detail {
template <class F> void checked(F operation) {
  struct diagnostic {
    On1xRT_Error *ptr = nullptr;
    ~diagnostic() {
      if (ptr)
        on1x_rt_error_release(&ptr);
    }
  } e;
  auto status = operation(&e.ptr);
  if (status != ON1X_RT_OK) {
    auto msg = on1x_rt_error_message(e.ptr);
    throw error(status, on1x_rt_error_code(e.ptr),
                msg.size ? std::string(msg.data, msg.size)
                         : on1x_rt_status_name(status));
  }
}
} // namespace detail

// Thread-confined owner of one engine, one static module and one instance.
// Provider callbacks and their userdata must outlive the host.
class host {
  On1xRT_Engine *engine_ = nullptr;
  On1xRT_Module module_{};
  On1xRT_Instance instance_{};

public:
  explicit host(const On1xRT_EngineConfig *config = nullptr) {
    detail::checked(
        [&](auto e) { return on1x_rt_engine_create(config, &engine_, e); });
  }
  host(const host &) = delete;
  host &operator=(const host &) = delete;
  host(host &&other) noexcept
      : engine_(std::exchange(other.engine_, nullptr)),
        module_(std::exchange(other.module_, {})),
        instance_(std::exchange(other.instance_, {})) {}
  ~host() noexcept { close(); }

  void register_static(const On1xRT_ModuleDescriptor &descriptor) {
    if (module_.id)
      throw error(ON1X_RT_INVALID_STATE, 0, "host already owns a module");
    detail::checked([&](auto e) {
      return on1x_rt_engine_register_static_module(engine_, &descriptor,
                                                   &module_, e);
    });
  }
  void instantiate() {
    detail::checked([&](auto e) {
      return on1x_rt_instance_create(engine_, module_, &instance_, e);
    });
  }
  std::int64_t call_i64(std::string_view name,
                        std::initializer_list<std::int64_t> args = {}) {
    std::int64_t result;
    detail::checked([&](auto e) {
      return on1x_rt_instance_call_i64(engine_, instance_,
                                       {name.data(), name.size()}, args.begin(),
                                       args.size(), &result, e);
    });
    return result;
  }
  // Failure preserves the remaining owners. Retry explicitly when a provider
  // can fail teardown; an unhandled failure in the destructor terminates.
  void close() {
    if (instance_.id)
      detail::checked([&](auto e) {
        return on1x_rt_instance_release(engine_, &instance_, e);
      });
    if (module_.id)
      detail::checked(
          [&](auto e) { return on1x_rt_module_unload(engine_, &module_, e); });
    if (engine_)
      detail::checked(
          [&](auto e) { return on1x_rt_engine_destroy(&engine_, e); });
  }
};
} // namespace on1x::rt
#endif
