#include "rk/FgCameraGeometry.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace rk {
namespace {
using Matrix=std::array<float,16>;
template<std::size_t N> bool finite(const std::array<float,N>& values) noexcept {
    return std::all_of(values.begin(),values.end(),
        [](float value){return std::isfinite(value);});
}
Matrix transpose(const Matrix& input) noexcept {
    Matrix out{};
    for(std::size_t row=0;row<4;++row)
        for(std::size_t column=0;column<4;++column)
            out[row*4+column]=input[column*4+row];
    return out;
}
Matrix multiply(const Matrix& left,const Matrix& right) noexcept {
    Matrix out{};
    for(std::size_t row=0;row<4;++row)
        for(std::size_t column=0;column<4;++column) {
            double value=0;
            for(std::size_t term=0;term<4;++term)
                value+=static_cast<double>(left[row*4+term])*
                    right[term*4+column];
            out[row*4+column]=static_cast<float>(value);
        }
    return out;
}
std::optional<Matrix> invert(const Matrix& input) noexcept {
    double rows[4][8]{};
    for(std::size_t row=0;row<4;++row) {
        for(std::size_t column=0;column<4;++column)
            rows[row][column]=input[row*4+column];
        rows[row][row+4]=1.0;
    }
    for(std::size_t column=0;column<4;++column) {
        std::size_t pivot=column;
        for(std::size_t row=column+1;row<4;++row)
            if(std::abs(rows[row][column])>std::abs(rows[pivot][column]))
                pivot=row;
        if(!std::isfinite(rows[pivot][column])||
           std::abs(rows[pivot][column])<1e-12)return std::nullopt;
        if(pivot!=column)
            for(std::size_t n=0;n<8;++n)
                std::swap(rows[column][n],rows[pivot][n]);
        const double scale=rows[column][column];
        for(double& value:rows[column])value/=scale;
        for(std::size_t row=0;row<4;++row)if(row!=column) {
            const double factor=rows[row][column];
            for(std::size_t n=0;n<8;++n)
                rows[row][n]-=factor*rows[column][n];
        }
    }
    Matrix out{};
    for(std::size_t row=0;row<4;++row)
        for(std::size_t column=0;column<4;++column) {
            const double value=rows[row][column+4];
            if(!std::isfinite(value))return std::nullopt;
            out[row*4+column]=static_cast<float>(value);
        }
    return out;
}
}
Result<FgCameraTransforms> deriveFgCameraTransforms(
    const FgGameCameraSample& camera) {
    if(!finite(camera.view)||!finite(camera.projection)||
       !finite(camera.currentViewProjection)||
       !finite(camera.previousViewProjection)||
       !finite(camera.inverseProjection)||!finite(camera.inverseView)||
       !finite(camera.inverseViewProjection)||!finite(camera.position)||
       !finite(camera.previousPosition))
        return Error{ErrorCode::InvalidInput,
            "FG camera matrices or positions are nonfinite"};
    Matrix translation{};
    for(std::size_t n=0;n<4;++n)translation[n*4+n]=1.0f;
    for(std::size_t n=0;n<3;++n)
        translation[n*4+3]=camera.position[n]-camera.previousPosition[n];
    const Matrix clipToPrevious=multiply(multiply(
        camera.previousViewProjection,translation),
        camera.inverseViewProjection);
    const auto previousToClip=invert(clipToPrevious);
    if(!previousToClip||!finite(clipToPrevious)||!finite(*previousToClip))
        return Error{ErrorCode::InvalidInput,
            "FG camera temporal transform is singular"};
    FgCameraTransforms out{};
    out.cameraViewToClip=transpose(camera.projection);
    out.clipToCameraView=transpose(camera.inverseProjection);
    out.clipToPrevClip=transpose(clipToPrevious);
    out.prevClipToClip=transpose(*previousToClip);
    return out;
}
}
