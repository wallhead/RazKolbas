#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

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
// Emits once after two consecutive world frames have the same valid four-ratio
// tuple. A changed tuple starts a new diagnostic generation.
class StableDrsTupleGate {
public:
    std::optional<std::uint64_t> observe(std::array<float,4> ratios) noexcept;
private:
    std::array<float,4> last_{};
    std::uint64_t generation_{};
    unsigned consecutive_{};
    bool hasTuple_{},emitted_{};
};
// A conservative one-frame check for a reduced SDR image in the top-left
// rectangle of a display-sized allocation. Rejects a detailed image outside
// the rectangle, which would indicate that the SDR source was already enlarged.
bool reducedSdrRegionLooksUnscaled(std::span<const std::uint8_t> rgba,
    std::uint32_t displayWidth,std::uint32_t displayHeight,std::size_t rowBytes,
    std::uint32_t renderWidth,std::uint32_t renderHeight) noexcept;
}
