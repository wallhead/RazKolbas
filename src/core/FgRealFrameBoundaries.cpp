#include "rk/FgRealFrameBoundaries.hpp"

namespace rk {
void FgRealFrameBoundaries::world(std::uint64_t thread) {
    std::scoped_lock lock(mutex_);
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
    }
    sample.realPresent=realPresent_;
    sample.ready=ready_;
    sample.noWorld=noWorld_;
    sample.multipleWorlds=multipleWorlds_;
    sample.threadMismatch=threadMismatch_;
    return sample;
}

void FgRealFrameBoundaries::reset() {
    std::scoped_lock lock(mutex_);
    ++epoch_;
    consumedWorld_=world_;
}
}
