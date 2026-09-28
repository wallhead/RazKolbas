#include "rk/FgInputLeaseRing.hpp"
#include <exception>

namespace rk {
namespace {
bool sameExtent(const FgResourceStamp& stamp,const FgSourceFrame& frame,
    Extent expected) noexcept {
    return stamp.ready&&stamp.source==frame.source&&
        stamp.generation==frame.generation&&stamp.resetEpoch==frame.resetEpoch&&
        stamp.extent.width==expected.width&&stamp.extent.height==expected.height;
}
bool sameDesc(const D3D11_TEXTURE2D_DESC& a,
    const D3D11_TEXTURE2D_DESC& b) noexcept {
    return a.Width==b.Width&&a.Height==b.Height&&a.Format==b.Format&&
        a.MipLevels==b.MipLevels&&a.ArraySize==b.ArraySize&&
        a.SampleDesc.Count==b.SampleDesc.Count&&
        a.SampleDesc.Quality==b.SampleDesc.Quality;
}
}
FgInputLeaseRing::FgInputLeaseRing(FgSharedInputs& bridge,
    std::uint64_t generation) noexcept : bridge_(bridge),
    lifetime_(bridge.retainLifetime()),pool_(generation) {}

std::vector<FgInputLeaseRing::Quarantined>&
FgInputLeaseRing::quarantine() {
    // Deliberately process-lifetime: an uncertain GPU/provider read must not
    // be released by a transient owner destructor.
    static auto* owners=new std::vector<Quarantined>;
    return *owners;
}
std::mutex& FgInputLeaseRing::quarantineMutex() {
    static auto* mutex=new std::mutex;
    return *mutex;
}
FgInputLeaseRing::~FgInputLeaseRing() noexcept {
    if(stopped_||(!failed_&&!pool_.hasOutstanding()))return;
    try {
        std::scoped_lock lock(quarantineMutex());
        quarantine().push_back({std::move(lifetime_),std::move(slots_)});
    } catch(...) {
        // Releasing an uncertain GPU resource is worse than failing closed.
        std::terminate();
    }
}
std::size_t FgInputLeaseRing::quarantinedOwners() noexcept {
    std::scoped_lock lock(quarantineMutex());
    return quarantine().size();
}

Result<FgInputLease> FgInputLeaseRing::prepare(const FgSourceFrame& frame,
    const FgInputSources& sources,ID3D11DeviceContext* context,
    const FgFenceProgress& progress) {
    if(stopped_||failed_||!bridge_.healthy()||!context||!frame.source||
       !frame.presentToken||
       !frame.resetEpoch||
       frame.generation!=pool_.generation()||
       progress.generation!=frame.generation||
       !frame.render.valid()||!frame.display.valid())
        return Error{ErrorCode::InvalidInput,"FG input lease has no current generation or context"};
    const std::array<const FgResourceStamp*,5> stamps{
        &frame.color,&frame.depth,&frame.motion,&frame.hudless,
        &frame.uiColorAlpha};
    std::array<D3D11_TEXTURE2D_DESC,5> descs{};
    for(std::size_t i=0;i<stamps.size();++i) {
        const auto expected=(i==1||i==2)?frame.render:frame.display;
        if(!sameExtent(*stamps[i],frame,expected)||!sources.textures[i])
            return Error{ErrorCode::InvalidInput,"FG input guide is stale or missing"};
        sources.textures[i]->GetDesc(&descs[i]);
        if(descs[i].Width!=expected.width||descs[i].Height!=expected.height||
           descs[i].MipLevels!=1||descs[i].ArraySize!=1||
           descs[i].SampleDesc.Count!=1||descs[i].SampleDesc.Quality!=0||
           descs[i].Format==DXGI_FORMAT_UNKNOWN)
            return Error{ErrorCode::InvalidInput,"FG input texture extent or sample contract differs"};
    }
    const auto acquired=pool_.acquire(progress);
    if(!acquired)return Error{ErrorCode::Unavailable,
        "No FG input slot has completed provider and presentation retirement"};
    auto& slot=slots_[*acquired];
    for(std::size_t i=0;i<descs.size();++i) {
        if(slot.surfaces[i]&&sameDesc(slot.descs[i],descs[i]))continue;
        auto made=bridge_.makeSurface(descs[i]);
        if(const auto error=std::get_if<Error>(&made)) {
            pool_.releaseUnsubmitted(*acquired);
            return *error;
        }
        slot.surfaces[i]=std::move(std::get<FgSharedSurface>(made));
        slot.descs[i]=descs[i];
    }
    FgInputLease lease{};
    lease.slot=*acquired;
    lease.generation=frame.generation;
    lease.source=frame.source;
    lease.presentToken=frame.presentToken;
    lease.resetEpoch=frame.resetEpoch;
    lease.serial=++nextSerial_;
    slot.serial=lease.serial;
    slot.source=lease.source;
    slot.presentToken=lease.presentToken;
    slot.resetEpoch=lease.resetEpoch;
    slot.prepared=true;
    slot.lastCopy={};
    for(std::size_t i=0;i<sources.textures.size();++i) {
        slot.sourceTextures[i]=sources.textures[i];
        lease.sourceTextures[i]=sources.textures[i];
    }
    for(std::size_t i=0;i<descs.size();++i) {
        const auto copied=bridge_.copy(context,sources.textures[i],*slot.surfaces[i]);
        if(const auto error=std::get_if<Error>(&copied)) {
            // A preceding copy may already be queued. Keep this slot and all
            // its surfaces quarantined until the owning device is torn down.
            failed_=true;
            return *error;
        }
        lease.lastCopy=std::get<FgCopyTicket>(copied);
        slot.lastCopy=lease.lastCopy;
        lease.resources[i]=slot.surfaces[i]->d12();
    }
    return lease;
}
bool FgInputLeaseRing::current(const FgInputLease& lease) const noexcept {
    if(stopped_||lease.slot>=slots_.size()||
       lease.generation!=pool_.generation()||!lease.serial)return false;
    const auto& slot=slots_[lease.slot];
    if(!slot.prepared||slot.serial!=lease.serial||
       slot.source!=lease.source||slot.presentToken!=lease.presentToken||
       slot.resetEpoch!=lease.resetEpoch||
       slot.lastCopy.producer!=lease.lastCopy.producer||
       slot.lastCopy.copy!=lease.lastCopy.copy)return false;
    for(std::size_t i=0;i<lease.resources.size();++i)
        if(!slot.surfaces[i]||
           slot.sourceTextures[i].Get()!=lease.sourceTextures[i].Get()||
           slot.surfaces[i]->d12()!=lease.resources[i].Get())return false;
    return true;
}
bool FgInputLeaseRing::submit(const FgInputLease& lease,
    const FgRetirementSet& retirement) noexcept {
    if(stopped_||failed_||!bridge_.healthy()||!current(lease)||
       retirement.generation!=lease.generation||
       retirement.producer<lease.lastCopy.producer||
       retirement.copy<lease.lastCopy.copy||
       !lease.lastCopy.producer||!lease.lastCopy.copy||
       !pool_.submit(lease.slot,retirement))return false;
    slots_[lease.slot].prepared=false;
    return true;
}
bool FgInputLeaseRing::discard(const FgInputLease& lease) noexcept {
    if(stopped_||failed_||!current(lease))return false;
    if(bridge_.copyStatus(lease.lastCopy.copy)==FgCopyStatus::DeviceRemoved) {
        failed_=true;
        return false;
    }
    if(!bridge_.waitCopy(lease.lastCopy.copy)||
       !pool_.releaseUnsubmitted(lease.slot))return false;
    slots_[lease.slot].prepared=false;
    return true;
}
bool FgInputLeaseRing::stop(const FgFenceProgress& progress) noexcept {
    if(stopped_)return true;
    if(failed_||!bridge_.healthy())return false;
    for(const auto& slot:slots_)
        if(slot.lastCopy.copy&&
           !bridge_.copyComplete(slot.lastCopy.copy))return false;
    if(!pool_.drain(progress))return false;
    for(auto& slot:slots_)slot=Slot{};
    stopped_=true;
    return true;
}
bool FgInputLeaseRing::advanceGeneration(std::uint64_t next,
    const FgFenceProgress& progress) noexcept {
    if(stopped_||failed_||!bridge_.healthy()||
       !pool_.advanceGeneration(next,progress))
        return false;
    for(auto& slot:slots_)slot=Slot{};
    return true;
}
bool FgInputLeaseRing::canAdvanceGeneration(std::uint64_t next,
    const FgFenceProgress& progress) const noexcept {
    return !stopped_&&!failed_&&bridge_.healthy()&&
        pool_.canAdvanceGeneration(next,progress);
}
}
