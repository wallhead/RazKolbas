#pragma once
#include "rk/Result.hpp"
#include "rk/Settings.hpp"
#include "rk/RenderSizePolicy.hpp"
#include <Windows.h>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rk {
inline constexpr std::string_view drsProbePatchId="skyrim1170.jitter-drs.sr-v1";
// The ratio-only DRS probe is retired after identifying Skyrim's shared
// counter and full-size scene-target allocator. An opt-in request is rejected
// without installing a hook; these accessors remain for the native path.
Result<bool> installDrsProbe(HMODULE game,std::string_view verifiedGameHash,
    const Settings& settings);
void bindDrsDisplay(std::uint32_t width,std::uint32_t height) noexcept;
bool drsProbeActive() noexcept;
bool drsProbeHasRun() noexcept;
// After a rejected probe, restores the native DLAA gate only when the game
// state and all measured scene inputs are back at display resolution.
bool drsProbeConfirmNativeRecovery(Extent colour,Extent motion,Extent depth,
    Extent display) noexcept;
std::optional<Extent> drsProbeRenderExtent() noexcept;
}
