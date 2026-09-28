#pragma once
#include "rk/FgD3D11PresentBridge.hpp"
#include <dxgi1_5.h>
#include <atomic>

namespace rk {
// Offline COM proof of the V5.4 game-facing swap contract. Resize and wrapper
// interception are deliberately gated until their lifetime tests exist.
class FgD3D11SwapFacade final : public IDXGISwapChain4 {
public:
    static Result<Microsoft::WRL::ComPtr<IDXGISwapChain4>> create(
        ID3D11Device* d11,ID3D11DeviceContext* context,ID3D12Device* d12,
        ID3D12CommandQueue* queue,IDXGISwapChain* lower);
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** result) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID name,UINT size,
        const void* data) override;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID name,
        const IUnknown* unknown) override;
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID name,UINT* size,
        void* data) override;
    HRESULT STDMETHODCALLTYPE GetParent(REFIID iid,void** parent) override;
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID iid,void** device) override;
    HRESULT STDMETHODCALLTYPE Present(UINT interval,UINT flags) override;
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT index,REFIID iid,void** buffer) override;
    HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL fullscreen,
        IDXGIOutput* target) override;
    HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* fullscreen,
        IDXGIOutput** target) override;
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* desc) override;
    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT count,UINT width,UINT height,
        DXGI_FORMAT format,UINT flags) override;
    HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* target) override;
    HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** output) override;
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* stats) override;
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* count) override;
    HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* desc) override;
    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC* desc) override;
    HRESULT STDMETHODCALLTYPE GetHwnd(HWND* window) override;
    HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID iid,void** window) override;
    HRESULT STDMETHODCALLTYPE Present1(UINT interval,UINT flags,
        const DXGI_PRESENT_PARAMETERS* parameters) override;
    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** output) override;
    HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* color) override;
    HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* color) override;
    HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION rotation) override;
    HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* rotation) override;
    HRESULT STDMETHODCALLTYPE SetSourceSize(UINT width,UINT height) override;
    HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* width,UINT* height) override;
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT latency) override;
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* latency) override;
    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override;
    HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* matrix) override;
    HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* matrix) override;
    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override;
    HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE color,
        UINT* support) override;
    HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE color) override;
    HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT count,UINT width,UINT height,
        DXGI_FORMAT format,UINT flags,const UINT* nodeMask,
        IUnknown* const* presentQueue) override;
    HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE type,
        UINT size,void* data) override;
private:
    FgD3D11SwapFacade(ID3D11Device* d11,IDXGISwapChain4* lower,
        std::unique_ptr<FgD3D11PresentBridge> bridge) noexcept;
    ~FgD3D11SwapFacade()=default;
    std::atomic<ULONG> refs_{1};
    Microsoft::WRL::ComPtr<ID3D11Device> d11_;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> lower_;
    std::unique_ptr<FgD3D11PresentBridge> bridge_;
};
}
