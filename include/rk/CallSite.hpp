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
// Builds the complete direct-CALL instruction for a verified plan. This is
// preparation only: the caller must separately establish exclusive execution
// quiescence, recheck the live bytes, and own the target's lifetime before write.
Result<std::array<std::uint8_t,5>> encodeCallSiteReplacement(const CallSitePlan& plan,
    std::uintptr_t imageBase,std::uintptr_t detourTarget);
// RIP-relative indirect jump plus an absolute 64-bit target. It preserves
// registers and flags when a process-lifetime relay is needed within rel32
// reach of the CALL. Allocation and executable lifetime remain the caller's.
Result<std::array<std::uint8_t,14>> encodeRegisterPreservingJump(std::uintptr_t target);
}
