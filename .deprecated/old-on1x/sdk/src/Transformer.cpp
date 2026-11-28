#include "On1xSDK-Transformer.h"
#include "FacetInternal.hpp"

#include <algorithm>

extern "C" {

On1xSDK_Status on1x_sdk_output_write(On1xSDK_OutputSink *sink, const void *bytes, size_t size) {
    if (!sink) return ON1X_SDK_INVALID_ARGUMENT;
    if (sink->status != ON1X_SDK_OK || size == 0) return sink->status;
    if (!bytes) return sink->status = ON1X_SDK_INVALID_ARGUMENT;
    if (size > sink->limit - sink->bytes.size()) return sink->status = ON1X_SDK_LIMIT;
    return sink->status = on1x::sdk::guard([&] {
        const auto offset = sink->bytes.size();
        const auto needed = offset + size;
        const auto capacity = sink->bytes.capacity();
        if (needed > capacity)
            sink->bytes.reserve(std::max(needed, std::min(sink->limit, capacity * 2)));
        sink->bytes.resize(needed);
        std::memcpy(sink->bytes.data() + offset, bytes, size);
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_transformer_register(On1xSDK_Registry *registry,
                                             const On1xSDK_Transformer *transformer) {
    return on1x::sdk::register_facet(registry, on1x::sdk::Kind::Transformer, transformer, true, false);
}

On1xSDK_Status on1x_sdk_transformer_run(const On1xSDK_Registry *registry, const char *name,
                                       const On1xSDK_Program *program,
                                       size_t byte_limit, On1xSDK_Program **out) {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    using namespace on1x::sdk;
    return run_bytes<On1xSDK_Transformer>(registry, Kind::Transformer, name, program,
                                         byte_limit, ON1X_SDK_MAX_PROGRAM_BYTES, [&](auto &&bytes) {
        *out = new On1xSDK_Program{ON1X_SDK_SOURCE, std::move(bytes)};
        return ON1X_SDK_OK;
    });
}

} // extern "C"
