#include "ArtifactInternal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

namespace {

class Sha256 {
public:
    void update(const std::uint8_t *data, std::size_t size) noexcept {
        total_ += size;
        while (size) {
            const auto take = std::min(size, block_.size() - used_);
            std::memcpy(block_.data() + used_, data, take);
            used_ += take;
            data += take;
            size -= take;
            if (used_ == block_.size()) {
                compress(block_.data());
                used_ = 0;
            }
        }
    }

    std::array<std::uint8_t, 32> finish() noexcept {
        const auto bit_size = static_cast<std::uint64_t>(total_) * 8u;
        block_[used_++] = 0x80u;
        if (used_ > 56u) {
            std::fill(block_.begin() + static_cast<std::ptrdiff_t>(used_), block_.end(), 0u);
            compress(block_.data());
            used_ = 0;
        }
        std::fill(block_.begin() + static_cast<std::ptrdiff_t>(used_), block_.begin() + 56, 0u);
        for (unsigned index = 0; index < 8; ++index)
            block_[63u - index] = static_cast<std::uint8_t>(bit_size >> (index * 8u));
        compress(block_.data());

        std::array<std::uint8_t, 32> result{};
        for (std::size_t word = 0; word < state_.size(); ++word)
            for (unsigned byte = 0; byte < 4; ++byte)
                result[word * 4u + byte] =
                    static_cast<std::uint8_t>(state_[word] >> ((3u - byte) * 8u));
        return result;
    }

private:
    static constexpr std::array<std::uint32_t, 64> constants_{
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
        0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
        0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
        0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
        0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
        0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
        0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
        0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
        0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };

    void compress(const std::uint8_t *block) noexcept {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index)
            words[index] = (static_cast<std::uint32_t>(block[index * 4u]) << 24u) |
                           (static_cast<std::uint32_t>(block[index * 4u + 1u]) << 16u) |
                           (static_cast<std::uint32_t>(block[index * 4u + 2u]) << 8u) |
                           static_cast<std::uint32_t>(block[index * 4u + 3u]);
        for (std::size_t index = 16; index < words.size(); ++index) {
            const auto s0 = std::rotr(words[index - 15u], 7) ^
                            std::rotr(words[index - 15u], 18) ^
                            (words[index - 15u] >> 3u);
            const auto s1 = std::rotr(words[index - 2u], 17) ^
                            std::rotr(words[index - 2u], 19) ^
                            (words[index - 2u] >> 10u);
            words[index] = words[index - 16u] + s0 + words[index - 7u] + s1;
        }

        auto [a, b, c, d, e, f, g, h] = state_;
        for (std::size_t index = 0; index < words.size(); ++index) {
            const auto sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const auto choice = (e & f) ^ (~e & g);
            const auto first = h + sum1 + choice + constants_[index] + words[index];
            const auto sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto second = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + first;
            d = c;
            c = b;
            b = a;
            a = first + second;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    std::array<std::uint8_t, 64> block_{};
    std::size_t used_{};
    std::size_t total_{};
};

} // namespace

namespace on1x::sdk::backend {

On1xSDK_Status make_artifact(std::vector<uint8_t> bytes,
                             On1xSDK_ArtifactKind kind,
                             std::uint32_t input_schema,
                             std::uint32_t flags,
                             std::string_view provider_name,
                             std::string_view target_triple,
                             On1xSDK_Artifact **out) noexcept {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    try {
        auto artifact = std::make_unique<On1xSDK_Artifact>();
        artifact->bytes = std::move(bytes);
        artifact->provider_name.assign(provider_name);
        artifact->target_triple.assign(target_triple);
        Sha256 sha;
        if (!artifact->bytes.empty()) sha.update(artifact->bytes.data(), artifact->bytes.size());
        const auto digest = sha.finish();
        artifact->info = On1xSDK_ArtifactInfo{
            ON1X_SDK_ARTIFACT_ABI_VERSION,
            sizeof(On1xSDK_ArtifactInfo),
            kind,
            ON1X_SDK_ARTIFACT_STAGED,
            flags,
            input_schema,
            artifact->provider_name.empty() ? nullptr : artifact->provider_name.data(),
            artifact->provider_name.size(),
            artifact->target_triple.empty() ? nullptr : artifact->target_triple.data(),
            artifact->target_triple.size(),
            {}
        };
        std::copy(digest.begin(), digest.end(), artifact->info.sha256);
        *out = artifact.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

} // namespace on1x::sdk::backend

extern "C" {

On1xSDK_Status on1x_sdk_artifact_bytes(const On1xSDK_Artifact *artifact,
                                       const uint8_t **bytes, size_t *size) {
    if (!bytes || !size) return ON1X_SDK_INVALID_ARGUMENT;
    *bytes = nullptr;
    *size = 0;
    if (!artifact) return ON1X_SDK_INVALID_ARGUMENT;
    *bytes = artifact->bytes.empty() ? nullptr : artifact->bytes.data();
    *size = artifact->bytes.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_artifact_info(const On1xSDK_Artifact *artifact,
                                      const On1xSDK_ArtifactInfo **out_info) {
    if (!out_info) return ON1X_SDK_INVALID_ARGUMENT;
    *out_info = nullptr;
    if (!artifact) return ON1X_SDK_INVALID_ARGUMENT;
    *out_info = &artifact->info;
    return ON1X_SDK_OK;
}

void on1x_sdk_artifact_destroy(On1xSDK_Artifact *artifact) {
    delete artifact;
}

} // extern "C"
