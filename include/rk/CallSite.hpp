#pragma once
#include "rk/Result.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace rk {
struct CallSiteDescriptor {
    std::string id;
    std::string gameSha256;
    std::size_t imageSize{};
    std::uint32_t siteRva{}, originalTargetRva{};
    std::array<std::uint8_t,5> expected{};
};
struct CallSitePlan {
    std::string id;
    std::uint32_t siteRva{}, originalTargetRva{};
    std::array<std::uint8_t,5> original{};
};
// Inspects decoded live code only after the caller independently verifies the
// executable file identity. No write, hook installation, or ownership claim.
Result<CallSitePlan> prepareCallSite(std::span<const std::uint8_t> live,
    std::string_view verifiedGameHash,std::size_t imageSize,const CallSiteDescriptor& descriptor);
}
