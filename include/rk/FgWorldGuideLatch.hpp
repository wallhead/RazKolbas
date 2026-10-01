#pragma once
#include "rk/FgFrameContract.hpp"
#include "rk/FgRealFrameBoundaries.hpp"
#include "rk/FgCameraFramePairer.hpp"
#include "rk/FgCameraGeometry.hpp"
#include "rk/Result.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <mutex>
#include <optional>

namespace rk {
struct FgWorldGuideFrame {
    FgSourceFrame frame;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depth,motion,hudless;
    std::optional<FgCameraData> cameraCandidate;
};

// Binds a validated producer write and the jitter used by this SR frame to
// the same real-Present packet. The candidate does not set camera readiness.
Result<bool> attachFgWorldCameraCandidate(FgWorldGuideFrame& packet,
    const FgCameraProducerSample& producer,std::uint64_t jitterSource,
    std::uint64_t jitterGeneration,NgxJitter jitter);

// Retains caller-supplied world guide candidates across the native UI interval.
// Depth, motion and the pre-UI display are copied so later game draws cannot
// change their pixels. The caller must establish depth/motion format and
// convention; this latch does not. It must not mark any FG resource stamp
// ready or claim provider retirement.
class FgWorldGuideLatch {
public:
    Result<bool> capture(std::uint64_t source,std::uint64_t generation,
        Extent render,Extent display,ID3D11DeviceContext* context,
        ID3D11Texture2D* depth,ID3D11Texture2D* motion,
        ID3D11Texture2D* displayBeforeUi);
    std::optional<FgWorldGuideFrame> take(const FgBoundarySample& boundary);
    void clear();
private:
    struct Pending {
        std::uint64_t source{},generation{},thread{};
        Extent render{},display{};
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth,motion,hudless;
    };
    std::mutex mutex_;
    Pending pending_{};
    std::uint64_t lastSource_{};
};
}
