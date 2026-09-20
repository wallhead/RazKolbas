#pragma once
#include "rk/Result.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace rk {
struct OwnedRouteSite {
    std::string_view id,moduleSha256;
    std::size_t fileSize{};
    std::uint32_t imageSize{},tableRva{},methodRva{};
    unsigned slot{};
    std::array<std::uint8_t,16> prologue{};
};
const OwnedRouteSite& reshade673FactoryCreateSite() noexcept;
const OwnedRouteSite& reshade673SwapGetBufferSite() noexcept;
const OwnedRouteSite& enbContextOmSite() noexcept;
const OwnedRouteSite& enbContextViewportSite() noexcept;
const OwnedRouteSite& enbContextPsResourcesSite() noexcept;
// Mapped image bytes, not disk file layout. The caller separately verifies the
// loaded file's hash and the actual COM object's table address before patching.
Result<bool> validateOwnedRouteSite(std::span<const std::uint8_t> image,
    std::uintptr_t moduleBase,std::string_view verifiedFileHash,
    std::size_t verifiedFileSize,std::uint32_t actualTableRva,
    const OwnedRouteSite& site);
}
