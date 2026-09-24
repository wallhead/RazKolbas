#include "rk/FrameProbe.hpp"
#include "rk/PatchDescriptor.hpp"
#include <wrl/client.h>
#include <cstring>
#include <fstream>
namespace rk {
bool frameProbeBoundary(std::string_view profile,SwapCall call) noexcept {
    return profile==enbSwapObserverPatchId&&call==SwapCall::Present;
}
Result<std::array<std::uintptr_t,3>> rendererCandidatePointers(std::span<const std::uint8_t> bytes,
    std::uint32_t thread,std::uintptr_t device,std::uintptr_t context,std::uintptr_t swap) {
    if(bytes.size()!=renderer1170Size)return Error{ErrorCode::InvalidInput,"Renderer snapshot extent mismatch"};
    const auto pointer=[&](std::size_t offset){std::uintptr_t value{};std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;};
    LONG recursion{};std::memcpy(&recursion,bytes.data()+0x27f0+12,sizeof(recursion));
    if(!thread||recursion<=0||pointer(0x27f0+16)!=thread)return Error{ErrorCode::Unavailable,"Present thread does not own renderer lock"};
    if(!device||!context||!swap||pointer(0x48)!=device||pointer(0x50)!=context||pointer(0x70)!=swap)
        return Error{ErrorCode::Conflict,"Engine renderer identity differs from actual swap device/context"};
    const std::array result{pointer(0xa58+0x30),pointer(0xa58+7*0x30),pointer(0x2018)};
    for(const auto value:result)if(!value)return Error{ErrorCode::Unavailable,"One or more candidate textures are absent"};
    return result;
}
namespace {
std::size_t pixelSize(DXGI_FORMAT format) {
    switch(format) {
    case DXGI_FORMAT_R16G16B16A16_FLOAT:case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R32G32_FLOAT:case DXGI_FORMAT_R32G8X24_TYPELESS:case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:return 8;
    case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:case DXGI_FORMAT_R32_FLOAT:case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R24G8_TYPELESS:case DXGI_FORMAT_D24_UNORM_S8_UINT:case DXGI_FORMAT_D32_FLOAT:return 4;
    default:return 0;
    }
}
}
Result<std::vector<ProbeImage>> readbackCandidates(ID3D11DeviceContext* context,std::span<ID3D11Texture2D* const> textures,std::size_t budget) {
    using Microsoft::WRL::ComPtr;
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||textures.empty()||textures.size()>3)
        return Error{ErrorCode::InvalidInput,"Capture requires immediate context and one to three textures"};
    ComPtr<ID3D11Device> device;context->GetDevice(&device);
    ComPtr<IUnknown> identity;if(!device||FAILED(device.As(&identity)))return Error{ErrorCode::Unavailable,"Capture device unavailable"};
    std::vector<ProbeImage> result;
    // Validate every input and the complete allocation budget before GPU copies.
    for(auto* texture:textures) {
        if(!texture)return Error{ErrorCode::InvalidInput,"Null capture texture"};
        ComPtr<ID3D11Device> owner;texture->GetDevice(&owner);ComPtr<IUnknown> ownerIdentity;
        if(!owner||FAILED(owner.As(&ownerIdentity))||identity.Get()!=ownerIdentity.Get())return Error{ErrorCode::Conflict,"Capture texture belongs to another device"};
        ProbeImage image;texture->GetDesc(&image.descriptor);const auto& d=image.descriptor;
        const auto size=pixelSize(d.Format);
        if(!size||!d.Width||!d.Height||d.Width>8192||d.Height>8192||d.SampleDesc.Count!=1||d.ArraySize!=1||d.MipLevels!=1)
            return Error{ErrorCode::Unsupported,"Capture supports known single-sample single-subresource textures up to 8192"};
        image.rowBytes=static_cast<std::size_t>(d.Width)*size;
        const auto total=image.rowBytes*d.Height;
        if(total>budget)return Error{ErrorCode::Unavailable,"Capture CPU byte budget exceeded"};
        budget-=total;image.pixels.resize(total);result.push_back(std::move(image));
    }
    for(std::size_t i=0;i<textures.size();++i) {
        auto& image=result[i];auto d=image.descriptor;
        d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
        ComPtr<ID3D11Texture2D> staging;
        if(FAILED(device->CreateTexture2D(&d,nullptr,&staging)))return Error{ErrorCode::Unavailable,"Capture staging allocation failed"};
        context->CopyResource(staging.Get(),textures[i]);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return Error{ErrorCode::Unavailable,"Capture staging Map failed"};
        struct Unmap { ID3D11DeviceContext* context;ID3D11Texture2D* texture;~Unmap(){context->Unmap(texture,0);} } unmap{context,staging.Get()};
        if(!mapped.pData||mapped.RowPitch<image.rowBytes)return Error{ErrorCode::Unavailable,"Capture mapped row extent invalid"};
        for(UINT y=0;y<d.Height;++y)std::memcpy(image.pixels.data()+y*image.rowBytes,
            static_cast<const std::uint8_t*>(mapped.pData)+static_cast<std::size_t>(y)*mapped.RowPitch,image.rowBytes);
    }
    return result;
}
Result<ProbeImage> readbackRegion(ID3D11DeviceContext* context,
    ID3D11Texture2D* texture,UINT left,UINT top,UINT width,UINT height,
    std::size_t budget) {
    using Microsoft::WRL::ComPtr;
    if(!context||!texture||!width||!height||
       context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,"Region capture arguments are invalid"};
    D3D11_TEXTURE2D_DESC source{};texture->GetDesc(&source);
    if(left>source.Width||top>source.Height||width>source.Width-left||
       height>source.Height-top||source.SampleDesc.Count!=1||
       source.ArraySize!=1||source.MipLevels!=1)
        return Error{ErrorCode::InvalidInput,"Region capture rectangle exceeds source"};
    ComPtr<ID3D11Device> device,owner;
    context->GetDevice(&device);texture->GetDevice(&owner);
    ComPtr<IUnknown> deviceId,ownerId;
    if(!device||!owner||FAILED(device.As(&deviceId))||FAILED(owner.As(&ownerId))||
       deviceId.Get()!=ownerId.Get())
        return Error{ErrorCode::Conflict,"Region capture texture belongs to another device"};
    auto regionDesc=source;
    regionDesc.Width=width;regionDesc.Height=height;
    regionDesc.Usage=D3D11_USAGE_DEFAULT;regionDesc.CPUAccessFlags=0;
    regionDesc.BindFlags=0;regionDesc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> region;
    if(FAILED(device->CreateTexture2D(&regionDesc,nullptr,&region)))
        return Error{ErrorCode::Unavailable,"Region capture texture allocation failed"};
    const D3D11_BOX box{left,top,0,left+width,top+height,1};
    context->CopySubresourceRegion(region.Get(),0,0,0,0,texture,0,&box);
    const std::array<ID3D11Texture2D*,1> sourceRegion{region.Get()};
    auto captured=readbackCandidates(context,sourceRegion,budget);
    if(const auto error=std::get_if<Error>(&captured))return *error;
    return std::move(std::get<std::vector<ProbeImage>>(captured).front());
}
Result<bool> saveProbeBundle(const std::filesystem::path& directory,
    std::span<const ProbeImage> images,std::span<const std::string_view> names,
    std::string_view description) {
    namespace fs=std::filesystem;
    if(directory.empty()||!directory.is_absolute()||description.empty()||
       description.find('\n')!=std::string_view::npos||images.empty()||
       images.size()>64||images.size()!=names.size())
        return Error{ErrorCode::InvalidInput,"Probe bundle path or image count is invalid"};
    for(std::size_t i=0;i<images.size();++i) {
        const auto& image=images[i];
        if(!image.descriptor.Width||!image.descriptor.Height||!image.rowBytes||
           image.rowBytes>image.pixels.size()/image.descriptor.Height||
           image.pixels.size()!=image.rowBytes*image.descriptor.Height||
           names[i].empty()||names[i]=="."||names[i]==".."||
           names[i].find_first_of("/\\:")!=std::string_view::npos)
            return Error{ErrorCode::InvalidInput,"Probe bundle image or file name is invalid"};
        for(std::size_t j=0;j<i;++j)if(names[j]==names[i])
            return Error{ErrorCode::Conflict,"Probe bundle file names repeat"};
    }
    try {
        if(fs::exists(directory))
            return Error{ErrorCode::Conflict,"Probe bundle directory already exists"};
        fs::create_directories(directory);
        for(std::size_t i=0;i<images.size();++i) {
            std::ofstream output(directory/std::string(names[i]),std::ios::binary);
            output.exceptions(std::ios::failbit|std::ios::badbit);
            output.write(reinterpret_cast<const char*>(images[i].pixels.data()),
                static_cast<std::streamsize>(images[i].pixels.size()));
            output.close();
        }
        std::ofstream manifest(directory/"manifest.pending");
        manifest.exceptions(std::ios::failbit|std::ios::badbit);
        manifest<<description<<"\n";
        for(std::size_t i=0;i<images.size();++i) {
            const auto& image=images[i];
            manifest<<names[i]<<" width="<<image.descriptor.Width
                <<" height="<<image.descriptor.Height
                <<" format="<<static_cast<unsigned>(image.descriptor.Format)
                <<" rowBytes="<<image.rowBytes<<" bytes="<<image.pixels.size()
                <<" sha256="<<sha256(image.pixels)<<"\n";
        }
        manifest<<"complete=true\n";
        manifest.close();
        fs::rename(directory/"manifest.pending",directory/"manifest.txt");
        return true;
    } catch(const std::exception& error) {
        return Error{ErrorCode::Io,std::string("Cannot save probe bundle: ")+error.what()};
    }
}
}
