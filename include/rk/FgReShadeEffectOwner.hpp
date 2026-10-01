#pragma once
#include "rk/Result.hpp"
#include <Windows.h>
#include <dxgi1_6.h>
#include <atomic>
#include <cstdint>
#include <memory>

namespace rk {
// Exact ReShade 6.8 add-on lease for the private FG-Off route. It keeps the
// D3D11 preset as the sole effect owner without changing the user's preset.
class FgReShadeEffectOwner final {
public:
    static Result<std::unique_ptr<FgReShadeEffectOwner>> arm(
        IDXGIAdapter* adapter,const DXGI_SWAP_CHAIN_DESC& gameDesc,
        bool addonAlreadyRegisteredForProbe=false);
    ~FgReShadeEffectOwner() noexcept;
    FgReShadeEffectOwner(const FgReShadeEffectOwner&)=delete;
    FgReShadeEffectOwner& operator=(const FgReShadeEffectOwner&)=delete;
    bool initialSuppressed() const noexcept;
    void commit() noexcept {accepting_.store(false,std::memory_order_release);}
    unsigned suppressedInits() const noexcept {
        return suppressedInits_.load(std::memory_order_acquire);
    }
    unsigned reassertions() const noexcept {
        return reassertions_.load(std::memory_order_acquire);
    }
private:
    FgReShadeEffectOwner()=default;
    static void initialized(void* runtime) noexcept;
    static void destroyed(void* runtime) noexcept;
    static void beginningEffects(void* runtime,void*,std::uint64_t,
        std::uint64_t) noexcept;
    static bool changingState(void* runtime,bool enabled) noexcept;
    void onInit(void* runtime) noexcept;
    void onDestroy(void* runtime) noexcept;
    using RegisterEvent=void(*)(std::uint32_t,void*);
    using UnregisterAddon=void(*)(HMODULE);
    static std::atomic<FgReShadeEffectOwner*> active_;
    HMODULE reshade_{};
    HMODULE addon_{};
    RegisterEvent registerEvent_{};
    RegisterEvent unregisterEvent_{};
    UnregisterAddon unregisterAddon_{};
    HWND window_{};
    LUID adapter_{};
    UINT width_{},height_{};
    DWORD thread_{};
    std::atomic<void*> runtime_{nullptr};
    std::atomic<void*> nativeSwap_{nullptr};
    std::atomic<bool> accepting_{true};
    std::atomic<bool> live_{false};
    std::atomic<bool> failed_{false};
    std::atomic<unsigned> suppressedInits_{0};
    std::atomic<unsigned> reassertions_{0};
    bool priorEnabled_{};
    bool registeredAddon_{};
    bool registeredEvents_{};
};
}
