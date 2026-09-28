#pragma once
#include "rk/FgFrameContract.hpp"
#include "rk/Result.hpp"
#include <cstdint>

namespace rk {
struct FgBackendPresent {
    std::int32_t resultCode{};
    std::uint32_t actualGeneratedFrames{};
};
struct FgPresentOutcome {
    FgDecision decision{};
    std::int32_t resultCode{};
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
        bool enabled)=0;
};

class FgPresentationCoordinator {
public:
    FgPresentationCoordinator(FgProviderSession session,
        IFgPresentBackend& backend) noexcept;
    Result<FgPresentOutcome> present(const FgSourceFrame& frame,
        bool requestedEnabled);
    bool enabled() const noexcept { return enabled_; }
private:
    FgProviderSession session_;
    IFgPresentBackend& backend_;
    FgPresentLedger ledger_;
    bool enabled_{};
};
}
