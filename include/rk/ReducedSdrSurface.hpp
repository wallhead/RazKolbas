#pragma once
#include "rk/FrameContracts.hpp"
#include "rk/Result.hpp"
#include <d3d11.h>
#include <wrl/client.h>

namespace rk {
// Offscreen render-sized colour. It is not a swap-chain buffer and does not
// change Skyrim's renderer until a separately verified integration owns it.
class ReducedSdrSurface final {
public:
    ReducedSdrSurface()=default;
    ReducedSdrSurface(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view,
        Extent render,Extent display) noexcept;
    ReducedSdrSurface(const ReducedSdrSurface&)=delete;
    ReducedSdrSurface& operator=(const ReducedSdrSurface&)=delete;
    ReducedSdrSurface(ReducedSdrSurface&&) noexcept=default;
    ReducedSdrSurface& operator=(ReducedSdrSurface&&) noexcept=default;
    ID3D11Texture2D* texture() const noexcept { return texture_.Get(); }
    ID3D11RenderTargetView* renderTarget() const noexcept { return view_.Get(); }
    // For a future controlled swap-chain GetBuffer route. The caller validates
    // the buffer index and generation before exposing this stable identity.
    HRESULT queryBuffer(REFIID iid,void** output) const noexcept;
    Extent renderExtent() const noexcept { return render_; }
    Extent displayExtent() const noexcept { return display_; }
private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view_;
    Extent render_{},display_{};
};

Result<ReducedSdrSurface> createReducedSdrSurface(ID3D11Device* device,
    Extent display,Extent render);
}
