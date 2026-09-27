#pragma once
#include "rk/FrameContracts.hpp"
#include <array>
#include <cstdint>

namespace rk {
// Temporarily presents native dimensions to Skyrim's deferred HUD producer.
// The caller verifies module identity, render-thread ownership and writable
// storage before constructing this window. Other writers retain ownership of
// any field they change while it is open.
class NativeHudDimensions {
public:
    NativeHudDimensions(volatile std::uint32_t* pairs,
        Extent render,Extent display) noexcept : pairs_(pairs) {
        if(!pairs_||!render.valid()||!display.valid()||
           render.width>=display.width||render.height>=display.height)return;
        original_={render.width,render.height,render.width,render.height};
        replacement_={display.width,display.height,display.width,display.height};
        for(unsigned i=0;i<4;++i)
            if(pairs_[i]!=original_[i])return;
        for(unsigned i=0;i<4;++i)pairs_[i]=replacement_[i];
        active_=true;
    }
    NativeHudDimensions(const NativeHudDimensions&)=delete;
    NativeHudDimensions& operator=(const NativeHudDimensions&)=delete;
    ~NativeHudDimensions() {restore();}
    bool active() const noexcept {return active_;}
    bool restore() noexcept {
        if(!active_)return false;
        bool unchanged=true;
        for(unsigned i=0;i<4;++i) {
            if(pairs_[i]==replacement_[i])pairs_[i]=original_[i];
            else unchanged=false;
        }
        active_=false;
        return unchanged;
    }
private:
    volatile std::uint32_t* pairs_{};
    std::array<std::uint32_t,4> original_{};
    std::array<std::uint32_t,4> replacement_{};
    bool active_{};
};
}
