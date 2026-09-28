#pragma once
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <atomic>
#include <functional>
#include <utility>

namespace rk_test {
// Test-only lower-chain observer. The callback runs before the native/proxy
// Present can discard its backbuffer. Keep callbacks no-throw: production
// Present implementations may be noexcept.
class FgObservedSwap final : public IDXGISwapChain4 {
public:
    FgObservedSwap(IDXGISwapChain4* lower,std::function<HRESULT(UINT)> observe):
        lower_(lower),observe_(std::move(observe)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;
        *out=nullptr;
        if(iid==__uuidof(IUnknown)||iid==__uuidof(IDXGIObject)||
           iid==__uuidof(IDXGIDeviceSubObject)||iid==__uuidof(IDXGISwapChain)||
           iid==__uuidof(IDXGISwapChain1)||iid==__uuidof(IDXGISwapChain2)||
           iid==__uuidof(IDXGISwapChain3)||iid==__uuidof(IDXGISwapChain4)) {
            *out=static_cast<IDXGISwapChain4*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto left=--refs_;
        if(!left)delete this;
        return left;
    }
    HRESULT STDMETHODCALLTYPE Present(UINT interval,UINT flags) override {
        if(!(flags&DXGI_PRESENT_TEST)&&observe_) {
            const auto observed=observe_(lower_->GetCurrentBackBufferIndex());
            if(FAILED(observed))return observed;
        }
        return lower_->Present(interval,flags);
    }
    HRESULT STDMETHODCALLTYPE Present1(UINT interval,UINT flags,
        const DXGI_PRESENT_PARAMETERS* params) override {
        if(!(flags&DXGI_PRESENT_TEST)&&observe_) {
            const auto observed=observe_(lower_->GetCurrentBackBufferIndex());
            if(FAILED(observed))return observed;
        }
        return lower_->Present1(interval,flags,params);
    }
#define RK_TEST_FORWARD_HR(name,decl,args) HRESULT STDMETHODCALLTYPE name decl override { return lower_->name args; }
    RK_TEST_FORWARD_HR(SetPrivateData,(REFGUID id,UINT n,const void* data),(id,n,data))
    RK_TEST_FORWARD_HR(SetPrivateDataInterface,(REFGUID id,const IUnknown* value),(id,value))
    RK_TEST_FORWARD_HR(GetPrivateData,(REFGUID id,UINT* n,void* data),(id,n,data))
    RK_TEST_FORWARD_HR(GetParent,(REFIID id,void** out),(id,out))
    RK_TEST_FORWARD_HR(GetDevice,(REFIID id,void** out),(id,out))
    RK_TEST_FORWARD_HR(GetBuffer,(UINT index,REFIID id,void** out),(index,id,out))
    RK_TEST_FORWARD_HR(SetFullscreenState,(BOOL full,IDXGIOutput* output),(full,output))
    RK_TEST_FORWARD_HR(GetFullscreenState,(BOOL* full,IDXGIOutput** output),(full,output))
    RK_TEST_FORWARD_HR(GetDesc,(DXGI_SWAP_CHAIN_DESC* desc),(desc))
    RK_TEST_FORWARD_HR(ResizeBuffers,(UINT n,UINT w,UINT h,DXGI_FORMAT format,UINT flags),(n,w,h,format,flags))
    RK_TEST_FORWARD_HR(ResizeTarget,(const DXGI_MODE_DESC* target),(target))
    RK_TEST_FORWARD_HR(GetContainingOutput,(IDXGIOutput** output),(output))
    RK_TEST_FORWARD_HR(GetFrameStatistics,(DXGI_FRAME_STATISTICS* stats),(stats))
    RK_TEST_FORWARD_HR(GetLastPresentCount,(UINT* count),(count))
    RK_TEST_FORWARD_HR(GetDesc1,(DXGI_SWAP_CHAIN_DESC1* desc),(desc))
    RK_TEST_FORWARD_HR(GetFullscreenDesc,(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* desc),(desc))
    RK_TEST_FORWARD_HR(GetHwnd,(HWND* window),(window))
    RK_TEST_FORWARD_HR(GetCoreWindow,(REFIID id,void** out),(id,out))
    RK_TEST_FORWARD_HR(GetRestrictToOutput,(IDXGIOutput** out),(out))
    RK_TEST_FORWARD_HR(SetBackgroundColor,(const DXGI_RGBA* color),(color))
    RK_TEST_FORWARD_HR(GetBackgroundColor,(DXGI_RGBA* color),(color))
    RK_TEST_FORWARD_HR(SetRotation,(DXGI_MODE_ROTATION rotation),(rotation))
    RK_TEST_FORWARD_HR(GetRotation,(DXGI_MODE_ROTATION* rotation),(rotation))
    RK_TEST_FORWARD_HR(SetSourceSize,(UINT w,UINT h),(w,h))
    RK_TEST_FORWARD_HR(GetSourceSize,(UINT* w,UINT* h),(w,h))
    RK_TEST_FORWARD_HR(SetMaximumFrameLatency,(UINT latency),(latency))
    RK_TEST_FORWARD_HR(GetMaximumFrameLatency,(UINT* latency),(latency))
    RK_TEST_FORWARD_HR(SetMatrixTransform,(const DXGI_MATRIX_3X2_F* matrix),(matrix))
    RK_TEST_FORWARD_HR(GetMatrixTransform,(DXGI_MATRIX_3X2_F* matrix),(matrix))
    RK_TEST_FORWARD_HR(CheckColorSpaceSupport,(DXGI_COLOR_SPACE_TYPE color,UINT* support),(color,support))
    RK_TEST_FORWARD_HR(SetColorSpace1,(DXGI_COLOR_SPACE_TYPE color),(color))
    RK_TEST_FORWARD_HR(ResizeBuffers1,(UINT n,UINT w,UINT h,DXGI_FORMAT format,UINT flags,
        const UINT* mask,IUnknown* const* queues),(n,w,h,format,flags,mask,queues))
    RK_TEST_FORWARD_HR(SetHDRMetaData,(DXGI_HDR_METADATA_TYPE type,UINT size,void* data),(type,size,data))
#undef RK_TEST_FORWARD_HR
    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override {
        return lower_->IsTemporaryMonoSupported();
    }
    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override {
        return lower_->GetFrameLatencyWaitableObject();
    }
    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override {
        return lower_->GetCurrentBackBufferIndex();
    }
private:
    std::atomic<ULONG> refs_{1};
    Microsoft::WRL::ComPtr<IDXGISwapChain4> lower_;
    std::function<HRESULT(UINT)> observe_;
};
}
