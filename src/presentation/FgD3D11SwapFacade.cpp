#include "rk/FgD3D11SwapFacade.hpp"
#include <variant>

namespace rk {
FgD3D11SwapFacade::FgD3D11SwapFacade(ID3D11Device* d11,
    IDXGISwapChain4* lower,
    std::unique_ptr<FgD3D11PresentBridge> bridge) noexcept:
    d11_(d11),lower_(lower),bridge_(std::move(bridge)) {}
Result<Microsoft::WRL::ComPtr<IDXGISwapChain4>> FgD3D11SwapFacade::create(
    ID3D11Device* d11,ID3D11DeviceContext* context,ID3D12Device* d12,
    ID3D12CommandQueue* queue,IDXGISwapChain* lower,
    ID3D12Device* verifiedLowerNative,
    std::unique_ptr<FgD3D11AuxSwapSource> auxiliary) {
    if(!lower)return Error{ErrorCode::InvalidInput,"FG facade lower swap is null"};
    Microsoft::WRL::ComPtr<IDXGISwapChain4> lower4;
    if(FAILED(lower->QueryInterface(IID_PPV_ARGS(&lower4))))
        return Error{ErrorCode::Unsupported,"FG facade requires lower IDXGISwapChain4"};
    auto made=FgD3D11PresentBridge::create(d11,context,d12,queue,lower,
        verifiedLowerNative,std::move(auxiliary));
    if(!std::holds_alternative<std::unique_ptr<FgD3D11PresentBridge>>(made))
        return std::get<Error>(std::move(made));
    Microsoft::WRL::ComPtr<IDXGISwapChain4> facade;
    facade.Attach(new FgD3D11SwapFacade(d11,lower4.Get(),
        std::move(std::get<std::unique_ptr<FgD3D11PresentBridge>>(made))));
    return facade;
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::QueryInterface(REFIID iid,
    void** result) {
    if(!result)return E_POINTER;
    *result=nullptr;
    if(iid==__uuidof(IUnknown)||iid==__uuidof(IDXGIObject)||
       iid==__uuidof(IDXGIDeviceSubObject)||iid==__uuidof(IDXGISwapChain)||
       iid==__uuidof(IDXGISwapChain1)||iid==__uuidof(IDXGISwapChain2)||
       iid==__uuidof(IDXGISwapChain3)||iid==__uuidof(IDXGISwapChain4)) {
        *result=static_cast<IDXGISwapChain4*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE FgD3D11SwapFacade::AddRef() {
    return refs_.fetch_add(1,std::memory_order_relaxed)+1;
}
ULONG STDMETHODCALLTYPE FgD3D11SwapFacade::Release() {
    const auto count=refs_.fetch_sub(1,std::memory_order_acq_rel)-1;
    if(!count)delete this;
    return count;
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetPrivateData(REFGUID name,
    UINT size,const void* data) { return lower_->SetPrivateData(name,size,data); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetPrivateDataInterface(REFGUID name,
    const IUnknown* unknown) {
    return lower_->SetPrivateDataInterface(name,unknown);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetPrivateData(REFGUID name,
    UINT* size,void* data) { return lower_->GetPrivateData(name,size,data); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetParent(REFIID iid,
    void** parent) { return lower_->GetParent(iid,parent); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetDevice(REFIID iid,
    void** device) {
    if(!device)return E_POINTER;
    *device=nullptr;
    return d11_->QueryInterface(iid,device);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::Present(UINT interval,UINT flags) {
    const FgPresentCall call{FgPresentMethod::Present,interval,flags};
    if(!(flags&DXGI_PRESENT_TEST)) {
        const auto hr=bridge_->copyToCurrent();
        if(FAILED(hr))return hr;
    }
    return bridge_->presentPrepared(call);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetBuffer(UINT index,REFIID iid,
    void** buffer) {
    if(!buffer)return E_POINTER;
    *buffer=nullptr;
    auto* texture=bridge_->renderBuffer(index);
    return texture?texture->QueryInterface(iid,buffer):DXGI_ERROR_INVALID_CALL;
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetFullscreenState(BOOL fullscreen,
    IDXGIOutput* target) { return lower_->SetFullscreenState(fullscreen,target); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetFullscreenState(BOOL* fullscreen,
    IDXGIOutput** target) { return lower_->GetFullscreenState(fullscreen,target); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetDesc(DXGI_SWAP_CHAIN_DESC* desc) {
    return lower_->GetDesc(desc);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::ResizeBuffers(UINT count,
    UINT width,UINT height,DXGI_FORMAT format,UINT flags) {
    return bridge_->resize({FgResizeMethod::ResizeBuffers,count,width,height,
        format,flags});
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::ResizeTarget(
    const DXGI_MODE_DESC* target) { return lower_->ResizeTarget(target); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetContainingOutput(
    IDXGIOutput** output) { return lower_->GetContainingOutput(output); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetFrameStatistics(
    DXGI_FRAME_STATISTICS* stats) { return lower_->GetFrameStatistics(stats); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetLastPresentCount(UINT* count) {
    return lower_->GetLastPresentCount(count);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetDesc1(DXGI_SWAP_CHAIN_DESC1* desc) {
    return lower_->GetDesc1(desc);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetFullscreenDesc(
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC* desc) { return lower_->GetFullscreenDesc(desc); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetHwnd(HWND* window) {
    return lower_->GetHwnd(window);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetCoreWindow(REFIID iid,
    void** window) { return lower_->GetCoreWindow(iid,window); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::Present1(UINT interval,UINT flags,
    const DXGI_PRESENT_PARAMETERS* parameters) {
    if(!parameters)return E_INVALIDARG;
    const FgPresentCall call{FgPresentMethod::Present1,interval,flags,parameters};
    if(!(flags&DXGI_PRESENT_TEST)) {
        const auto hr=bridge_->copyToCurrent();
        if(FAILED(hr))return hr;
    }
    return bridge_->presentPrepared(call);
}
BOOL STDMETHODCALLTYPE FgD3D11SwapFacade::IsTemporaryMonoSupported() {
    return lower_->IsTemporaryMonoSupported();
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetRestrictToOutput(
    IDXGIOutput** output) { return lower_->GetRestrictToOutput(output); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetBackgroundColor(
    const DXGI_RGBA* color) { return lower_->SetBackgroundColor(color); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetBackgroundColor(
    DXGI_RGBA* color) { return lower_->GetBackgroundColor(color); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetRotation(
    DXGI_MODE_ROTATION rotation) { return lower_->SetRotation(rotation); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetRotation(
    DXGI_MODE_ROTATION* rotation) { return lower_->GetRotation(rotation); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetSourceSize(UINT width,UINT height) {
    return lower_->SetSourceSize(width,height);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetSourceSize(UINT* width,
    UINT* height) { return lower_->GetSourceSize(width,height); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetMaximumFrameLatency(UINT latency) {
    return lower_->SetMaximumFrameLatency(latency);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetMaximumFrameLatency(
    UINT* latency) { return lower_->GetMaximumFrameLatency(latency); }
HANDLE STDMETHODCALLTYPE FgD3D11SwapFacade::GetFrameLatencyWaitableObject() {
    return lower_->GetFrameLatencyWaitableObject();
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetMatrixTransform(
    const DXGI_MATRIX_3X2_F* matrix) { return lower_->SetMatrixTransform(matrix); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::GetMatrixTransform(
    DXGI_MATRIX_3X2_F* matrix) { return lower_->GetMatrixTransform(matrix); }
UINT STDMETHODCALLTYPE FgD3D11SwapFacade::GetCurrentBackBufferIndex() {
    return bridge_->currentIndex();
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::CheckColorSpaceSupport(
    DXGI_COLOR_SPACE_TYPE color,UINT* support) {
    return lower_->CheckColorSpaceSupport(color,support);
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetColorSpace1(
    DXGI_COLOR_SPACE_TYPE color) { return lower_->SetColorSpace1(color); }
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::ResizeBuffers1(UINT count,
    UINT width,UINT height,DXGI_FORMAT format,UINT flags,
    const UINT* nodeMask,IUnknown* const* presentQueue) {
    return bridge_->resize({FgResizeMethod::ResizeBuffers1,count,width,height,
        format,flags,nodeMask,presentQueue});
}
HRESULT STDMETHODCALLTYPE FgD3D11SwapFacade::SetHDRMetaData(
    DXGI_HDR_METADATA_TYPE type,UINT size,void* data) {
    return lower_->SetHDRMetaData(type,size,data);
}
}
