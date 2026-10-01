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
    auto probe=std::unique_ptr<FgGameInputProbe>(new FgGameInputProbe);
    if(FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&probe->d12_))))
        return Error{ErrorCode::Unavailable,
            "FG game copy cannot create a same-adapter D3D12 device"};
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    if(FAILED(probe->d12_->CreateCommandQueue(&queueDesc,
        IID_PPV_ARGS(&probe->queue_))))
        return Error{ErrorCode::Unavailable,
            "FG game copy cannot create the companion direct queue"};
    auto bridge=FgSharedInputs::create(device,probe->d12_.Get(),
        probe->queue_.Get());
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
    return true;
}
}
