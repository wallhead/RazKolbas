#pragma once
#include "rk/OwnedSceneDomain.hpp"
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace rk {
struct UiContextNext {
    using OM=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,
        ID3D11RenderTargetView* const*,ID3D11DepthStencilView*);
    using VP=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,const D3D11_VIEWPORT*);
    OM om{};
    VP viewport{};
};
// Scoped translations for a single verified immediate-context chain. The
// installer must save the current downstream methods; this class never jumps
// around ENB/ReShade or installs a vtable hook on its own.
class NativeUiRedirector final {
public:
    explicit NativeUiRedirector(OwnedSceneDomain& route) noexcept:route_(route) {}
    HRESULT configure(ID3D11DeviceContext* context,DWORD renderThread,
        UiContextNext next,ID3D11Texture2D* reducedScene,
        ID3D11RenderTargetView* nativeRtv) noexcept;
    // Select the native flip buffer for this frame before UI publication.
    HRESULT replaceNativeTarget(ID3D11RenderTargetView* nativeRtv) noexcept;
    // Move output binding to the current native target before DLSS publication
    // or spatial fallback, while retaining the Processing phase.
    HRESULT bindNativeForProcessing(std::uint64_t frame) noexcept;
    // Call only after a valid same-frame SR result or spatial fallback has
    // actually been published to the native output and state scopes retired.
    HRESULT commitPublishedUi(std::uint64_t frame) noexcept;
    void onOMSetRenderTargets(ID3D11DeviceContext* context,UINT count,
        ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth) noexcept;
    void onRSSetViewports(ID3D11DeviceContext* context,UINT count,
        const D3D11_VIEWPORT* views) noexcept;
    bool compatibilityFault() const noexcept { return compatibilityFault_; }
    void releaseAfterRetirement(bool unbindNative=false) noexcept;
private:
    bool eligible(ID3D11DeviceContext* context) const noexcept;
    bool nativeBound() const noexcept;
    void bindNativeTarget() noexcept;
    OwnedSceneDomain& route_;
    UiContextNext next_{};
    DWORD thread_{};
    std::uint64_t generation_{};
    bool compatibilityFault_{};
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> scene_;
    Microsoft::WRL::ComPtr<IUnknown> sceneId_,nativeId_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> nativeRtv_;
};
}
