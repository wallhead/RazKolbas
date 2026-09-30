#include "rk/FgRealFrameBoundaries.hpp"

namespace rk {
void FgRealFrameBoundaries::rendererBegin(std::uint64_t thread) {
    std::scoped_lock lock(mutex_);
    if(rendererBegins_||worldBegins_||world_!=consumedWorld_)
        phaseOrderValid_=false;
    ++rendererBegins_;
    rendererThread_=thread;
}

void FgRealFrameBoundaries::worldBegin(std::uint64_t thread) {
    std::scoped_lock lock(mutex_);
    if(rendererBegins_!=1||worldBegins_||world_!=consumedWorld_)
        phaseOrderValid_=false;
    ++worldBegins_;
    worldBeginThread_=thread;
}

void FgRealFrameBoundaries::world(std::uint64_t thread) {
    std::scoped_lock lock(mutex_);
    if(rendererBegins_!=1||worldBegins_!=1||world_!=consumedWorld_)
        phaseOrderValid_=false;
    ++world_;
    worldThread_=thread;
}

FgBoundarySample FgRealFrameBoundaries::present(std::uint64_t thread,
    bool test,bool ownSwap) {
    std::scoped_lock lock(mutex_);
    FgBoundarySample sample{};
    sample.world=world_;
    sample.epoch=epoch_;
    sample.worldThread=worldThread_;
    sample.presentThread=thread;
    sample.rendererBegins=rendererBegins_;
    sample.worldBegins=worldBegins_;
    sample.rendererThread=rendererThread_;
    sample.worldBeginThread=worldBeginThread_;
    if(!ownSwap)sample.kind=FgBoundaryKind::ForeignSwap;
    else if(test)sample.kind=FgBoundaryKind::Test;
    else {
        ++realPresent_;
        const auto pending=world_-consumedWorld_;
        consumedWorld_=world_;
        if(!pending) {sample.kind=FgBoundaryKind::NoWorld;++noWorld_;}
        else if(pending!=1) {
            sample.kind=FgBoundaryKind::MultipleWorlds;++multipleWorlds_;
        } else if(worldThread_!=thread) {
            sample.kind=FgBoundaryKind::ThreadMismatch;++threadMismatch_;
        } else {sample.kind=FgBoundaryKind::Ready;++ready_;}
        sample.phaseReady=sample.kind==FgBoundaryKind::Ready&&
            phaseOrderValid_&&rendererBegins_==1&&worldBegins_==1&&
            rendererThread_==thread&&worldBeginThread_==thread;
        if(sample.kind==FgBoundaryKind::Ready) {
            if(sample.phaseReady)++phaseReadyCount_;
            else ++phaseRejectedCount_;
        }
        rendererBegins_=worldBegins_=rendererThread_=worldBeginThread_=0;
        phaseOrderValid_=true;
    }
    sample.realPresent=realPresent_;
    sample.ready=ready_;
    sample.noWorld=noWorld_;
    sample.multipleWorlds=multipleWorlds_;
    sample.threadMismatch=threadMismatch_;
    sample.phaseReadyCount=phaseReadyCount_;
    sample.phaseRejectedCount=phaseRejectedCount_;
    return sample;
}

void FgRealFrameBoundaries::reset() {
    std::scoped_lock lock(mutex_);
    ++epoch_;
    consumedWorld_=world_;
    rendererBegins_=worldBegins_=rendererThread_=worldBeginThread_=0;
    phaseOrderValid_=true;
}
}
