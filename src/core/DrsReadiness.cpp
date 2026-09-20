#include "rk/DrsReadiness.hpp"
#include <cmath>

namespace rk {
std::optional<EngineDrsTarget> engineDrsTarget(std::uint32_t displayWidth,
    std::uint32_t displayHeight,float widthRatio,float heightRatio) noexcept {
    if(!displayWidth||!displayHeight||displayWidth>8192||displayHeight>8192||
       !std::isfinite(widthRatio)||!std::isfinite(heightRatio)||
       widthRatio<=0.0f||heightRatio<=0.0f||
       widthRatio>1.0f||heightRatio>1.0f)return std::nullopt;
    const auto width=static_cast<std::uint32_t>(std::lround(displayWidth*widthRatio));
    const auto height=static_cast<std::uint32_t>(std::lround(displayHeight*heightRatio));
    if(!width||!height)return std::nullopt;
    return EngineDrsTarget{width,height};
}
bool nativeDlaaRatiosReady(float currentWidth,float currentHeight,
    float previousWidth,float previousHeight) noexcept {
    return std::isfinite(currentWidth)&&std::isfinite(currentHeight)&&
        std::isfinite(previousWidth)&&std::isfinite(previousHeight)&&
        currentWidth==1.0f&&currentHeight==1.0f&&
        previousWidth==1.0f&&previousHeight==1.0f;
}
}
