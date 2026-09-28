#pragma once
#include "rk/FgFrameContract.hpp"
#include "rk/Result.hpp"
#include <dxgi1_2.h>
#include <cstdint>

namespace rk {
enum class FgPresentMethod { Present, Present1 };
struct FgPresentCall {
    FgPresentMethod method{FgPresentMethod::Present};
    UINT interval{1},flags{};
    // Borrowed for this call only. The backend forwards the exact parameters.
    const DXGI_PRESENT_PARAMETERS* parameters{};
};
struct FgBackendPresent {
    HRESULT resultCode{};
    std::uint32_t actualGeneratedFrames{};
};
struct FgPresentOutcome {
    FgDecision decision{};
    HRESULT resultCode{};
    std::uint32_t actualGeneratedFrames{};
};

// The selected backend owns the one lower Present. setMode(false) must turn
// generation Off and drain inputs that its worker queues may still reference.
class IFgPresentBackend {
public:
    virtual ~IFgPresentBackend()=default;
    virtual FgCapability capability() const noexcept=0;
    virtual Result<bool> setMode(bool enabled)=0;
    virtual Result<FgBackendPresent> presentReal(const FgSourceFrame& frame,
        bool enabled,const FgPresentCall& call)=0;
};

class FgPresentationCoordinator {
public:
    FgPresentationCoordinator(FgProviderSession session,
        IFgPresentBackend& backend) noexcept;
    Result<FgPresentOutcome> present(const FgSourceFrame& frame,
        bool requestedEnabled,const FgPresentCall& call={});
    Result<bool> suspend();
    bool enabled() const noexcept { return enabled_; }
private:
    FgProviderSession session_;
    IFgPresentBackend& backend_;
    FgPresentLedger ledger_;
    bool enabled_{};
};
}
