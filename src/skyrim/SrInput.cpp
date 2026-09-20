#include "rk/SrInput.hpp"
#include <array>
#include <utility>

namespace rk {
using Microsoft::WRL::ComPtr;
PreparedSrInputs::PreparedSrInputs(ComPtr<ID3D11Texture2D> color,
    ComPtr<ID3D11Texture2D> motion,ComPtr<ID3D11Texture2D> depth,
    ComPtr<ID3D11Texture2D> output,UINT width,UINT height) noexcept:
    color_(std::move(color)),motion_(std::move(motion)),depth_(std::move(depth)),
    output_(std::move(output)),width_(width),height_(height) {}

Result<PreparedSrInputs> prepareSrInputs(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||sources.size()!=3)
        return Error{ErrorCode::InvalidInput,"SR preparation requires an immediate context and three textures"};
    ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    ComPtr<IUnknown> deviceIdentity;
    if(!device||FAILED(device.As(&deviceIdentity)))
        return Error{ErrorCode::Unavailable,"SR preparation device unavailable"};
    constexpr std::array expected{DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R24G8_TYPELESS};
    std::array<D3D11_TEXTURE2D_DESC,3> descriptions{};
    for(std::size_t i=0;i<sources.size();++i) {
        if(!sources[i])return Error{ErrorCode::InvalidInput,"Null SR source texture"};
        ComPtr<ID3D11Device> owner;
        ComPtr<IUnknown> ownerIdentity;
        sources[i]->GetDevice(&owner);
        if(!owner||FAILED(owner.As(&ownerIdentity))||ownerIdentity.Get()!=deviceIdentity.Get())
            return Error{ErrorCode::Conflict,"SR source belongs to another device"};
        auto& d=descriptions[i];sources[i]->GetDesc(&d);
        if(d.Format!=expected[i]||!d.Width||!d.Height||d.Width>8192||d.Height>8192||
           d.MipLevels!=1||d.ArraySize!=1||d.SampleDesc.Count!=1||
           d.Usage!=D3D11_USAGE_DEFAULT||!(d.BindFlags&D3D11_BIND_SHADER_RESOURCE))
            return Error{ErrorCode::Unsupported,"SR source format or texture geometry differs"};
        if(i&&(d.Width!=descriptions[0].Width||d.Height!=descriptions[0].Height))
            return Error{ErrorCode::Conflict,"SR source dimensions differ"};
    }
    std::array<ComPtr<ID3D11Texture2D>,3> copies;
    ComPtr<ID3D11Texture2D> output;
    for(std::size_t i=0;i<copies.size();++i) {
        auto d=descriptions[i];
        d.CPUAccessFlags=0;d.MiscFlags=0;
        if(FAILED(device->CreateTexture2D(&d,nullptr,&copies[i])))
            return Error{ErrorCode::Unavailable,"Cannot allocate owned SR source texture"};
    }
    auto d=descriptions[0];
    d.CPUAccessFlags=0;d.MiscFlags=0;
    d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
    if(FAILED(device->CreateTexture2D(&d,nullptr,&output)))
        return Error{ErrorCode::Unavailable,"Cannot allocate owned SR output texture"};
    for(std::size_t i=0;i<copies.size();++i)context->CopyResource(copies[i].Get(),sources[i]);
    return PreparedSrInputs{std::move(copies[0]),std::move(copies[1]),std::move(copies[2]),
        std::move(output),d.Width,d.Height};
}
}
