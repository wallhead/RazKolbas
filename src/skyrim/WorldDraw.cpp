#include "rk/WorldDraw.hpp"
#include <exception>

namespace rk {
Result<bool> WorldDrawForwarder::configure(WorldDrawFn original,WorldDrawFn afterOriginal) {
    if(!original)return Error{ErrorCode::InvalidInput,"World-draw original target is null"};
    std::scoped_lock lock(configureMutex_);
    if(const auto current=original_.load(std::memory_order_acquire)) {
        if(current==original&&afterOriginal_.load(std::memory_order_acquire)==afterOriginal)return true;
        return Error{ErrorCode::Conflict,"World-draw forwarding owner already configured"};
    }
    afterOriginal_.store(afterOriginal,std::memory_order_release);
    original_.store(original,std::memory_order_release);
    return true;
}
void WorldDrawForwarder::dispatch(void* first,std::uint32_t second) noexcept {
    const auto original=original_.load(std::memory_order_acquire);
    if(!original)std::terminate(); // Never skip the original game world draw.
    original(first,second);
    if(const auto observe=afterOriginal_.load(std::memory_order_acquire))observe(first,second);
}
}
