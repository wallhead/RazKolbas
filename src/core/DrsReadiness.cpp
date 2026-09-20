#include "rk/DrsReadiness.hpp"
#include <cmath>

namespace rk {
bool nativeDlaaRatiosReady(float currentWidth,float currentHeight,
    float previousWidth,float previousHeight) noexcept {
    return std::isfinite(currentWidth)&&std::isfinite(currentHeight)&&
        std::isfinite(previousWidth)&&std::isfinite(previousHeight)&&
        currentWidth==1.0f&&currentHeight==1.0f&&
        previousWidth==1.0f&&previousHeight==1.0f;
}
}
