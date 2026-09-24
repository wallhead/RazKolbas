#include "rk/MenuInputCapture.hpp"

namespace rk {
MenuInputProfile menuInputProfile(MenuInputRuntime runtime) noexcept {
    switch(runtime) {
    case MenuInputRuntime::SkyrimSe1597:
        return {0x2ec5bd0,0x121};
    case MenuInputRuntime::SkyrimAe161170:
        return {0x30fda10,0x129};
    }
    return {};
}

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
