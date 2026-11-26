#pragma once

#include <cstddef>

namespace on1x {

enum class DeclarationVisibility { Unspecified, Public, Shared, Private };

// Unspecified records the absence of an annotation, not an explicit public
// annotation. Interface matching needs to distinguish the two.
struct VisibilityRule {
    DeclarationVisibility kind = DeclarationVisibility::Unspecified;
    std::size_t elevation = 0;
};

} // namespace on1x
