#pragma once
#include "rk/FgLowerSwap.hpp"

namespace rk {
// Real-frame fallback for an already-owned DXGI chain. It never advertises
// generated output and cannot turn an unavailable FG provider On.
class FgNativePresentBackend final : public IFgPresentBackend {
public:
    explicit FgNativePresentBackend(FgLowerSwap& lower) noexcept : lower_(lower) {}
    FgCapability capability() const noexcept override { return {}; }
    Result<bool> setMode(bool enabled) override { return !enabled; }
    Result<FgBackendPresent> presentReal(const FgSourceFrame& frame,
        bool enabled,const FgPresentCall& call) override;
    std::uint64_t realPresentCalls() const noexcept { return realCalls_; }
    HRESULT lastResult() const noexcept { return lastResult_; }
private:
    FgLowerSwap& lower_;
    std::uint64_t realCalls_{};
    HRESULT lastResult_{};
};
}
