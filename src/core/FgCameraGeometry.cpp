#include "rk/FgCameraGeometry.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
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
Result<FgCameraCalibration> deriveFgCameraCalibration(
    const FgGameCameraSample& camera) {
    const auto transforms=deriveFgCameraTransforms(camera);
    if(const auto error=std::get_if<Error>(&transforms))return *error;
    const auto& p=camera.projection;
    constexpr double shapeTolerance=1e-4;
    constexpr std::array<std::size_t,11> zeroEntries{
        1,2,3,4,6,7,8,9,12,13,15};
    if(!std::all_of(zeroEntries.begin(),zeroEntries.end(),
            [&](std::size_t index){return std::abs(p[index])<=shapeTolerance;})||
       std::abs(p[14]-1.0f)>shapeTolerance||
       p[0]<=0.0f||p[5]<=0.0f||p[10]<=1.0f||p[11]>=0.0f)
        return Error{ErrorCode::Unsupported,
            "FG game projection is not the measured unjittered forward-Z form"};
    const auto& v=camera.view;
    if(std::abs(v[3])>shapeTolerance||std::abs(v[7])>shapeTolerance||
       std::abs(v[11])>shapeTolerance||std::abs(v[12])>shapeTolerance||
       std::abs(v[13])>shapeTolerance||std::abs(v[14])>shapeTolerance||
       std::abs(v[15]-1.0f)>shapeTolerance)
        return Error{ErrorCode::Unsupported,
            "FG game view includes unmeasured translation or homogeneous terms"};
    FgCameraCalibration out{};
    out.transforms=std::get<FgCameraTransforms>(transforms);
    out.position=camera.position;
    out.right={v[0],v[1],v[2]};
    out.up={v[4],v[5],v[6]};
    out.forward={v[8],v[9],v[10]};
    const std::array<const std::array<float,3>*,3> axes{
        &out.right,&out.up,&out.forward};
    for(std::size_t row=0;row<axes.size();++row) {
        for(std::size_t prior=0;prior<=row;++prior) {
            double dot=0;
            for(std::size_t n=0;n<3;++n)
                dot+=static_cast<double>((*axes[row])[n])*(*axes[prior])[n];
            const double expected=row==prior?1.0:0.0;
            if(!std::isfinite(dot)||std::abs(dot-expected)>1e-3)
                return Error{ErrorCode::InvalidInput,
                    "FG game camera basis is not orthonormal"};
        }
    }
    const double determinant=
        static_cast<double>(v[0])*(v[5]*v[10]-v[6]*v[9])-
        static_cast<double>(v[1])*(v[4]*v[10]-v[6]*v[8])+
        static_cast<double>(v[2])*(v[4]*v[9]-v[5]*v[8]);
    if(!std::isfinite(determinant)||std::abs(determinant+1.0)>1e-3)
        return Error{ErrorCode::Unsupported,
            "FG game camera basis handedness differs from the measured route"};
    const double a=p[10],b=p[11];
    const double nearPlane=-b/a;
    const double farPlane=b/(1.0-a);
    const double verticalFov=2.0*std::atan(1.0/static_cast<double>(p[5]));
    const double horizontalFov=2.0*std::atan(1.0/static_cast<double>(p[0]));
    const double aspect=static_cast<double>(p[5])/p[0];
    if(!std::isfinite(nearPlane)||!std::isfinite(farPlane)||
       !std::isfinite(verticalFov)||!std::isfinite(horizontalFov)||
       !std::isfinite(aspect)||
       nearPlane<=0||farPlane<=nearPlane||
       verticalFov<=0||verticalFov>=3.141592653589793||
       horizontalFov<=0||horizontalFov>=3.141592653589793||aspect<=0||
       farPlane>std::numeric_limits<float>::max())
        return Error{ErrorCode::InvalidInput,
            "FG game projection has invalid frustum parameters"};
    out.nearPlane=static_cast<float>(nearPlane);
    out.farPlane=static_cast<float>(farPlane);
    out.verticalFovRadians=static_cast<float>(verticalFov);
    out.horizontalFovRadians=static_cast<float>(horizontalFov);
    out.aspectRatio=static_cast<float>(aspect);
    return out;
}

Result<FgCameraData> bindFgGameCamera(
    const FgSourceFrame& frame,const FgGameCameraSample& camera,
    NgxJitter sameFrameJitter,std::uint64_t producerRevision,bool reset) {
    if(!frame.source||!frame.generation||!frame.presentToken||
       !frame.resetEpoch||!frame.cameraValid||!frame.worldActive||
       frame.loading||frame.paused||!frame.render.valid()||
       !frame.display.valid()||!producerRevision||
       producerRevision==UINT64_MAX)
        return Error{ErrorCode::InvalidInput,
            "FG camera requires a valid real-frame stamp and producer revision"};
    if(frame.cameraCut&&!reset)
        return Error{ErrorCode::Conflict,
            "FG camera cut requires a temporal reset"};
    constexpr float jitterLimit=0.5001f;
    if(!std::isfinite(sameFrameJitter.x)||
       !std::isfinite(sameFrameJitter.y)||
       std::abs(sameFrameJitter.x)>jitterLimit||
       std::abs(sameFrameJitter.y)>jitterLimit)
        return Error{ErrorCode::InvalidInput,
            "FG same-frame pixel jitter is invalid"};
    const auto calibrated=deriveFgCameraCalibration(camera);
    if(const auto error=std::get_if<Error>(&calibrated))return *error;
    const auto& source=std::get<FgCameraCalibration>(calibrated);
    FgCameraData out{};
    out.source=frame.source;
    out.generation=frame.generation;
    out.presentToken=frame.presentToken;
    out.resetEpoch=frame.resetEpoch;
    out.sampleRevision=producerRevision;
    out.viewToClip=source.transforms.cameraViewToClip;
    out.clipToView=source.transforms.clipToCameraView;
    out.clipToPrevClip=source.transforms.clipToPrevClip;
    out.prevClipToClip=source.transforms.prevClipToClip;
    out.position=source.position;
    out.right=source.right;
    out.up=source.up;
    out.forward=source.forward;
    out.jitter={sameFrameJitter.x,sameFrameJitter.y};
    out.mvecScale={1.0f,1.0f};
    out.nearPlane=source.nearPlane;
    out.farPlane=source.farPlane;
    out.fovRadians=source.verticalFovRadians;
    out.aspectRatio=source.aspectRatio;
    out.depthInverted=false;
    out.cameraMotionIncluded=true;
    out.reset=reset;
    return out;
}
}
