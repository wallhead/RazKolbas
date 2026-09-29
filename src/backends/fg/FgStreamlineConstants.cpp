#include "rk/FgStreamlineConstants.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace rk {
namespace {
sl::float4x4 matrix(const std::array<float,16>& values) noexcept {
    static_assert(sizeof(sl::float4x4)==sizeof(values));
    sl::float4x4 out{};
    std::memcpy(&out,values.data(),sizeof(out));
    return out;
}
template<std::size_t N> bool finite(const std::array<float,N>& values) noexcept {
    return std::all_of(values.begin(),values.end(),
        [](float value){return std::isfinite(value);});
}
}
Result<sl::Constants> makeFgStreamlineConstants(
    const FgSourceFrame& frame,const FgCameraData& camera) {
    if(!frame.source||!frame.generation||!frame.presentToken||
       !frame.resetEpoch||!frame.cameraValid||!frame.render.valid()||
       !frame.display.valid()||
       camera.source!=frame.source||camera.generation!=frame.generation||
       camera.presentToken!=frame.presentToken||
       camera.resetEpoch!=frame.resetEpoch||!camera.sampleRevision||
       camera.sampleRevision==UINT64_MAX)
        return Error{ErrorCode::Conflict,
            "FG Streamline constants lack a matching real-frame camera"};
    if(!finite(camera.viewToClip)||!finite(camera.clipToView)||
       !finite(camera.clipToPrevClip)||!finite(camera.prevClipToClip)||
       !finite(camera.position)||!finite(camera.up)||
       !finite(camera.right)||!finite(camera.forward)||
       !finite(camera.jitter)||!finite(camera.mvecScale)||
       camera.mvecScale[0]<=0.0f||camera.mvecScale[1]<=0.0f||
       !std::isfinite(camera.nearPlane)||camera.nearPlane<=0.0f||
       !std::isfinite(camera.farPlane)||camera.farPlane<=camera.nearPlane||
       !std::isfinite(camera.fovRadians)||camera.fovRadians<=0.0f||
       !std::isfinite(camera.aspectRatio)||camera.aspectRatio<=0.0f)
        return Error{ErrorCode::InvalidInput,
            "FG Streamline camera constants are nonfinite or invalid"};
    sl::Constants out{};
    out.cameraViewToClip=matrix(camera.viewToClip);
    out.clipToCameraView=matrix(camera.clipToView);
    out.clipToPrevClip=matrix(camera.clipToPrevClip);
    out.prevClipToClip=matrix(camera.prevClipToClip);
    out.jitterOffset={camera.jitter[0],camera.jitter[1]};
    out.mvecScale={camera.mvecScale[0],camera.mvecScale[1]};
    out.cameraPinholeOffset={0.0f,0.0f};
    out.cameraPos={camera.position[0],camera.position[1],camera.position[2]};
    out.cameraUp={camera.up[0],camera.up[1],camera.up[2]};
    out.cameraRight={camera.right[0],camera.right[1],camera.right[2]};
    out.cameraFwd={camera.forward[0],camera.forward[1],camera.forward[2]};
    out.cameraNear=camera.nearPlane;
    out.cameraFar=camera.farPlane;
    out.cameraFOV=camera.fovRadians;
    out.cameraAspectRatio=camera.aspectRatio;
    out.depthInverted=camera.depthInverted?
        sl::Boolean::eTrue:sl::Boolean::eFalse;
    out.cameraMotionIncluded=camera.cameraMotionIncluded?
        sl::Boolean::eTrue:sl::Boolean::eFalse;
    out.motionVectors3D=sl::Boolean::eFalse;
    out.reset=camera.reset?sl::Boolean::eTrue:sl::Boolean::eFalse;
    return out;
}
}
