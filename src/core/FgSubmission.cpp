#include "rk/FgSubmission.hpp"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace rk {
namespace {
bool same(Extent a,Extent b) noexcept {
    return a.width==b.width&&a.height==b.height;
}
bool current(const FgResourceStamp& stamp,const FgSourceFrame& frame,
    Extent expected) noexcept {
    return stamp.ready&&stamp.source==frame.source&&
        stamp.generation==frame.generation&&
        stamp.resetEpoch==frame.resetEpoch&&same(stamp.extent,expected);
}
template<std::size_t N> bool finite(const std::array<float,N>& values) noexcept {
    return std::all_of(values.begin(),values.end(),
        [](float value){return std::isfinite(value);});
}
bool matrix(const std::array<float,16>& values) noexcept {
    return finite(values)&&std::any_of(values.begin(),values.end(),
        [](float value){return value!=0.0f;});
}
bool inversePair(const std::array<float,16>& forward,
    const std::array<float,16>& reverse) noexcept {
    constexpr double tolerance=0.01;
    for(const auto* pair:{&forward,&reverse}) {
        const auto& left=*pair;
        const auto& right=pair==&forward?reverse:forward;
        for(std::size_t row=0;row<4;++row)
            for(std::size_t column=0;column<4;++column) {
                double result=0;
                for(std::size_t term=0;term<4;++term)
                    result+=static_cast<double>(left[row*4+term])*
                        right[term*4+column];
                const double expected=row==column?1.0:0.0;
                if(!std::isfinite(result)||
                   std::fabs(result-expected)>tolerance)return false;
            }
    }
    return true;
}
bool cameraReady(const FgCameraData& camera) noexcept {
    return matrix(camera.viewToClip)&&matrix(camera.clipToView)&&
        matrix(camera.clipToPrevClip)&&matrix(camera.prevClipToClip)&&
        inversePair(camera.clipToPrevClip,camera.prevClipToClip)&&
        finite(camera.position)&&finite(camera.up)&&
        finite(camera.right)&&finite(camera.forward)&&
        finite(camera.jitter)&&finite(camera.mvecScale)&&
        camera.mvecScale[0]>0.0f&&camera.mvecScale[1]>0.0f&&
        std::isfinite(camera.nearPlane)&&camera.nearPlane>0.0f&&
        std::isfinite(camera.farPlane)&&camera.farPlane>camera.nearPlane&&
        std::isfinite(camera.fovRadians)&&camera.fovRadians>0.0f&&
        camera.fovRadians<3.14159265f&&
        std::isfinite(camera.aspectRatio)&&camera.aspectRatio>0.0f;
}
bool alphaFormat(DXGI_FORMAT format) noexcept {
    switch(format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:return true;
    default:return false;
    }
}
bool sameDevice(ID3D12Resource* resource,IUnknown* expected) noexcept {
    Microsoft::WRL::ComPtr<ID3D12Device> owner;
    Microsoft::WRL::ComPtr<IUnknown> identity;
    return SUCCEEDED(resource->GetDevice(IID_PPV_ARGS(&owner)))&&
        SUCCEEDED(owner.As(&identity))&&identity.Get()==expected;
}
}
Result<FgPreparedSubmission> prepareFgSubmission(
    const FgSourceFrame& frame,const FgInputLease& lease,
    const FgUiPlaneFrame& ui,const FgCameraData& camera,
    std::uint32_t physicalOutputIndex,std::uint32_t swapBufferCount) {
    if(!frame.source||!frame.generation||!frame.presentToken||
       !frame.resetEpoch||!frame.cameraValid||!frame.render.valid()||
       !frame.display.valid()||!swapBufferCount||
       physicalOutputIndex>=swapBufferCount||!cameraReady(camera))
        return Error{ErrorCode::InvalidInput,
            "FG frame, camera or physical output index is invalid"};
    if(camera.source!=frame.source||camera.generation!=frame.generation||
       camera.presentToken!=frame.presentToken||
       camera.resetEpoch!=frame.resetEpoch||!camera.sampleRevision||
       camera.sampleRevision==UINT64_MAX)
        return Error{ErrorCode::Conflict,
            "FG camera sample belongs to a different real frame"};
    if(!lease.serial||!lease.lastCopy.producer||!lease.lastCopy.copy||
       lease.source!=frame.source||lease.generation!=frame.generation||
       lease.presentToken!=frame.presentToken||
       lease.resetEpoch!=frame.resetEpoch)
        return Error{ErrorCode::Conflict,
            "FG copied inputs belong to a different real frame"};
    if(ui.source!=frame.source||ui.generation!=frame.generation||
       ui.presentToken!=frame.presentToken||
       ui.resetEpoch!=frame.resetEpoch||!same(ui.display,frame.display)||
       !ui.hudless||!ui.uiColorAlpha||!ui.finalColor||
       ui.hudless.Get()==ui.uiColorAlpha.Get()||
       ui.hudless.Get()==ui.finalColor.Get()||
       ui.uiColorAlpha.Get()==ui.finalColor.Get()||
       ui.uiRegion.left<0||ui.uiRegion.top<0||
       ui.uiRegion.right<=ui.uiRegion.left||
       ui.uiRegion.bottom<=ui.uiRegion.top||
       static_cast<std::uint32_t>(ui.uiRegion.right)>frame.display.width||
       static_cast<std::uint32_t>(ui.uiRegion.bottom)>frame.display.height||
       !current(ui.hudlessStamp,frame,frame.display)||
       !current(ui.uiStamp,frame,frame.display)||
       !current(ui.finalStamp,frame,frame.display))
        return Error{ErrorCode::Conflict,
            "FG UI planes do not belong to the current display frame"};
    if(lease.sourceTextures[0].Get()!=ui.finalColor.Get()||
       lease.sourceTextures[3].Get()!=ui.hudless.Get()||
       lease.sourceTextures[4].Get()!=ui.uiColorAlpha.Get())
        return Error{ErrorCode::Conflict,
            "FG copied colour and UI sources differ from the UI snapshot"};
    const std::array<const FgResourceStamp*,5> stamps{
        &frame.color,&frame.depth,&frame.motion,&frame.hudless,
        &frame.uiColorAlpha};
    Microsoft::WRL::ComPtr<ID3D12Device> owner;
    Microsoft::WRL::ComPtr<IUnknown> identity;
    for(std::size_t i=0;i<lease.resources.size();++i) {
        const auto expected=(i==1||i==2)?frame.render:frame.display;
        if(!current(*stamps[i],frame,expected)||!lease.resources[i])
            return Error{ErrorCode::Conflict,
                "FG guide stamp or resource is stale"};
        const auto desc=lease.resources[i]->GetDesc();
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
           desc.Width!=expected.width||desc.Height!=expected.height||
           desc.DepthOrArraySize!=1||desc.MipLevels!=1||
           desc.SampleDesc.Count!=1||desc.SampleDesc.Quality!=0||
           desc.Format==DXGI_FORMAT_UNKNOWN||
           (i==4&&!alphaFormat(desc.Format)))
            return Error{ErrorCode::InvalidInput,
                "FG copied resource descriptor is incompatible with its role"};
        if(i==0) {
            if(FAILED(lease.resources[i]->GetDevice(IID_PPV_ARGS(&owner)))||
               FAILED(owner.As(&identity)))
                return Error{ErrorCode::Unavailable,
                    "FG copied colour lacks a D3D12 device"};
        } else if(!sameDevice(lease.resources[i].Get(),identity.Get()))
            return Error{ErrorCode::Conflict,
                "FG copied inputs do not share one D3D12 device"};
    }
    FgPreparedSubmission result{};
    result.source=frame.source;result.generation=frame.generation;
    result.presentToken=frame.presentToken;
    result.resetEpoch=frame.resetEpoch;
    result.render=frame.render;result.display=frame.display;
    result.physicalOutputIndex=physicalOutputIndex;
    result.swapBufferCount=swapBufferCount;
    result.uiRegion=ui.uiRegion;
    result.copyTicket=lease.lastCopy;
    result.camera=camera;
    result.resources=lease.resources;
    return result;
}
}
