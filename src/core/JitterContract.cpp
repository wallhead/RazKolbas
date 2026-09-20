#include "rk/JitterContract.hpp"
#include <cmath>
#include <cstring>

namespace rk {
Result<NgxJitter> ngxJitterFromGameCamera(std::span<const std::uint8_t> camera,
    std::uint32_t targetWidth,std::uint32_t targetHeight) {
    if(camera.size()<0x4c)
        return Error{ErrorCode::InvalidInput,"Game camera sample is truncated"};
    std::uint32_t width{},height{};
    float projectionX{},projectionY{};
    std::memcpy(&width,camera.data()+0x24,sizeof(width));
    std::memcpy(&height,camera.data()+0x28,sizeof(height));
    std::memcpy(&projectionX,camera.data()+0x44,sizeof(projectionX));
    std::memcpy(&projectionY,camera.data()+0x48,sizeof(projectionY));
    if(!width||!height||width>8192||height>8192||
       width!=targetWidth||height!=targetHeight)
        return Error{ErrorCode::Conflict,"Game camera and DLAA target extents differ"};
    if(!std::isfinite(projectionX)||!std::isfinite(projectionY))
        return Error{ErrorCode::InvalidInput,"Game camera jitter is non-finite"};
    const NgxJitter result{0.5f*static_cast<float>(width)*projectionX,
        -0.5f*static_cast<float>(height)*projectionY};
    constexpr float limit=0.5001f;
    if(!std::isfinite(result.x)||!std::isfinite(result.y)||
       std::abs(result.x)>limit||std::abs(result.y)>limit)
        return Error{ErrorCode::Unsupported,"Game camera jitter is outside one pixel"};
    return result;
}
}
