#include "rk/FgFrameContract.hpp"
#include <algorithm>

namespace rk {
namespace {
bool same(Extent a,Extent b) noexcept {
    return a.width==b.width&&a.height==b.height;
}
bool current(const FgResourceStamp& resource,const FgSourceFrame& frame,
    Extent extent) noexcept {
    return resource.ready&&resource.source==frame.source&&
        resource.generation==frame.generation&&
        resource.resetEpoch==frame.resetEpoch&&same(resource.extent,extent);
}
const FgCapability* find(std::initializer_list<FgCapability> capabilities,
    FgProvider provider) noexcept {
    for(const auto& capability:capabilities)
        if(capability.provider==provider)return &capability;
    return nullptr;
}
FgReason capabilityReason(const FgCapability* capability,
    FgSrProvider sr) noexcept {
    if(!capability||!capability->supported||!capability->validated||
       capability->maxExtraFrames==0)return FgReason::ProviderUnavailable;
    if(!(capability->validatedSrPairings&fgSrMask(sr)))
        return FgReason::UnvalidatedPairing;
    if(!capability->separateUiSupported)
        return FgReason::UnvalidatedUiContract;
    return FgReason::Ready;
}
bool allowedOnAdapter(FgProvider provider,std::uint32_t vendor) noexcept {
    if(vendor==0x10de)return provider==FgProvider::Dlss||provider==FgProvider::Fsr;
    if(vendor==0x1002||vendor==0x8086)return provider==FgProvider::Fsr;
    return false;
}
}
FgDecision decideFg(const FgSourceFrame& frame,const FgRequest& request,
    std::initializer_list<FgCapability> capabilities) noexcept {
    FgDecision decision{request.provider,FgProvider::Off,0,FgReason::Disabled};
    if(!request.enabled||request.provider==FgProvider::Off)return decision;
    if(frame.renderVendor!=0x10de&&frame.renderVendor!=0x1002&&
       frame.renderVendor!=0x8086) {
        decision.reason=FgReason::UnsupportedAdapter;return decision;
    }
    if(request.provider!=FgProvider::Auto&&
       !allowedOnAdapter(request.provider,frame.renderVendor)) {
        decision.reason=FgReason::UnsupportedAdapter;return decision;
    }
    if(!frame.ownerReady) {
        decision.reason=FgReason::PresentationOwnerUnavailable;return decision;
    }
    if(!frame.worldActive||frame.loading||frame.paused) {
        decision.reason=FgReason::InactiveScene;return decision;
    }
    if(frame.cameraCut) {
        decision.reason=FgReason::HistoryReset;return decision;
    }
    if(!frame.source||!frame.generation||!frame.presentToken||
       !frame.resetEpoch||!frame.render.valid()||!frame.display.valid()||
       !frame.cameraValid||
       !current(frame.color,frame,frame.display)||
       !current(frame.depth,frame,frame.render)||
       !current(frame.motion,frame,frame.render)) {
        decision.reason=FgReason::StaleInput;return decision;
    }
    if(!current(frame.hudless,frame,frame.display)||
       !current(frame.uiColorAlpha,frame,frame.display)) {
        decision.reason=FgReason::MissingNativeUi;return decision;
    }
    const FgCapability* selected=nullptr;
    if(request.provider==FgProvider::Auto) {
        for(const auto provider:{FgProvider::Dlss,FgProvider::Fsr}) {
            if(!allowedOnAdapter(provider,frame.renderVendor))continue;
            const auto* candidate=find(capabilities,provider);
            if(capabilityReason(candidate,frame.sr)==FgReason::Ready) {
                selected=candidate;break;
            }
        }
        if(!selected) {
            decision.reason=FgReason::ProviderUnavailable;return decision;
        }
    } else {
        selected=find(capabilities,request.provider);
        decision.reason=capabilityReason(selected,frame.sr);
        if(decision.reason!=FgReason::Ready)return decision;
    }
    decision.effective=selected->provider;
    decision.extraFrames=std::min({request.extraFrames,
        selected->maxExtraFrames,1u});
    if(decision.extraFrames==0) {
        decision.effective=FgProvider::Off;
        decision.reason=FgReason::ProviderUnavailable;
    } else decision.reason=request.extraFrames>decision.extraFrames?
        FgReason::LimitedFrameCount:FgReason::Ready;
    return decision;
}
FgProviderSession::FgProviderSession(FgRequest request,FgSrProvider sr,
    std::uint32_t renderVendor,
    std::initializer_list<FgCapability> capabilities) noexcept : request_(request) {
    if(request.provider==FgProvider::Off)return;
    if(request.provider==FgProvider::Auto) {
        for(const auto provider:{FgProvider::Dlss,FgProvider::Fsr}) {
            if(allowedOnAdapter(provider,renderVendor)&&
               capabilityReason(find(capabilities,provider),sr)==FgReason::Ready) {
                bound_=provider;
                return;
            }
        }
    } else if(allowedOnAdapter(request.provider,renderVendor)&&
        capabilityReason(find(capabilities,request.provider),sr)==FgReason::Ready) {
        bound_=request.provider;
    }
}
FgDecision FgProviderSession::decide(const FgSourceFrame& frame,
    const FgCapability& liveCapability) const noexcept {
    return decide(frame,liveCapability,request_.enabled);
}
FgDecision FgProviderSession::decide(const FgSourceFrame& frame,
    const FgCapability& liveCapability,bool enabled) const noexcept {
    auto request=request_;
    request.enabled=enabled;
    if(!enabled)return {request.provider,FgProvider::Off,0,FgReason::Disabled};
    if(bound_==FgProvider::Off||liveCapability.provider!=bound_)
        return {request.provider,FgProvider::Off,0,FgReason::ProviderUnavailable};
    return decideFg(frame,request,{liveCapability});
}
bool FgRetirementSet::ready(const FgFenceProgress& progress) const noexcept {
    const auto valid=[](std::uint64_t value) noexcept {
        return value!=0&&value!=UINT64_MAX;
    };
    return generation!=0&&progress.generation==generation&&
        valid(producer)&&valid(copy)&&valid(providerInput)&&
        valid(present)&&valid(allocator)&&
        progress.producer!=UINT64_MAX&&progress.copy!=UINT64_MAX&&
        progress.providerInput!=UINT64_MAX&&progress.present!=UINT64_MAX&&
        progress.allocator!=UINT64_MAX&&
        progress.producer>=producer&&progress.copy>=copy&&
        progress.providerInput>=providerInput&&progress.present>=present&&
        progress.allocator>=allocator;
}
void FgLeasePool::retire(const FgFenceProgress& progress) noexcept {
    if(progress.generation!=generation_)return;
    for(auto& slot:slots_)
        if(slot.state==State::InFlight&&slot.retirement.ready(progress))
            slot={};
}
std::optional<std::size_t> FgLeasePool::acquire(
    const FgFenceProgress& progress) noexcept {
    if(!generation_||progress.generation!=generation_||
       progress.producer==UINT64_MAX||progress.copy==UINT64_MAX||
       progress.providerInput==UINT64_MAX||progress.present==UINT64_MAX||
       progress.allocator==UINT64_MAX)return std::nullopt;
    retire(progress);
    for(std::size_t index=0;index<slots_.size();++index)
        if(slots_[index].state==State::Free) {
            slots_[index].state=State::Acquired;
            return index;
        }
    return std::nullopt;
}
bool FgLeasePool::submit(std::size_t index,
    const FgRetirementSet& retirement) noexcept {
    if(index>=slots_.size()||slots_[index].state!=State::Acquired||
       retirement.generation!=generation_||!retirement.producer||
       !retirement.copy||!retirement.providerInput||!retirement.present||
       !retirement.allocator||retirement.producer==UINT64_MAX||
       retirement.copy==UINT64_MAX||retirement.providerInput==UINT64_MAX||
       retirement.present==UINT64_MAX||retirement.allocator==UINT64_MAX)
        return false;
    slots_[index]={State::InFlight,retirement};
    return true;
}
bool FgLeasePool::releaseUnsubmitted(std::size_t index) noexcept {
    if(index>=slots_.size()||slots_[index].state!=State::Acquired)return false;
    slots_[index]={};
    return true;
}
bool FgLeasePool::hasOutstanding() const noexcept {
    return std::any_of(slots_.begin(),slots_.end(),[](const Slot& slot) {
        return slot.state!=State::Free;
    });
}
bool FgLeasePool::drain(const FgFenceProgress& progress) noexcept {
    if(progress.generation!=generation_||
       progress.producer==UINT64_MAX||progress.copy==UINT64_MAX||
       progress.providerInput==UINT64_MAX||progress.present==UINT64_MAX||
       progress.allocator==UINT64_MAX)return false;
    retire(progress);
    return !hasOutstanding();
}
bool FgLeasePool::advanceGeneration(std::uint64_t next,
    const FgFenceProgress& progress) noexcept {
    if(!canAdvanceGeneration(next,progress))return false;
    retire(progress);
    generation_=next;
    return true;
}
bool FgLeasePool::canAdvanceGeneration(std::uint64_t next,
    const FgFenceProgress& progress) const noexcept {
    if(!next||next<=generation_||progress.generation!=generation_||
       progress.producer==UINT64_MAX||progress.copy==UINT64_MAX||
       progress.providerInput==UINT64_MAX||progress.present==UINT64_MAX||
       progress.allocator==UINT64_MAX)return false;
    for(const auto& slot:slots_) {
        if(slot.state==State::Acquired)return false;
        if(slot.state==State::InFlight&&!slot.retirement.ready(progress))return false;
    }
    return true;
}
bool FgPresentLedger::canAcceptReal(const FgSourceFrame& frame) const noexcept {
    return frame.source&&frame.generation&&frame.presentToken&&
        frame.source>source_&&
        (frame.generation!=generation_||frame.presentToken!=token_);
}
bool FgPresentLedger::acceptReal(const FgSourceFrame& frame) noexcept {
    if(!canAcceptReal(frame))return false;
    source_=frame.source;
    generation_=frame.generation;
    token_=frame.presentToken;
    return true;
}
}
