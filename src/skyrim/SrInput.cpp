#include "rk/SrInput.hpp"
#include <array>
#include <utility>

namespace rk {
using Microsoft::WRL::ComPtr;
PreparedSrInputs::PreparedSrInputs(ComPtr<ID3D11Texture2D> color,
    ComPtr<ID3D11Texture2D> motion,ComPtr<ID3D11Texture2D> depth,
    ComPtr<ID3D11Texture2D> output,UINT width,UINT height,
    UINT outputWidth,UINT outputHeight) noexcept:
    color_(std::move(color)),motion_(std::move(motion)),depth_(std::move(depth)),
    output_(std::move(output)),width_(width),height_(height),
    outputWidth_(outputWidth),outputHeight_(outputHeight) {}

Result<DepthSampleStats> sampleWorldDepth(std::span<const std::uint8_t> pixels,
    UINT width,UINT height,std::size_t rowBytes) {
    if(!width||!height||width>8192||height>8192||rowBytes<static_cast<std::size_t>(width)*4||
       rowBytes>pixels.size()/height)
        return Error{ErrorCode::InvalidInput,"Raw depth sample extent differs"};
    std::array<std::uint32_t,100> seen{};
    DepthSampleStats stats{};
    for(UINT y=0;y<10;++y)for(UINT x=0;x<10;++x) {
        const auto sx=static_cast<UINT>((2*x+1)*static_cast<std::uint64_t>(width)/20);
        const auto sy=static_cast<UINT>((2*y+1)*static_cast<std::uint64_t>(height)/20);
        std::uint32_t packed{};
        std::memcpy(&packed,pixels.data()+static_cast<std::size_t>(sy)*rowBytes+sx*4,4);
        const auto depth=packed&0xffffffU;
        stats.nonFar+=depth!=0xffffffU;
        bool known=false;
        for(unsigned i=0;i<stats.distinct;++i)known|=seen[i]==depth;
        if(!known)seen[stats.distinct++]=depth;
    }
    return stats;
}

namespace {
Result<PreparedSrInputs> prepareInputs(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,DXGI_FORMAT colorFormat,
    UINT outputWidth,UINT outputHeight) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||sources.size()!=3)
        return Error{ErrorCode::InvalidInput,"SR preparation requires an immediate context and three textures"};
    ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    ComPtr<IUnknown> deviceIdentity;
    if(!device||FAILED(device.As(&deviceIdentity)))
        return Error{ErrorCode::Unavailable,"SR preparation device unavailable"};
    const std::array expected{colorFormat,DXGI_FORMAT_R16G16_FLOAT,
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
           d.Usage!=D3D11_USAGE_DEFAULT||
           (i||colorFormat!=DXGI_FORMAT_R8G8B8A8_UNORM)&&
           !(d.BindFlags&D3D11_BIND_SHADER_RESOURCE))
            return Error{ErrorCode::Unsupported,"SR source format or texture geometry differs"};
        if(i&&(d.Width!=descriptions[0].Width||d.Height!=descriptions[0].Height))
            return Error{ErrorCode::Conflict,"SR source dimensions differ"};
    }
    if(!outputWidth)outputWidth=descriptions[0].Width;
    if(!outputHeight)outputHeight=descriptions[0].Height;
    if(outputWidth<descriptions[0].Width||outputHeight<descriptions[0].Height||
       outputWidth>8192||outputHeight>8192)
        return Error{ErrorCode::InvalidInput,"SR display extent must cover render extent"};
    std::array<ComPtr<ID3D11Texture2D>,3> copies;
    ComPtr<ID3D11Texture2D> output;
    for(std::size_t i=0;i<copies.size();++i) {
        auto d=descriptions[i];
        d.CPUAccessFlags=0;d.MiscFlags=0;
        if(i==0&&colorFormat==DXGI_FORMAT_R8G8B8A8_UNORM)
            d.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device->CreateTexture2D(&d,nullptr,&copies[i])))
            return Error{ErrorCode::Unavailable,"Cannot allocate owned SR source texture"};
    }
    auto d=descriptions[0];
    d.Width=outputWidth;d.Height=outputHeight;
    d.CPUAccessFlags=0;d.MiscFlags=0;
    d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
    if(FAILED(device->CreateTexture2D(&d,nullptr,&output)))
        return Error{ErrorCode::Unavailable,"Cannot allocate owned SR output texture"};
    for(std::size_t i=0;i<copies.size();++i)context->CopyResource(copies[i].Get(),sources[i]);
    return PreparedSrInputs{std::move(copies[0]),std::move(copies[1]),std::move(copies[2]),
        std::move(output),descriptions[0].Width,descriptions[0].Height,d.Width,d.Height};
}
}
Result<PreparedSrInputs> prepareSrInputs(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources) {
    return prepareInputs(context,sources,DXGI_FORMAT_R16G16B16A16_FLOAT,0,0);
}
Result<PreparedSrInputs> prepareSrInputsForDisplay(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,UINT outputWidth,UINT outputHeight) {
    if(!outputWidth||!outputHeight)
        return Error{ErrorCode::InvalidInput,"SR display extent is zero"};
    return prepareInputs(context,sources,DXGI_FORMAT_R16G16B16A16_FLOAT,
        outputWidth,outputHeight);
}
Result<PreparedSrInputs> prepareSdrSrInputs(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources) {
    return prepareInputs(context,sources,DXGI_FORMAT_R8G8B8A8_UNORM,0,0);
}
Result<PreparedSrInputs> prepareSdrSrInputsForDisplay(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,UINT outputWidth,UINT outputHeight) {
    if(!outputWidth||!outputHeight)
        return Error{ErrorCode::InvalidInput,"SR display extent is zero"};
    return prepareInputs(context,sources,DXGI_FORMAT_R8G8B8A8_UNORM,
        outputWidth,outputHeight);
}
}
