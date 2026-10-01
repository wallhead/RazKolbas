#pragma once
#include "rk/FgFrameContract.hpp"
#include "rk/FgRealFrameBoundaries.hpp"
#include <cstdint>

namespace rk {
// A read-only inventory of missing inputs. Packet readiness is not a provider
// capability decision and does not create a Streamline token or submit work.
enum class FgLiveGap : std::uint32_t {
    Phase=1u<<0, Identity=1u<<1, Owner=1u<<2, Scene=1u<<3,
    Camera=1u<<4, Color=1u<<5, Depth=1u<<6, Motion=1u<<7,
    Hudless=1u<<8, NativeUi=1u<<9, Retirement=1u<<10
};
struct FgLiveFrameInspection {
    std::uint32_t gaps{};
    bool ready() const noexcept { return gaps==0; }
    bool has(FgLiveGap gap) const noexcept {
        return (gaps&static_cast<std::uint32_t>(gap))!=0;
    }
};
FgLiveFrameInspection inspectFgLiveFrame(const FgBoundarySample& boundary,
    const FgSourceFrame& frame,bool inputsRetained) noexcept;
}
