#include "rk/HudMovieViewport.hpp"
#include <limits>

namespace rk {
std::optional<HudMovieViewport> nativeHudMovieViewport(
    const HudMovieViewport& current,Extent render,Extent display) noexcept {
    constexpr auto max=static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    if(!render.valid()||!display.valid()||
       display.width<render.width||display.height<render.height||
       (display.width==render.width&&display.height==render.height)||
       render.width>max||render.height>max||
       display.width>max||display.height>max||
       current.bufferWidth!=static_cast<std::int32_t>(render.width)||
       current.bufferHeight!=static_cast<std::int32_t>(render.height)||
       current.left!=0||current.top!=0||
       current.width!=static_cast<std::int32_t>(render.width)||
       current.height!=static_cast<std::int32_t>(render.height)||
       (current.flags&4u)!=0||
       current.scissorLeft!=0||current.scissorTop!=0||
       current.scissorWidth!=0||current.scissorHeight!=0)
        return std::nullopt;
    auto result=current;
    result.bufferWidth=static_cast<std::int32_t>(display.width);
    result.bufferHeight=static_cast<std::int32_t>(display.height);
    result.width=static_cast<std::int32_t>(display.width);
    result.height=static_cast<std::int32_t>(display.height);
    return result;
}
bool sameHudMovieViewport(const HudMovieViewport& left,
    const HudMovieViewport& right) noexcept {
    return left.bufferWidth==right.bufferWidth&&
        left.bufferHeight==right.bufferHeight&&left.left==right.left&&
        left.top==right.top&&left.width==right.width&&
        left.height==right.height&&
        left.scissorLeft==right.scissorLeft&&
        left.scissorTop==right.scissorTop&&
        left.scissorWidth==right.scissorWidth&&
        left.scissorHeight==right.scissorHeight&&
        left.scale==right.scale&&left.aspectRatio==right.aspectRatio&&
        left.flags==right.flags;
}
}
