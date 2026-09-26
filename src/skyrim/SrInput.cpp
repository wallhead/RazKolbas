#include "rk/SrInput.hpp"
#include "rk/D3D11StateScope.hpp"
#include <d3dcompiler.h>
#include <array>
#include <memory>
#include <utility>

namespace rk {
using Microsoft::WRL::ComPtr;
PreparedSrInputs::PreparedSrInputs(ComPtr<ID3D11Texture2D> color,
    ComPtr<ID3D11Texture2D> motion,ComPtr<ID3D11Texture2D> depth,
    ComPtr<ID3D11Texture2D> output,UINT width,UINT height,
    UINT outputWidth,UINT outputHeight,SrSourceRegion sourceRegion,
    ComPtr<ID3D11ComputeShader> depthCropShader,
    ComPtr<ID3D11Texture2D> depthSnapshot,
    ComPtr<ID3D11ShaderResourceView> depthView,
    ComPtr<ID3D11UnorderedAccessView> depthTarget,
    ComPtr<ID3D11Buffer> cropConstants) noexcept:
    color_(std::move(color)),motion_(std::move(motion)),depth_(std::move(depth)),
    output_(std::move(output)),depthCropShader_(std::move(depthCropShader)),
    depthSnapshot_(std::move(depthSnapshot)),depthView_(std::move(depthView)),
    depthTarget_(std::move(depthTarget)),cropConstants_(std::move(cropConstants)),
    width_(width),height_(height),
    outputWidth_(outputWidth),outputHeight_(outputHeight),sourceRegion_(sourceRegion) {}

Result<bool> PreparedSrInputs::refreshConvertedDepth(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||sources.size()!=3)
        return Error{ErrorCode::InvalidInput,"Converted-depth SR refresh needs an immediate context and three textures"};
    if(!color_||!motion_||!depth_||!output_||!depthCropShader_||!depthSnapshot_||
       !depthView_||!depthTarget_||!cropConstants_||sourceRegion_.left||
       sourceRegion_.top||sourceRegion_.width!=width_||sourceRegion_.height!=height_)
        return Error{ErrorCode::Conflict,"Prepared SR slot has no reusable depth conversion"};
    ComPtr<ID3D11Device> device,slotOwner;
    context->GetDevice(&device);color_->GetDevice(&slotOwner);
    ComPtr<IUnknown> deviceIdentity,slotIdentity;
    if(!device||!slotOwner||FAILED(device.As(&deviceIdentity))||
       FAILED(slotOwner.As(&slotIdentity))||deviceIdentity.Get()!=slotIdentity.Get())
        return Error{ErrorCode::Conflict,"Converted-depth SR refresh device differs"};
    const std::array expected{DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R24G8_TYPELESS};
    std::array<D3D11_TEXTURE2D_DESC,3> descriptions{};
    for(std::size_t i=0;i<sources.size();++i) {
        if(!sources[i])return Error{ErrorCode::InvalidInput,"Converted-depth SR refresh source is null"};
        ComPtr<ID3D11Device> owner;
        ComPtr<IUnknown> ownerIdentity;
        sources[i]->GetDevice(&owner);
        if(!owner||FAILED(owner.As(&ownerIdentity))||ownerIdentity.Get()!=deviceIdentity.Get())
            return Error{ErrorCode::Conflict,"Converted-depth SR refresh source device differs"};
        sources[i]->GetDesc(&descriptions[i]);
        const auto& d=descriptions[i];
        if(d.Format!=expected[i]||d.MipLevels!=1||d.ArraySize!=1||
           d.SampleDesc.Count!=1||d.Usage!=D3D11_USAGE_DEFAULT)
            return Error{ErrorCode::Unsupported,"Converted-depth SR refresh source format or geometry differs"};
    }
    if(descriptions[0].Width!=width_||descriptions[0].Height!=height_||
       descriptions[1].Width!=descriptions[2].Width||
       descriptions[1].Height!=descriptions[2].Height||
       !((descriptions[1].Width==width_&&descriptions[1].Height==height_)||
         (descriptions[1].Width==outputWidth_&&descriptions[1].Height==outputHeight_)))
        return Error{ErrorCode::Conflict,"Converted-depth SR refresh source extents differ"};
    D3D11_TEXTURE2D_DESC snapshot{};
    depthSnapshot_->GetDesc(&snapshot);
    if(snapshot.Width!=descriptions[2].Width||snapshot.Height!=descriptions[2].Height||
       snapshot.Format!=descriptions[2].Format)
        return Error{ErrorCode::Conflict,"Converted-depth SR refresh depth snapshot extent differs"};
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    context->CopyResource(depthSnapshot_.Get(),sources[2]);
    const D3D11_BOX box{0,0,0,width_,height_,1};
    context->CopySubresourceRegion(color_.Get(),0,0,0,0,sources[0],0,&box);
    context->CopySubresourceRegion(motion_.Get(),0,0,0,0,sources[1],0,&box);
    context->CSSetShader(depthCropShader_.Get(),nullptr,0);
    context->CSSetShaderResources(0,1,depthView_.GetAddressOf());
    context->CSSetUnorderedAccessViews(0,1,depthTarget_.GetAddressOf(),nullptr);
    context->CSSetConstantBuffers(0,1,cropConstants_.GetAddressOf());
    context->Dispatch((width_+7)/8,(height_+7)/8,1);
    return true;
}

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
Result<ColorSampleStats> sampleWorldColor(std::span<const std::uint8_t> pixels,
    UINT width,UINT height,std::size_t rowBytes) {
    if(!width||!height||width>8192||height>8192||
       rowBytes<static_cast<std::size_t>(width)*4||rowBytes>pixels.size()/height)
        return Error{ErrorCode::InvalidInput,"Raw colour sample extent differs"};
    std::array<std::uint32_t,256> seen{};
    ColorSampleStats stats{};
    for(UINT y=0;y<16;++y)for(UINT x=0;x<16;++x) {
        const auto sx=static_cast<UINT>((2*x+1)*static_cast<std::uint64_t>(width)/32);
        const auto sy=static_cast<UINT>((2*y+1)*static_cast<std::uint64_t>(height)/32);
        const auto* pixel=pixels.data()+static_cast<std::size_t>(sy)*rowBytes+sx*4;
        stats.nonBlack+=pixel[0]>4||pixel[1]>4||pixel[2]>4;
        std::uint32_t color{};
        std::memcpy(&color,pixel,sizeof(color));
        bool known=false;
        for(unsigned i=0;i<stats.distinct;++i)known|=seen[i]==color;
        if(!known)seen[stats.distinct++]=color;
    }
    return stats;
}

namespace {
constexpr char depthCropShader[]=R"(
Texture2D<float> SourceDepth : register(t0);
RWTexture2D<float> CroppedDepth : register(u0);
cbuffer CropOrigin : register(b0) { uint2 Origin; uint2 Padding; };
[numthreads(8,8,1)]
void main(uint3 pixel : SV_DispatchThreadID) {
    uint width,height;
    CroppedDepth.GetDimensions(width,height);
    if(pixel.x<width&&pixel.y<height)
        CroppedDepth[pixel.xy]=SourceDepth.Load(int3(pixel.xy+Origin,0));
}
)";
ComPtr<ID3D11ComputeShader> cachedDepthCropShader(ID3D11Device* device) {
    static const ComPtr<ID3DBlob> code=[] {
        ComPtr<ID3DBlob> compiled,diagnostics;
        if(FAILED(D3DCompile(depthCropShader,sizeof(depthCropShader)-1,nullptr,
            nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,
            &compiled,&diagnostics)))compiled.Reset();
        return compiled;
    }();
    if(!device||!code)return {};
    struct DeviceShader {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11ComputeShader> shader;
    };
    thread_local DeviceShader cached;
    if(cached.device.Get()!=device) {
        cached.shader.Reset();cached.device=device;
    }
    if(!cached.shader&&FAILED(device->CreateComputeShader(code->GetBufferPointer(),
        code->GetBufferSize(),nullptr,&cached.shader)))return {};
    return cached.shader;
}
Result<PreparedSrInputs> prepareInputs(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,DXGI_FORMAT colorFormat,
    UINT outputWidth,UINT outputHeight,SrSourceRegion region={},
    bool ownedScene=false,bool normalizeDepthToR32=false) {
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
        if(i&&!ownedScene&&
           (d.Width!=descriptions[0].Width||d.Height!=descriptions[0].Height))
            return Error{ErrorCode::Conflict,"SR source dimensions differ"};
    }
    if(!outputWidth)outputWidth=descriptions[0].Width;
    if(!outputHeight)outputHeight=descriptions[0].Height;
    if((region.width==0)!=(region.height==0)||
       (!region.width&&(region.left||region.top)))
        return Error{ErrorCode::InvalidInput,"SR source region is incomplete"};
    const bool cropped=region.width!=0;
    const bool normalizeDepth=cropped||normalizeDepthToR32;
    if(cropped&&(region.left>=descriptions[0].Width||
                 region.top>=descriptions[0].Height||
                 region.width>descriptions[0].Width-region.left||
                 region.height>descriptions[0].Height-region.top))
        return Error{ErrorCode::InvalidInput,"SR source region exceeds its guide allocations"};
    const UINT renderWidth=cropped?region.width:descriptions[0].Width;
    const UINT renderHeight=cropped?region.height:descriptions[0].Height;
    if(!cropped)region={0,0,renderWidth,renderHeight};
    if(ownedScene) {
        if(!cropped||region.left||region.top||
           descriptions[0].Width!=renderWidth||
           descriptions[0].Height!=renderHeight||
           (renderWidth==outputWidth&&renderHeight==outputHeight))
            return Error{ErrorCode::Conflict,"Owned SDR scene must be the exact reduced rectangle"};
        const auto guideExtent=[&](const D3D11_TEXTURE2D_DESC& d) {
            return (d.Width==renderWidth&&d.Height==renderHeight)||
                (d.Width==outputWidth&&d.Height==outputHeight);
        };
        if(!guideExtent(descriptions[1])||!guideExtent(descriptions[2])||
           descriptions[1].Width!=descriptions[2].Width||
           descriptions[1].Height!=descriptions[2].Height)
            return Error{ErrorCode::Conflict,"Owned SDR motion/depth guides have inconsistent extents"};
    }
    if(outputWidth<renderWidth||outputHeight<renderHeight||
       outputWidth>8192||outputHeight>8192)
        return Error{ErrorCode::InvalidInput,"SR display extent must cover render extent"};
    std::array<ComPtr<ID3D11Texture2D>,3> copies;
    ComPtr<ID3D11Texture2D> output;
    for(std::size_t i=0;i<copies.size();++i) {
        auto d=descriptions[i];
        d.Width=renderWidth;d.Height=renderHeight;
        d.CPUAccessFlags=0;d.MiscFlags=0;
        if(normalizeDepth&&i==2) {
            d.Format=DXGI_FORMAT_R32_FLOAT;
            d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        }
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
    ComPtr<ID3D11ComputeShader> depthShader;
    ComPtr<ID3D11Texture2D> depthSnapshot;
    ComPtr<ID3D11ShaderResourceView> depthView;
    ComPtr<ID3D11UnorderedAccessView> depthTarget;
    ComPtr<ID3D11Buffer> cropConstants;
    std::unique_ptr<D3D11StateScope> isolated;
    if(normalizeDepth) {
        depthShader=cachedDepthCropShader(device.Get());
        if(!depthShader)
            return Error{ErrorCode::Unavailable,"Cannot prepare depth crop shader"};
        // The original depth may remain bound as Skyrim's writable DSV in the
        // context state we restore after this pass. Sample an owned full-size
        // snapshot, never that live resource, while producing reduced R32.
        auto snapshotDesc=descriptions[2];
        snapshotDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        snapshotDesc.CPUAccessFlags=0;
        snapshotDesc.MiscFlags=0;
        if(FAILED(device->CreateTexture2D(&snapshotDesc,nullptr,&depthSnapshot)))
            return Error{ErrorCode::Unavailable,"Cannot allocate owned depth snapshot"};
        D3D11_SHADER_RESOURCE_VIEW_DESC sourceView{};
        sourceView.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        sourceView.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
        sourceView.Texture2D.MipLevels=1;
        if(FAILED(device->CreateShaderResourceView(depthSnapshot.Get(),&sourceView,&depthView))||
           FAILED(device->CreateUnorderedAccessView(copies[2].Get(),nullptr,&depthTarget)))
            return Error{ErrorCode::Unavailable,"Cannot create depth crop views"};
        D3D11_BUFFER_DESC constantsDesc{};
        constantsDesc.ByteWidth=16;
        constantsDesc.Usage=D3D11_USAGE_DEFAULT;
        constantsDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        const std::array<UINT,4> origin{region.left,region.top,0,0};
        const D3D11_SUBRESOURCE_DATA constantsData{origin.data(),0,0};
        if(FAILED(device->CreateBuffer(&constantsDesc,&constantsData,&cropConstants)))
            return Error{ErrorCode::Unavailable,"Cannot create depth crop origin buffer"};
        auto scope=D3D11StateScope::begin(context);
        if(const auto error=std::get_if<Error>(&scope))return *error;
        isolated=std::move(std::get<std::unique_ptr<D3D11StateScope>>(scope));
        context->CopyResource(depthSnapshot.Get(),sources[2]);
    }
    for(std::size_t i=0;i<copies.size();++i) {
        if(cropped) {
            if(i==2)continue;
            const D3D11_BOX box{region.left,region.top,0,
                region.left+renderWidth,region.top+renderHeight,1};
            context->CopySubresourceRegion(copies[i].Get(),0,0,0,0,sources[i],0,&box);
        } else if(!(normalizeDepth&&i==2))
            context->CopyResource(copies[i].Get(),sources[i]);
    }
    if(normalizeDepth) {
        context->CSSetShader(depthShader.Get(),nullptr,0);
        context->CSSetShaderResources(0,1,depthView.GetAddressOf());
        context->CSSetUnorderedAccessViews(0,1,depthTarget.GetAddressOf(),nullptr);
        context->CSSetConstantBuffers(0,1,cropConstants.GetAddressOf());
        context->Dispatch((renderWidth+7)/8,(renderHeight+7)/8,1);
    }
    return PreparedSrInputs{std::move(copies[0]),std::move(copies[1]),std::move(copies[2]),
        std::move(output),renderWidth,renderHeight,d.Width,d.Height,region,
        std::move(depthShader),std::move(depthSnapshot),std::move(depthView),
        std::move(depthTarget),std::move(cropConstants)};
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
    std::span<ID3D11Texture2D* const> sources,UINT outputWidth,UINT outputHeight,
    SdrPreparationPolicy policy) {
    if(!outputWidth||!outputHeight)
        return Error{ErrorCode::InvalidInput,"SR display extent is zero"};
    return prepareInputs(context,sources,DXGI_FORMAT_R8G8B8A8_UNORM,
        outputWidth,outputHeight,{},false,policy.normalizeDepthToR32);
}
Result<PreparedSrInputs> prepareSdrSrInputsFromRegion(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,UINT renderWidth,UINT renderHeight,
    UINT outputWidth,UINT outputHeight) {
    if(!renderWidth||!renderHeight||!outputWidth||!outputHeight)
        return Error{ErrorCode::InvalidInput,"SR source or display region is zero"};
    return prepareInputs(context,sources,DXGI_FORMAT_R8G8B8A8_UNORM,
        outputWidth,outputHeight,{0,0,renderWidth,renderHeight});
}
Result<PreparedSrInputs> prepareSdrSrInputsFromRegion(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,SrSourceRegion region,
    UINT outputWidth,UINT outputHeight) {
    if(!region.width||!region.height||!outputWidth||!outputHeight)
        return Error{ErrorCode::InvalidInput,"SR source or display region is zero"};
    return prepareInputs(context,sources,DXGI_FORMAT_R8G8B8A8_UNORM,
        outputWidth,outputHeight,region);
}
Result<PreparedSrInputs> prepareSdrSrInputsFromOwnedScene(ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,UINT outputWidth,UINT outputHeight) {
    if(sources.size()!=3||!sources[0]||!outputWidth||!outputHeight)
        return Error{ErrorCode::InvalidInput,"Owned SDR scene or display extent is unavailable"};
    D3D11_TEXTURE2D_DESC scene{};
    sources[0]->GetDesc(&scene);
    return prepareInputs(context,sources,DXGI_FORMAT_R8G8B8A8_UNORM,
        outputWidth,outputHeight,{0,0,scene.Width,scene.Height},true);
}
}
