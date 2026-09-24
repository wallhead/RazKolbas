#include "rk/MenuInputCapture.hpp"

namespace rk {
std::optional<bool> MenuInputCapture::update(
    bool shouldCapture,bool currentlyIgnored) noexcept {
    if(shouldCapture) {
        if(!active_) {
            active_=true;
            restoreIgnored_=currentlyIgnored;
        }
        if(!currentlyIgnored)return true;
        return std::nullopt;
    }
    if(!active_)return std::nullopt;
    active_=false;
    if(currentlyIgnored!=restoreIgnored_)return restoreIgnored_;
    return std::nullopt;
}

bool MenuInputCapture::active() const noexcept {
    return active_;
}
}
