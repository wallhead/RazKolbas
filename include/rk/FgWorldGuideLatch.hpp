#pragma once
#include "rk/FgFrameContract.hpp"
#include "rk/FgRealFrameBoundaries.hpp"
#include "rk/Result.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <mutex>
#include <optional>

namespace rk {
struct FgWorldGuideFrame {
    FgSourceFrame frame;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depth,motion,hudless;
};

// Retains raw world guide candidates across the native UI interval. The
// pre-UI display is copied because Skyrim draws UI into its original target.
// Raw depth/motion still require format and convention conversion. This latch
// must not mark any FG resource stamp ready or claim provider retirement.
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
