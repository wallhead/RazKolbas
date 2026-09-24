#pragma once
#include "rk/CallSite.hpp"
#include "rk/Result.hpp"
#include "rk/Settings.hpp"
#include <Windows.h>
#include <cstdint>
#include <span>
#include <string_view>

namespace rk {
inline constexpr std::string_view menuInputDispatchPatchId =
    "skyrim1170.menu-input-dispatch-v1";

Result<bool> verifySkyrim1170InputDispatchCall(
    std::span<const std::uint8_t> caller) noexcept;
struct ChainedInputCall {
    std::uintptr_t syntheticBase{};
    std::uintptr_t priorTarget{};
    CallSitePlan plan;
};
Result<ChainedInputCall> prepareChainedInputCall(std::uintptr_t site,
    std::span<const std::uint8_t> call) noexcept;
void* const* selectMenuInputEvents(bool capture,
    void* const* events) noexcept;
Result<bool> installMenuInputDispatchHook(HMODULE game,
    std::string_view verifiedGameHash,const Settings& settings);
}
