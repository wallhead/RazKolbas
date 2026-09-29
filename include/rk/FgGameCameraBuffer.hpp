#pragma once
#include "rk/Result.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace rk {
// Observed subset of a Skyrim 1.6.1170 world-route constant buffer.
// The caller must separately prove the exact game version, frame phase,
// source identity and freshness; this is not a Streamline constants record.
struct FgGameCameraSample {
    std::array<float,16> view{},projection{},currentViewProjection{},
        previousViewProjection{},inverseProjection{},inverseView{},
        inverseViewProjection{};
    std::array<float,3> position{},previousPosition{};
};
Result<FgGameCameraSample> decodeFgGameCameraBuffer(
    std::span<const std::uint8_t> bytes);
bool fgGameCameraConsecutive(const FgGameCameraSample& prior,
    const FgGameCameraSample& next) noexcept;
}
