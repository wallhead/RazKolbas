#pragma once
#include "rk/FrameContracts.hpp"
#include <cstdint>
#include <optional>

namespace rk {
// Layout of Scaleform's GViewport on the verified Skyrim x64 runtime.
struct HudMovieViewport {
    std::int32_t bufferWidth{},bufferHeight{},left{},top{},width{},height{};
    std::int32_t scissorLeft{},scissorTop{},scissorWidth{},scissorHeight{};
    float scale{},aspectRatio{};
    std::uint32_t flags{},padding{};
};
static_assert(sizeof(HudMovieViewport)==0x38);

std::optional<HudMovieViewport> nativeHudMovieViewport(
    const HudMovieViewport& current,Extent render,Extent display) noexcept;
bool sameHudMovieViewport(const HudMovieViewport& left,
    const HudMovieViewport& right) noexcept;
}
