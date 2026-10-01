#include "rk/FgGameInputCandidate.hpp"

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
bool same(Extent a,Extent b) noexcept {
    return a.width==b.width&&a.height==b.height;
}
bool sameObject(IUnknown* a,IUnknown* b) noexcept {
    ComPtr<IUnknown> first,second;
    return a&&b&&SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&first)))&&
        SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&second)))&&
        first.Get()==second.Get();
}
bool current(const FgResourceStamp& stamp,const FgSourceFrame& frame,
    Extent extent) noexcept {
    return stamp.ready&&stamp.source==frame.source&&
        stamp.generation==frame.generation&&
        stamp.resetEpoch==frame.resetEpoch&&same(stamp.extent,extent);
}
bool validTexture(ID3D11Texture2D* texture,ID3D11Device* owner,
    Extent extent,DXGI_FORMAT format) noexcept {
    if(!texture||!extent.valid())return false;
    ComPtr<ID3D11Device> device;
    texture->GetDevice(&device);
    if(!sameObject(device.Get(),owner))return false;
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    return desc.Width==extent.width&&desc.Height==extent.height&&
        desc.Format==format&&desc.MipLevels==1&&desc.ArraySize==1&&
        desc.SampleDesc.Count==1&&desc.SampleDesc.Quality==0&&
        desc.Usage==D3D11_USAGE_DEFAULT;
}
}
FgInputSources FgGameInputCandidate::sources() const noexcept {
    FgInputSources result{};
    for(std::size_t i=0;i<textures.size();++i)
        result.textures[i]=textures[i].Get();
    return result;
}
Result<FgGameInputCandidate> pairFgGameInputs(
    const FgWorldGuideFrame& world,const FgUiPlaneFrame& ui) {
    const auto& source=world.frame;
    if(!source.source||!source.generation||!source.presentToken||
       !source.resetEpoch||!source.render.valid()||!source.display.valid()||
       !source.worldActive||source.loading||source.paused||
       ui.source!=source.source||ui.generation!=source.generation||
       ui.presentToken!=source.presentToken||
       ui.resetEpoch!=source.resetEpoch||!same(ui.display,source.display)||
       !current(ui.hudlessStamp,source,source.display)||
       !current(ui.uiStamp,source,source.display)||
       !current(ui.finalStamp,source,source.display))
        return Error{ErrorCode::Conflict,
            "FG world guides and native UI belong to different real frames"};
    ComPtr<ID3D11Device> owner;
    if(world.depth)world.depth->GetDevice(&owner);
    if(!owner||
       !validTexture(world.depth.Get(),owner.Get(),source.render,
            DXGI_FORMAT_R32_FLOAT)||
       !validTexture(world.motion.Get(),owner.Get(),source.render,
            DXGI_FORMAT_R16G16_FLOAT)||
       !validTexture(world.hudless.Get(),owner.Get(),source.display,
            DXGI_FORMAT_R8G8B8A8_UNORM)||
       !validTexture(ui.hudless.Get(),owner.Get(),source.display,
            DXGI_FORMAT_R8G8B8A8_UNORM)||
       !validTexture(ui.uiColorAlpha.Get(),owner.Get(),source.display,
            DXGI_FORMAT_R8G8B8A8_UNORM)||
       !validTexture(ui.finalColor.Get(),owner.Get(),source.display,
            DXGI_FORMAT_R8G8B8A8_UNORM)||
       sameObject(ui.hudless.Get(),ui.uiColorAlpha.Get())||
       sameObject(ui.hudless.Get(),ui.finalColor.Get())||
       sameObject(world.hudless.Get(),ui.finalColor.Get())||
       sameObject(ui.uiColorAlpha.Get(),ui.finalColor.Get()))
        return Error{ErrorCode::InvalidInput,
            "FG game input textures have incompatible owners, formats or roles"};
    FgGameInputCandidate result{};
    result.frame=source;
    result.frame.color={source.source,source.generation,
        source.display,true,source.resetEpoch};
    result.frame.depth={source.source,source.generation,
        source.render,true,source.resetEpoch};
    result.frame.motion=result.frame.depth;
    result.frame.hudless=result.frame.color;
    result.frame.uiColorAlpha=result.frame.color;
    // Provider submission validates the HUD-less input against the exact
    // native-UI snapshot. The independent world latch is a phase witness,
    // but its separate copy cannot be substituted for ui.hudless.
    result.textures={ui.finalColor,world.depth,world.motion,
        ui.hudless,ui.uiColorAlpha};
    return result;
}
}
