#include "rk/MenuDisplay.hpp"
#include <exception>

namespace rk {
Result<bool> MenuDisplayForwarder::configure(MenuDisplayFn original,
    MenuDisplayFn beforeOriginal) {
    if(!original)return Error{ErrorCode::InvalidInput,"Menu-display original target is null"};
    std::scoped_lock lock(configureMutex_);
    if(const auto current=original_.load(std::memory_order_acquire)) {
        if(current==original&&beforeOriginal_.load(std::memory_order_acquire)==beforeOriginal)
            return true;
        return Error{ErrorCode::Conflict,"Menu-display forwarding owner already configured"};
    }
    beforeOriginal_.store(beforeOriginal,std::memory_order_release);
    original_.store(original,std::memory_order_release);
    return true;
}
void MenuDisplayForwarder::dispatch(void* first,std::uint32_t second,
    std::uint32_t third,std::uint32_t fourth) noexcept {
    const auto original=original_.load(std::memory_order_acquire);
    if(!original)std::terminate();
    if(const auto before=beforeOriginal_.load(std::memory_order_acquire))
        before(first,second,third,fourth);
    original(first,second,third,fourth);
}
}
