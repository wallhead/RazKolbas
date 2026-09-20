#pragma once
#include "rk/RenderSizePolicy.hpp"
#include <cstdint>
#include <optional>

namespace rk {
struct DrsStateSnapshot {
    std::uint32_t displayWidth{},displayHeight{};
    float currentWidth{},currentHeight{};
    std::uint32_t lock{};
};
struct DrsTransition {
    float previousWidth{},previousHeight{};
    float currentWidth{},currentHeight{};
    std::uint32_t lock{};
};
// Computes a single exact-state update. The caller must verify the game
// layout, own its prior lock, and apply all five fields at a safe render-time
// callback. No game memory is read or written by this policy.
std::optional<DrsTransition> planDrsTransition(const RenderSizePlan& plan,
    DrsStateSnapshot state,bool ownsLock) noexcept;
// Releases only a ratio/lock still matching this plugin's active plan.
std::optional<DrsTransition> planDrsRelease(const RenderSizePlan& plan,
    DrsStateSnapshot state,bool ownsLock) noexcept;
// A native-sized lock may be transient at Renderer Begin. Waiting is safe;
// this never grants permission to overwrite that locked state.
bool drsStateMayRetryAfterNativeLock(const RenderSizePlan& plan,
    DrsStateSnapshot state) noexcept;
}
