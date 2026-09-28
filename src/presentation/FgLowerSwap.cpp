#include "rk/FgLowerSwap.hpp"

namespace rk {
Result<FgLowerSwap> FgLowerSwap::create(IDXGISwapChain* swap) {
    if(!swap)return Error{ErrorCode::InvalidInput,"FG lower swap is null"};
    FgLowerSwap lower;
    lower.swap_=swap;
    swap->QueryInterface(IID_PPV_ARGS(&lower.swap1_));
    swap->QueryInterface(IID_PPV_ARGS(&lower.swap3_));
    return lower;
}
HRESULT FgLowerSwap::present(const FgPresentCall& call) const noexcept {
    if(call.method==FgPresentMethod::Present) {
        if(call.parameters)return E_INVALIDARG;
        return swap_->Present(call.interval,call.flags);
    }
    if(call.method!=FgPresentMethod::Present1||!call.parameters)return E_INVALIDARG;
    if(!swap1_)return E_NOINTERFACE;
    return swap1_->Present1(call.interval,call.flags,call.parameters);
}
HRESULT FgLowerSwap::resize(const FgResizeCall& call) const noexcept {
    if(call.method==FgResizeMethod::ResizeBuffers) {
        if(call.creationNodeMask||call.presentQueue)return E_INVALIDARG;
        return swap_->ResizeBuffers(call.buffers,call.width,call.height,
            call.format,call.flags);
    }
    if(call.method!=FgResizeMethod::ResizeBuffers1)return E_INVALIDARG;
    if(!swap3_)return E_NOINTERFACE;
    return swap3_->ResizeBuffers1(call.buffers,call.width,call.height,
        call.format,call.flags,call.creationNodeMask,call.presentQueue);
}
HRESULT FgLowerSwap::queryInterface(REFIID interfaceId,
    void** result) const noexcept {
    return swap_->QueryInterface(interfaceId,result);
}
HRESULT FgLowerSwap::getBuffer(UINT index,REFIID interfaceId,
    void** result) const noexcept {
    return swap_->GetBuffer(index,interfaceId,result);
}
HRESULT FgLowerSwap::getDevice(REFIID interfaceId,
    void** result) const noexcept {
    return swap_->GetDevice(interfaceId,result);
}
HRESULT FgLowerSwap::getDesc(DXGI_SWAP_CHAIN_DESC* result) const noexcept {
    return swap_->GetDesc(result);
}
}
