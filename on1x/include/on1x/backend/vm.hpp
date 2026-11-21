#pragma once
#include <on1x/backend/mir.hpp>

namespace on1x::vm {
inline constexpr std::size_t max_artifact_bytes = 16 * 1024 * 1024;
std::string compile(const mir::Module&);
void verify(std::string_view bytes);
mir::Evaluation run(std::string_view bytes, std::string_view function = "main",
                    const std::vector<mir::RuntimeValue>& arguments = {},
                    std::size_t steps = 1000000, std::size_t call_depth = 256);
// CLI/process entry rejects non-process signatures before executing any effects.
mir::Evaluation run_main(std::string_view bytes, std::size_t steps = 1000000,
                         std::size_t call_depth = 256);
}
