#include "rk/DrsState.hpp"
#include <cmath>

namespace rk {
std::optional<DrsTransition> planDrsTransition(const RenderSizePlan& plan,
    DrsStateSnapshot state,bool ownsLock) noexcept {
    if(!plan.valid()||!plan.reduced||state.displayWidth!=plan.display.width||
       state.displayHeight!=plan.display.height||
       !std::isfinite(state.currentWidth)||!std::isfinite(state.currentHeight)||
       state.currentWidth<=0.0f||state.currentWidth>1.0f||
       state.currentHeight<=0.0f||state.currentHeight>1.0f||
       state.lock!=(ownsLock?1u:0u))return std::nullopt;
    const auto width=static_cast<float>(plan.render.width)/plan.display.width;
    const auto height=static_cast<float>(plan.render.height)/plan.display.height;
    if(ownsLock && (std::fabs(state.currentWidth-width)>0.00001f||
       std::fabs(state.currentHeight-height)>0.00001f))return std::nullopt;
    return DrsTransition{state.currentWidth,state.currentHeight,width,height,1};
}
std::optional<DrsTransition> planDrsRelease(const RenderSizePlan& plan,
    DrsStateSnapshot state,bool ownsLock) noexcept {
    if(!ownsLock||!plan.valid()||!plan.reduced||state.lock!=1||
       !std::isfinite(state.currentWidth)||!std::isfinite(state.currentHeight))
        return std::nullopt;
    const auto width=static_cast<float>(plan.render.width)/plan.display.width;
    const auto height=static_cast<float>(plan.render.height)/plan.display.height;
    if(std::fabs(state.currentWidth-width)>0.00001f||
       std::fabs(state.currentHeight-height)>0.00001f)return std::nullopt;
    return DrsTransition{state.currentWidth,state.currentHeight,1.0f,1.0f,0};
}
bool drsStateMayRetryAfterNativeLock(const RenderSizePlan& plan,
    DrsStateSnapshot state) noexcept {
    return plan.valid()&&plan.reduced&&
        state.displayWidth==plan.display.width&&
        state.displayHeight==plan.display.height&&
        state.lock==1&&state.currentWidth==1.0f&&state.currentHeight==1.0f;
}
}
