#pragma once
#include "rk/FgInputLeaseRing.hpp"
#include "rk/FgUiPlanes.hpp"
#include <array>

namespace rk {
// Row-major, unjittered matrices and motion-vector normalization are
// provider-independent values. The game must supply them from one real frame.
struct FgCameraData {
    // Must be assigned by a verified same-frame camera producer/observation.
    // Simply copying the current frame token onto old bytes is not freshness.
    std::uint64_t source{},generation{},presentToken{},resetEpoch{},
        sampleRevision{};
    std::array<float,16> viewToClip{},clipToView{},clipToPrevClip{},
        prevClipToClip{};
    std::array<float,3> position{},up{},right{},forward{};
    std::array<float,2> jitter{},mvecScale{};
    float nearPlane{},farPlane{},fovRadians{},aspectRatio{};
    bool depthInverted{},cameraMotionIncluded{},reset{};
};
struct FgPreparedSubmission {
    std::uint64_t source{},generation{},presentToken{},resetEpoch{};
    Extent render{},display{};
    std::uint32_t physicalOutputIndex{},swapBufferCount{};
    D3D11_RECT uiRegion{};
    FgCopyTicket copyTicket{};
    FgCameraData camera{};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,5> resources{};
};

// This is deliberately not a Streamline call. The caller still has to prove
// the GPU copy completed, provide an SL token matching presentToken, and
// retire the lease against SL's returned previous-frame input fence.
Result<FgPreparedSubmission> prepareFgSubmission(
    const FgSourceFrame& frame,const FgInputLease& lease,
    const FgUiPlaneFrame& ui,const FgCameraData& camera,
    std::uint32_t physicalOutputIndex,std::uint32_t swapBufferCount);
}
