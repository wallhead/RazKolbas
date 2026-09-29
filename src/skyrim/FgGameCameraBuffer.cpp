#include "rk/FgGameCameraBuffer.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace rk {
namespace {
constexpr double matrixTolerance=1e-6;
bool projectionViewProduct(const FgGameCameraSample& sample) noexcept {
    for(std::size_t row=0;row<4;++row)
        for(std::size_t column=0;column<4;++column) {
            double expected=0;
            for(std::size_t term=0;term<4;++term)
                expected+=static_cast<double>(sample.projection[row*4+term])*
                    sample.view[term*4+column];
            const double actual=sample.currentViewProjection[row*4+column];
            if(!std::isfinite(expected)||!std::isfinite(actual)||
               std::abs(actual-expected)>
                   matrixTolerance*std::max(1.0,std::abs(expected)))
                return false;
        }
    return true;
}
bool inversePair(const std::array<float,16>& forward,
    const std::array<float,16>& reverse) noexcept {
    for(std::size_t row=0;row<4;++row)
        for(std::size_t column=0;column<4;++column) {
            double actual=0;
            for(std::size_t term=0;term<4;++term)
                actual+=static_cast<double>(forward[row*4+term])*
                    reverse[term*4+column];
            const double expected=row==column?1.0:0.0;
            if(!std::isfinite(actual)||
               std::abs(actual-expected)>matrixTolerance)
                return false;
        }
    return true;
}
template<std::size_t N> bool finite(const std::array<float,N>& values) noexcept {
    return std::all_of(values.begin(),values.end(),
        [](float value){return std::isfinite(value);});
}
}
Result<FgGameCameraSample> decodeFgGameCameraBuffer(
    std::span<const std::uint8_t> bytes) {
    if(bytes.size()!=720)
        return Error{ErrorCode::InvalidInput,"FG game camera buffer has wrong size"};
    FgGameCameraSample result{};
    const auto read=[&](std::size_t offset,auto& value) {
        std::memcpy(value.data(),bytes.data()+offset,sizeof(value));
    };
    read(0x000,result.view);
    read(0x040,result.projection);
    read(0x080,result.currentViewProjection);
    read(0x100,result.previousViewProjection);
    read(0x140,result.inverseProjection);
    read(0x1c0,result.inverseView);
    read(0x200,result.inverseViewProjection);
    read(0x280,result.position);
    read(0x290,result.previousPosition);
    if(!projectionViewProduct(result))
        return Error{ErrorCode::Conflict,
            "FG game camera view-projection is inconsistent"};
    if(!inversePair(result.projection,result.inverseProjection)||
       !inversePair(result.view,result.inverseView)||
       !inversePair(result.currentViewProjection,
                    result.inverseViewProjection))
        return Error{ErrorCode::Conflict,
            "FG game camera inverse matrix is inconsistent"};
    if(!finite(result.previousViewProjection)||!finite(result.position)||
       !finite(result.previousPosition))
        return Error{ErrorCode::Conflict,
            "FG game camera history or position is nonfinite"};
    return result;
}
bool fgGameCameraConsecutive(const FgGameCameraSample& prior,
    const FgGameCameraSample& next) noexcept {
    return next.previousViewProjection==prior.currentViewProjection&&
        next.previousPosition==prior.position;
}
}
