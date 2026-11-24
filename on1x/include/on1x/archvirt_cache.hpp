#pragma once

#include <optional>
#include <string>
#include <string_view>

// Private on1xc integration, not an SDK or guest ABI.
namespace on1x::archvirt_cache {
struct Identity {
  // Versioned guest target/boot profile. Change this when replay compatibility changes.
  std::string profile;
  // Lowercase SHA-256 of the exact guest runtime archive bytes used by the linker.
  std::string archive_sha256;
};

std::string sha256(std::string_view bytes);
bool valid_key(std::string_view key) noexcept;
std::string identify(const Identity& identity, std::string_view elf);

// Publish only a freshly built ELF. There is deliberately no source-to-key lookup.
// Errors (including an unsafe cache location) throw std::exception-derived types.
std::string publish(const Identity& identity, std::string_view elf);

// Load by explicit key using the current guest identity. Returned binary bytes
// have been verified and are independent of concurrent clear/publication.
std::string load(const Identity& identity, std::string_view key);
// A missing key is not a corrupt key. A caller can attach to a live VM
// without the image; corruption or a wrong profile still throws.
std::optional<std::string> find(const Identity& identity, std::string_view key);

// Clear ArchVirt artifacts only. A missing namespace is already clear.
void clear();
}
