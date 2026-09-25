#include "rk/DeferredUiFlush.hpp"
#include <exception>

namespace rk {
Result<bool> DeferredUiFlushForwarder::configure(DeferredUiFlushFn original,
    DeferredUiFlushFn beforeOriginal) {
    if(!original)
        return Error{ErrorCode::InvalidInput,"Deferred UI flush original target is null"};
    std::scoped_lock lock(configureMutex_);
    if(const auto current=original_.load(std::memory_order_acquire)) {
        if(current==original&&
           beforeOriginal_.load(std::memory_order_acquire)==beforeOriginal)return true;
        return Error{ErrorCode::Conflict,
            "Deferred UI flush forwarding owner already configured"};
    }
    beforeOriginal_.store(beforeOriginal,std::memory_order_release);
    original_.store(original,std::memory_order_release);
    return true;
}
void DeferredUiFlushForwarder::dispatch(void* renderer) noexcept {
    const auto original=original_.load(std::memory_order_acquire);
    if(!original)std::terminate();
    if(const auto before=beforeOriginal_.load(std::memory_order_acquire)) {
        try {before(renderer);}catch(...) {}
    }
    original(renderer);
}
}
