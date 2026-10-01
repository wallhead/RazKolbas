#include "rk/FgWorldGuideLatch.hpp"
#include <utility>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;

bool sameObject(IUnknown* first,IUnknown* second) noexcept {
    ComPtr<IUnknown> a,b;
    return first&&second&&
        SUCCEEDED(first->QueryInterface(IID_PPV_ARGS(&a)))&&
        SUCCEEDED(second->QueryInterface(IID_PPV_ARGS(&b)))&&
        a.Get()==b.Get();
}
bool valid(ID3D11Device* owner,ID3D11Texture2D* texture,
    Extent extent) noexcept {
    if(!owner||!texture||!extent.valid())return false;
    ComPtr<ID3D11Device> device;
    texture->GetDevice(&device);
    if(!sameObject(owner,device.Get()))return false;
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    return desc.Width==extent.width&&desc.Height==extent.height&&
        desc.MipLevels==1&&desc.ArraySize==1&&
        desc.SampleDesc.Count==1&&desc.SampleDesc.Quality==0&&
        desc.Usage==D3D11_USAGE_DEFAULT&&desc.Format!=DXGI_FORMAT_UNKNOWN;
}
}

Result<bool> FgWorldGuideLatch::capture(std::uint64_t source,
    std::uint64_t generation,Extent render,Extent display,
    ID3D11DeviceContext* context,ID3D11Texture2D* depth,
    ID3D11Texture2D* motion,ID3D11Texture2D* displayBeforeUi) {
    if(!source||!generation||!context||
       context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,
            "FG world guide capture lacks a verified real-frame context"};
    ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    if(!valid(device.Get(),depth,render)||
       !valid(device.Get(),motion,render)||
       !valid(device.Get(),displayBeforeUi,display))
        return Error{ErrorCode::InvalidInput,
            "FG world guide texture owner, extent or sample contract differs"};
    std::scoped_lock lock(mutex_);
    if(source<=lastSource_)
        return Error{ErrorCode::Conflict,
            "FG world guide source identity was already captured"};
    const auto copyTarget=[&](ID3D11Texture2D* source,
        ComPtr<ID3D11Texture2D>& target,bool hudless) {
        D3D11_TEXTURE2D_DESC desc{};
        source->GetDesc(&desc);
        desc.Usage=D3D11_USAGE_DEFAULT;
        if(hudless)desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags=0;desc.MiscFlags=0;
        return SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&target));
    };
    ComPtr<ID3D11Texture2D> depthCopy,motionCopy,hudless;
    if(!copyTarget(depth,depthCopy,false)||
       !copyTarget(motion,motionCopy,false)||
       !copyTarget(displayBeforeUi,hudless,true))
        return Error{ErrorCode::Unavailable,
            "FG world guide snapshot allocation failed"};
    context->CopyResource(depthCopy.Get(),depth);
    context->CopyResource(motionCopy.Get(),motion);
    context->CopyResource(hudless.Get(),displayBeforeUi);
    Pending next{};
    next.source=source;next.generation=generation;
    next.thread=GetCurrentThreadId();
    next.render=render;next.display=display;
    next.depth=std::move(depthCopy);next.motion=std::move(motionCopy);
    next.hudless=std::move(hudless);
    pending_=std::move(next);
    lastSource_=source;
    return true;
}

std::optional<FgWorldGuideFrame> FgWorldGuideLatch::take(
    const FgBoundarySample& boundary) {
    if(boundary.kind==FgBoundaryKind::Test||
       boundary.kind==FgBoundaryKind::ForeignSwap)return std::nullopt;
    std::scoped_lock lock(mutex_);
    auto pending=std::move(pending_);
    pending_={};
    if(!pending.source||boundary.kind!=FgBoundaryKind::Ready||
       !boundary.phaseReady||!boundary.realPresent||!boundary.epoch||
       boundary.world!=pending.source||
       boundary.worldThread!=pending.thread||
       boundary.presentThread!=pending.thread)
        return std::nullopt;
    FgWorldGuideFrame result{};
    auto& frame=result.frame;
    frame.source=pending.source;
    frame.generation=pending.generation;
    frame.presentToken=boundary.realPresent;
    frame.resetEpoch=boundary.epoch;
    frame.render=pending.render;
    frame.display=pending.display;
    result.depth=std::move(pending.depth);
    result.motion=std::move(pending.motion);
    result.hudless=std::move(pending.hudless);
    return result;
}

void FgWorldGuideLatch::clear() {
    std::scoped_lock lock(mutex_);
    pending_={};
}
Result<bool> attachFgWorldCameraCandidate(FgWorldGuideFrame& packet,
    const FgCameraProducerSample& producer,std::uint64_t jitterSource,
    std::uint64_t jitterGeneration,NgxJitter jitter) {
    const auto& frame=packet.frame;
    if(packet.cameraCandidate)
        return Error{ErrorCode::Conflict,"FG camera candidate was already attached"};
    if(!producer.source||!producer.revision||!producer.writeGeneration||
       producer.source!=frame.source||jitterSource!=frame.source||
       jitterGeneration!=frame.generation)
        return Error{ErrorCode::Conflict,
            "FG camera write or SR jitter belongs to a different source"};
    auto candidateFrame=frame;
    candidateFrame.cameraValid=true;
    const auto bound=bindFgGameCamera(candidateFrame,producer.camera,jitter,
        producer.revision,false);
    if(const auto* error=std::get_if<Error>(&bound))return *error;
    packet.cameraCandidate=std::get<FgCameraData>(bound);
    return true;
}
}
