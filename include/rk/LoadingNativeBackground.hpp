#pragma once

#include <d3d11.h>
#include <cstdint>
#include <utility>
#include <wrl/client.h>

namespace rk {
// One-frame native background lease for a loading-menu call that overwrites
// the already-published image. The scratch texture is reused at a fixed size.
class LoadingNativeBackground final {
public:
    HRESULT capture(ID3D11DeviceContext* context,ID3D11Texture2D* target,
        std::uint64_t frame,std::uint64_t generation) noexcept {
        valid_=false;
        if(!context||!target||!frame||!generation||
           context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return E_INVALIDARG;
        D3D11_TEXTURE2D_DESC desc{};
        target->GetDesc(&desc);
        if(!desc.Width||!desc.Height||desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
           desc.MipLevels!=1||desc.ArraySize!=1||desc.SampleDesc.Count!=1||
           desc.SampleDesc.Quality!=0||desc.Usage!=D3D11_USAGE_DEFAULT)
            return E_INVALIDARG;
        Microsoft::WRL::ComPtr<ID3D11Device> contextDevice,targetDevice;
        context->GetDevice(&contextDevice);
        target->GetDevice(&targetDevice);
        if(!contextDevice||contextDevice.Get()!=targetDevice.Get())return E_INVALIDARG;
        if(!scratch_||desc.Width!=width_||desc.Height!=height_||
           desc.Format!=format_) {
            auto owned=desc;
            owned.BindFlags=0;
            owned.CPUAccessFlags=0;
            owned.MiscFlags=0;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> scratch;
            const auto hr=contextDevice->CreateTexture2D(&owned,nullptr,&scratch);
            if(FAILED(hr))return hr;
            scratch_=std::move(scratch);
            width_=desc.Width;
            height_=desc.Height;
            format_=desc.Format;
        }
        Microsoft::WRL::ComPtr<IUnknown> identity;
        const auto hr=target->QueryInterface(IID_PPV_ARGS(&identity));
        if(FAILED(hr))return hr;
        context->CopyResource(scratch_.Get(),target);
        targetIdentity_=std::move(identity);
        frame_=frame;
        generation_=generation;
        valid_=true;
        return S_OK;
    }

    HRESULT restore(ID3D11DeviceContext* context,ID3D11Texture2D* target,
        std::uint64_t frame,std::uint64_t generation) noexcept {
        if(!valid_||!context||!target||!scratch_||frame_!=frame||
           generation_!=generation||
           context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return E_INVALIDARG;
        Microsoft::WRL::ComPtr<IUnknown> identity;
        if(FAILED(target->QueryInterface(IID_PPV_ARGS(&identity)))||
           identity.Get()!=targetIdentity_.Get())return E_INVALIDARG;
        context->CopyResource(target,scratch_.Get());
        valid_=false;
        targetIdentity_.Reset();
        return S_OK;
    }

    void reset() noexcept {
        scratch_.Reset();
        targetIdentity_.Reset();
        width_=height_=0;
        format_=DXGI_FORMAT_UNKNOWN;
        frame_=generation_=0;
        valid_=false;
    }

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> scratch_;
    Microsoft::WRL::ComPtr<IUnknown> targetIdentity_;
    UINT width_{},height_{};
    DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};
    std::uint64_t frame_{},generation_{};
    bool valid_{};
};
}
