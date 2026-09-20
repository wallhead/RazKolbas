#pragma once
#include <cstdint>
#include <optional>

namespace rk {
struct EngineDrsTarget { std::uint32_t width{},height{}; };
// Ratio-derived target for diagnostics. It is not proof of a valid SR input.
std::optional<EngineDrsTarget> engineDrsTarget(std::uint32_t displayWidth,
    std::uint32_t displayHeight,float widthRatio,float heightRatio) noexcept;
// A native-resolution DLAA submission needs a native current and previous
// engine DRS ratio. This is only a ratio gate; guide extents are checked at
// the D3D11 source boundary.
bool nativeDlaaRatiosReady(float currentWidth,float currentHeight,
    float previousWidth,float previousHeight) noexcept;
}
