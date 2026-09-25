#pragma once
#include "rk/OwnedSceneDomain.hpp"
#include <Windows.h>
#include <array>
#include <d3d11.h>
#include <optional>
#include <wrl/client.h>

namespace rk {
struct UiContextNext {
    using OM=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,
        ID3D11RenderTargetView* const*,ID3D11DepthStencilView*);
    using VP=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,const D3D11_VIEWPORT*);
    using SC=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,const D3D11_RECT*);
    using PS=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,
        ID3D11ShaderResourceView* const*);
    OM om{};
    VP viewport{};
    SC scissor{};
    PS ps{};
};
struct UiCompatibilityFault {
    UINT targetCount{};
    UINT sceneSlot{};
    bool hasDepth{};
    UINT depthWidth{},depthHeight{};
};
enum class UiObservationKind { RenderTargets, Viewport };
struct UiObservationEvent {
    UiObservationKind kind{UiObservationKind::RenderTargets};
    UINT targetCount{};
    int sceneSlot{-1};
    bool hasDepth{};
    Extent depth{},viewport{};
    std::array<Extent,4> targets{};
    std::array<std::uintptr_t,4> targetIdentities{};
    std::uintptr_t depthIdentity{};
};
struct UiFrameObservation {
    std::uint64_t frame{};
    std::uint32_t count{},dropped{};
    std::uint32_t sampledDepthReads{},otherSingletonReads{};
    UINT firstSampledDepthSlot{~0u};
    std::array<UiObservationEvent,64> events{};
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
    // Read-only bounded trace between the semantic menu marker and Present.
    // It records only binds that contain the owned reduced scene and their
    // immediately following viewport; forwarding is unchanged.
    bool beginObservation(std::uint64_t frame) noexcept;
    std::optional<UiFrameObservation> finishObservation(
        std::uint64_t frame) noexcept;
    // Allocate display-sized counterparts for the reduced auxiliary colour
    // and depth resources learned by the read-only menu trace. Allocation is
    // kept outside the context callbacks.
    HRESULT prepareObservedCompanions() noexcept;
    bool companionsReady() const noexcept;
    bool latePassRoutingAvailable() const noexcept {
        return !latePassRoutingDisabled_&&companionsReady();
    }
    // Preserve the owned reduced-scene route but permanently stop translating
    // late passes after a runtime contract mismatch. The controller then
    // returns to its proven pre-Present publication path.
    void disableLatePassRouting() noexcept {
        latePassRoutingDisabled_=true;compatibilityFault_=false;faultInfo_={};
    }
    void onOMSetRenderTargets(ID3D11DeviceContext* context,UINT count,
        ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth) noexcept;
    void onRSSetViewports(ID3D11DeviceContext* context,UINT count,
        const D3D11_VIEWPORT* views) noexcept;
    void onRSSetScissorRects(ID3D11DeviceContext* context,UINT count,
        const D3D11_RECT* rects) noexcept;
    // Trace the exact singleton original-depth read used by the verified ENB
    // context. During NativeUi, only that resource is replaced by its
    // display-sized sampled-depth companion.
    void onPSSetShaderResources(ID3D11DeviceContext* context,UINT start,UINT count,
        ID3D11ShaderResourceView* const* views) noexcept;
    bool compatibilityFault() const noexcept { return compatibilityFault_; }
    UiCompatibilityFault compatibilityFaultInfo() const noexcept { return faultInfo_; }
    void releaseAfterRetirement(bool unbindNative=false) noexcept;
private:
    struct AuxiliaryCompanion {
        Microsoft::WRL::ComPtr<IUnknown> sourceId;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sourceView;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> nativeView;
    };
    bool eligible(ID3D11DeviceContext* context) const noexcept;
    bool nativeBound() const noexcept;
    void bindNativeTarget(bool bindUiDepth) noexcept;
    bool observationMatchesRoute() const noexcept;
    void rememberObservedCompanions(UINT count,
        ID3D11RenderTargetView* const* views,ID3D11DepthStencilView* depth,
        int sceneSlot) noexcept;
    ID3D11RenderTargetView* auxiliaryReplacement(IUnknown* sourceId) const noexcept;
    OwnedSceneDomain& route_;
    UiContextNext next_{};
    DWORD thread_{};
    std::uint64_t generation_{};
    bool compatibilityFault_{};
    bool latePassRoutingDisabled_{};
    UiCompatibilityFault faultInfo_{};
    UiFrameObservation observation_{};
    bool observing_{},observeViewport_{};
    std::uint32_t completedObservations_{};
    std::uint32_t validRouteObservations_{};
    bool observationContractFault_{};
    bool observedMrtDepth_{},observedSingleDepth_{};
    std::array<AuxiliaryCompanion,4> auxiliaries_{};
    Microsoft::WRL::ComPtr<IUnknown> depthSourceId_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthSourceView_,nativeDepthView_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSourceShaderView_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> nativeSampledDepthView_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> nativeSampledDepthClearView_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> scene_;
    Microsoft::WRL::ComPtr<IUnknown> sceneId_,nativeId_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> nativeRtv_;
};
}
