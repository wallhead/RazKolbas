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
// Experimental engine DRS probe, installed only at SKSE startup. The
// default settings keep the callback an exact pass-through.
Result<bool> installDrsProbe(HMODULE game,std::string_view verifiedGameHash,
    const Settings& settings);
void bindDrsDisplay(std::uint32_t width,std::uint32_t height) noexcept;
bool drsProbeActive() noexcept;
bool drsProbeHasRun() noexcept;
std::optional<Extent> drsProbeRenderExtent() noexcept;
}
