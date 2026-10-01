#include "rk/FgGameInputProbe.hpp"
#include <dxgi1_6.h>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
bool sameObject(IUnknown* a,IUnknown* b) noexcept {
    ComPtr<IUnknown> first,second;
    return a&&b&&SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&first)))&&
        SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&second)))&&
        first.Get()==second.Get();
}
}
Result<std::unique_ptr<FgGameInputProbe>> FgGameInputProbe::begin(
    ID3D11Device* device,ID3D11DeviceContext* context,
    const FgGameInputCandidate& candidate) {
    ComPtr<ID3D11Device> contextDevice;
    if(context)context->GetDevice(&contextDevice);
    if(!device||!context||
       context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !sameObject(device,contextDevice.Get()))
        return Error{ErrorCode::InvalidInput,
            "FG game copy requires the real immediate D3D11 context"};
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    if(FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))||
       FAILED(dxgi->GetAdapter(&adapter)))
        return Error{ErrorCode::Unavailable,
            "FG game copy cannot identify the render adapter"};
    ComPtr<ID3D12Device> d12;
    if(FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&d12))))
        return Error{ErrorCode::Unavailable,
            "FG game copy cannot create a same-adapter D3D12 device"};
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if(FAILED(d12->CreateCommandQueue(&queueDesc,
        IID_PPV_ARGS(&queue))))
        return Error{ErrorCode::Unavailable,
            "FG game copy cannot create the companion direct queue"};
    return beginOnOwner(device,context,d12.Get(),queue.Get(),nullptr,candidate);
}
Result<std::unique_ptr<FgGameInputProbe>> FgGameInputProbe::beginOnOwner(
    ID3D11Device* device,ID3D11DeviceContext* context,
    ID3D12Device* nativeDevice,ID3D12CommandQueue* nativeQueue,
    std::shared_ptr<void> providerLifetime,
    const FgGameInputCandidate& candidate) {
    ComPtr<ID3D11Device> contextDevice;
    if(context)context->GetDevice(&contextDevice);
    ComPtr<ID3D12Device> queueDevice;
    if(!device||!context||
       context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !sameObject(device,contextDevice.Get())||!nativeDevice||!nativeQueue||
       nativeQueue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT||
       FAILED(nativeQueue->GetDevice(IID_PPV_ARGS(&queueDevice)))||
       !sameObject(nativeDevice,queueDevice.Get()))
        return Error{ErrorCode::Conflict,
            "FG input copy requires the exact native presentation device/direct queue"};
    auto probe=std::unique_ptr<FgGameInputProbe>(new FgGameInputProbe);
    probe->d12_=nativeDevice;
    probe->queue_=nativeQueue;
    probe->providerLifetime_=providerLifetime;
    auto bridge=FgSharedInputs::create(device,nativeDevice,nativeQueue,
        std::move(providerLifetime));
    if(const auto* error=std::get_if<Error>(&bridge))return *error;
    probe->bridge_=std::move(std::get<std::unique_ptr<FgSharedInputs>>(
        bridge));
    probe->ring_=std::make_unique<FgInputLeaseRing>(*probe->bridge_,
        candidate.frame.generation);
    probe->generation_=candidate.frame.generation;
    const auto enqueued=probe->enqueue(context,candidate);
    if(const auto* error=std::get_if<Error>(&enqueued))return *error;
    return Result<std::unique_ptr<FgGameInputProbe>>{std::move(probe)};
}
Result<FgCopyTicket> FgGameInputProbe::enqueue(
    ID3D11DeviceContext* context,const FgGameInputCandidate& candidate) {
    if(!ring_||!bridge_||lease_||state_!=FgGameCopyState::Complete||
       candidate.frame.generation!=generation_||
       !candidate.frame.source||!candidate.frame.presentToken||
       (source_&&candidate.frame.source<=source_)||
       (presentToken_&&candidate.frame.presentToken<=presentToken_))
        return Error{ErrorCode::Conflict,
            "FG game copy is pending, closed or out of source order"};
    auto prepared=ring_->prepare(candidate.frame,candidate.sources(),
        context,{generation_,0,0,0,0,0});
    if(const auto* error=std::get_if<Error>(&prepared)) {
        if(ring_->failed())state_=FgGameCopyState::Failed;
        return *error;
    }
    lease_=std::move(std::get<FgInputLease>(prepared));
    source_=candidate.frame.source;
    presentToken_=candidate.frame.presentToken;
    ticket_=lease_->lastCopy;
    state_=FgGameCopyState::Pending;
    return ticket_;
}
Result<FgPreparedSubmission> FgGameInputProbe::inspectPrepared(
    const FgGameInputCandidate& candidate,const FgUiPlaneFrame& ui,
    const FgCameraData& camera,std::uint32_t physicalOutputIndex,
    std::uint32_t swapBufferCount) const {
    if(!lease_||state_!=FgGameCopyState::Pending||
       candidate.frame.source!=lease_->source||
       candidate.frame.generation!=lease_->generation||
       candidate.frame.presentToken!=lease_->presentToken||
       candidate.frame.resetEpoch!=lease_->resetEpoch)
        return Error{ErrorCode::Conflict,
            "FG game copy has no matching pending input lease"};
    for(std::size_t i=0;i<candidate.textures.size();++i)
        if(candidate.textures[i].Get()!=lease_->sourceTextures[i].Get())
            return Error{ErrorCode::Conflict,
                "FG game candidate differs from the copied input lease"};
    auto frame=candidate.frame;
    // Promote only this candidate for metadata validation. The live frame
    // stays unadmitted until owner, copy and provider fences are proven.
    frame.cameraValid=true;
    return prepareFgSubmission(frame,*lease_,ui,camera,physicalOutputIndex,
        swapBufferCount);
}
FgGameCopyState FgGameInputProbe::poll() noexcept {
    if(state_!=FgGameCopyState::Pending)return state_;
    if(!bridge_||!ring_||!lease_)return state_=FgGameCopyState::Failed;
    const auto progress=bridge_->copyStatus(ticket_.copy);
    if(progress==FgCopyStatus::Pending)return state_;
    if(progress!=FgCopyStatus::Complete||!ring_->discard(*lease_)||
       !bridge_->healthy())
        return state_=FgGameCopyState::Failed;
    lease_.reset();
    return state_=FgGameCopyState::Complete;
}
bool FgGameInputProbe::close() noexcept {
    if(state_==FgGameCopyState::Pending||
       state_==FgGameCopyState::Failed)return false;
    if(!ring_)return true;
    if(!ring_->stop({generation_,0,0,0,0,0}))return false;
    ring_.reset();
    bridge_.reset();
    queue_.Reset();d12_.Reset();
    providerLifetime_.reset();
    return true;
}
}
