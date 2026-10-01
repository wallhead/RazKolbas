#pragma once
#include "rk/FgFrameContract.hpp"
#include "rk/Result.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

namespace rk {
struct FgUiPlaneFrame {
    std::uint64_t source{},generation{},presentToken{},resetEpoch{};
    Extent display{};
    D3D11_RECT uiRegion{};
    FgResourceStamp hudlessStamp{},uiStamp{},finalStamp{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> hudless,uiColorAlpha,finalColor;
};

// Source-only capture boundary. Pre-UI capture may leave presentToken and
// resetEpoch both zero; finish binds them after the same source reaches the
// verified real Present. The caller must prove a complete transparent route.
// Final colour alone cannot recover the UI plane.
class FgUiPlanes {
public:
    FgUiPlanes(ID3D11Device* device,std::uint64_t generation) noexcept;
    Result<bool> captureBeforeUi(const FgSourceFrame& frame,
        ID3D11DeviceContext* context,ID3D11Texture2D* displayBeforeUi);
    Result<FgUiPlaneFrame> finish(const FgSourceFrame& frame,
        ID3D11DeviceContext* context,ID3D11Texture2D* uiColorAlpha,
        ID3D11Texture2D* finalColor,D3D11_RECT uiRegion);
    void discard() noexcept;
    bool advanceGeneration(std::uint64_t next) noexcept;
private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    std::uint64_t generation_{};
    FgUiPlaneFrame pending_{};
    Microsoft::WRL::ComPtr<IUnknown> preUiSource_;
    bool pendingValid_{};
};
}
