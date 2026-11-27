#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace on1x::archvirt_driver {
struct Guest {
    std::string image;
    std::string archive_sha256;
};
class GuestBuildError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
Guest compile_guest(std::string_view generated);
std::string guest_id(const Guest& guest, bool remember);
std::string linked_program(std::string_view guest, bool always);
int cache_and_run(const Guest& guest);
int register_elf(std::string_view path, bool execute);
int rerun(std::string_view key);
void clear_cache();
void print_build_inputs();
int vmm_command(int argc, char** argv);
}
