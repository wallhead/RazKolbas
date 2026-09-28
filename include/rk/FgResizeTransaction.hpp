#pragma once
#include "rk/FgInputLeaseRing.hpp"
#include "rk/FgLowerSwap.hpp"

namespace rk {
// Single presentation-thread transaction. A failed DXGI resize leaves the
// old generation intact; the backend remains Off until the next real frame.
class FgResizeTransaction {
public:
    FgResizeTransaction(FgPresentationCoordinator& coordinator,
        FgInputLeaseRing& inputs,FgLowerSwap& lower) noexcept :
        coordinator_(coordinator),inputs_(inputs),lower_(lower) {}
    Result<HRESULT> resize(const FgResizeCall& call,
        std::uint64_t nextGeneration,const FgFenceProgress& progress);
private:
    FgPresentationCoordinator& coordinator_;
    FgInputLeaseRing& inputs_;
    FgLowerSwap& lower_;
};
}
