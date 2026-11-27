#pragma once

#include <on1x/diagnostic.hpp>
#include <memory>

namespace on1x::diagnostic {

// Presentation only: never use this to decide whether a stage succeeded.
class PresentationFilter {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    static constexpr std::size_t max_selectors = 256;
    PresentationFilter();
    ~PresentationFilter();
    void set(std::string_view selector, bool enabled);
    bool active() const noexcept { return bool(impl_); }
    bool visible(Severity, std::string_view name = {}, std::string_view group = {}) const;
    bool visible(const Record&) const;
};

} // namespace on1x::diagnostic
