#pragma once
#include "rk/Result.hpp"
#include "rk/Settings.hpp"
#include <Windows.h>
#include <cstdint>
#include <string_view>

namespace rk {
inline constexpr std::string_view worldDrawPatchId="skyrim1170.world-draw.sr-v1";
// Called only at SKSEPlugin_Load, before any renderer/world-draw execution.
// An installed hook and relay remain process-lifetime; no hot unpatch.
Result<bool> installWorldDrawPassThrough(HMODULE game,std::string_view verifiedGameHash,
    const Settings& settings);
std::uint64_t worldDrawForwardedCalls() noexcept;
}
