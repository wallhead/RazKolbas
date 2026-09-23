#include "rk/SamplerBiasCache.hpp"
#include <algorithm>
#include <cmath>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
ComPtr<IUnknown> identity(IUnknown* object) noexcept {
    ComPtr<IUnknown> result;
    if(object)object->QueryInterface(IID_PPV_ARGS(result.GetAddressOf()));
    return result;
}
}

HRESULT SamplerBiasCache::configure(ID3D11DeviceContext* context,float bias) noexcept {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !std::isfinite(bias)||bias<-3.0f||bias>3.0f)return E_INVALIDARG;
    ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());
    auto id=identity(device.Get());
    if(!device||!id)return E_NOINTERFACE;
    if(device_)return id.Get()==deviceIdentity_.Get()&&bias==bias_?S_FALSE:E_UNEXPECTED;
    device_=std::move(device);deviceIdentity_=std::move(id);bias_=bias;
    return S_OK;
}

bool SamplerBiasCache::remap(ID3D11DeviceContext* context,
    std::span<ID3D11SamplerState* const> input,
    std::span<ID3D11SamplerState*> output) noexcept {
    const auto copyOriginal=[&] {
        const auto count=std::min(input.size(),output.size());
        for(std::size_t i=0;i<count;++i)output[i]=input[i];
    };
    copyOriginal();
    if(!device_||!context||input.size()!=output.size()||input.empty())return false;
    ComPtr<ID3D11Device> owner;context->GetDevice(owner.GetAddressOf());
    if(identity(owner.Get()).Get()!=deviceIdentity_.Get())return false;
    bool changed=false;
    try {
        for(std::size_t i=0;i<input.size();++i) {
            auto* source=input[i];
            if(!source)continue;
            auto sourceId=identity(source);
            if(!sourceId)continue;
            const auto found=std::find_if(entries_.begin(),entries_.end(),
                [&](const Entry& entry){return entry.source.Get()==sourceId.Get();});
            if(found!=entries_.end()) {
                output[i]=found->replacement.Get();changed=true;continue;
            }
            if(std::ranges::any_of(rejected_,[&](const auto& rejected) {
                return rejected.Get()==sourceId.Get();
            }))continue;
            D3D11_SAMPLER_DESC desc{};source->GetDesc(&desc);
            if(desc.MipLODBias!=0.0f||desc.MaxAnisotropy<=1) {
                if(rejected_.size()<256)rejected_.push_back(std::move(sourceId));
                continue;
            }
            if(entries_.size()>=256)continue;
            desc.MipLODBias=bias_;
            ComPtr<ID3D11SamplerState> replacement;
            if(FAILED(device_->CreateSamplerState(&desc,replacement.GetAddressOf())))continue;
            entries_.push_back({std::move(sourceId),std::move(replacement)});
            output[i]=entries_.back().replacement.Get();changed=true;
        }
    } catch(...) {
        copyOriginal();return false;
    }
    return changed;
}
}
