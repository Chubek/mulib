#ifndef ON1X_SDK_CORE_INTERNAL_HPP
#define ON1X_SDK_CORE_INTERNAL_HPP

#include "On1xSDK-Capability.h"
#include "On1xSDK-Context.h"
#include "On1xSDK-Handle.h"
#include "On1xSDK-Host.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct On1xSDK_CapabilitySet {
    struct Grant {
        std::string name;
        std::uint32_t revision = 0;
        std::uint32_t flags = 0;
    };
    std::vector<Grant> grants;
};

struct On1xSDK_Context {
    struct HandleSlot {
        std::uint32_t generation = 1;
        std::uint32_t references = 0;
        std::uint32_t kind = ON1X_SDK_HANDLE_KIND_NONE;
        std::uint32_t state = ON1X_SDK_HANDLE_STATE_CLOSED;
        void *object = nullptr;
        On1xSDK_HandleDropFn drop = nullptr;
        void *drop_userdata = nullptr;
        bool retired = false;
    };

    std::atomic<std::uint32_t> references{1};
    std::uint64_t owner = 0;
    On1xSDK_Allocator allocator{};
    On1xSDK_DiagnosticSinkV2 diagnostics{};
    On1xSDK_LogFn log = nullptr;
    void *log_userdata = nullptr;
    On1xSDK_SubmitCandidateFn submit_candidate = nullptr;
    void *submit_userdata = nullptr;
    std::uint32_t max_handles = 0;
    std::thread::id handle_thread = std::this_thread::get_id();
    std::mutex handle_mutex;
    std::vector<HandleSlot> handles;

    struct HostToken {
        On1xSDK_Context *context = nullptr;
        const On1xSDK_CapabilitySet *capabilities = nullptr;
    } base_host_token;
    On1xSDK_Host host{};
};

namespace on1x::sdk::v2 {

On1xSDK_Status normalize_status(On1xSDK_Status status) noexcept;
bool valid_state(std::uint32_t state) noexcept;

void make_host(
    On1xSDK_Context *context,
    const On1xSDK_CapabilitySet *capabilities,
    On1xSDK_Context::HostToken &token,
    On1xSDK_Host &host
) noexcept;

} // namespace on1x::sdk::v2

#endif
