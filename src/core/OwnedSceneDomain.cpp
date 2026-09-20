#include "rk/OwnedSceneDomain.hpp"
#include <cmath>

namespace rk {
bool OwnedScenePlan::valid() const noexcept {
    return generation&&render.valid()&&display.valid()&&
        display.width<=8192&&display.height<=8192&&
        render.width<=display.width&&render.height<=display.height&&
        (render.width<display.width||render.height<display.height);
}
bool OwnedSceneDomain::configure(OwnedScenePlan plan) noexcept {
    if(!plan.valid()||(plan_.generation&&plan.generation<=plan_.generation))return false;
    plan_=plan;frame_=0;renderThread_=0;phase_=ScenePhase::Dormant;return true;
}
bool OwnedSceneDomain::begin(std::uint64_t frame,std::uint64_t generation,
    std::uint32_t renderThread) noexcept {
    if(!plan_.valid()||generation!=plan_.generation||!frame||frame<=frame_||
       phase_==ScenePhase::Suspended||phase_==ScenePhase::Processing||phase_==ScenePhase::World)
        return false;
    frame_=frame;renderThread_=renderThread;phase_=ScenePhase::World;return true;
}
bool OwnedSceneDomain::startProcessing(std::uint64_t frame,std::uint64_t generation) noexcept {
    if(phase_!=ScenePhase::World||frame!=frame_||generation!=plan_.generation)return false;
    phase_=ScenePhase::Processing;return true;
}
bool OwnedSceneDomain::enterUi(std::uint64_t frame,std::uint64_t generation,
    bool currentImagePublished) noexcept {
    if(phase_!=ScenePhase::Processing||frame!=frame_||generation!=plan_.generation||
       !currentImagePublished)return false;
    phase_=ScenePhase::NativeUi;return true;
}
bool OwnedSceneDomain::closePublishedFrame(std::uint64_t frame,
    std::uint64_t generation) noexcept {
    if(phase_!=ScenePhase::NativeUi||frame!=frame_||generation!=plan_.generation)
        return false;
    phase_=ScenePhase::Dormant;renderThread_=0;return true;
}
bool OwnedSceneDomain::remapFullUiViewport(float& width,float& height,float x,float y,
    bool nativeTargetBound) const noexcept {
    if(phase_!=ScenePhase::NativeUi||!nativeTargetBound||x!=0||y!=0||
       !std::isfinite(width)||!std::isfinite(height)||
       width!=static_cast<float>(plan_.render.width)||
       height!=static_cast<float>(plan_.render.height))return false;
    width=static_cast<float>(plan_.display.width);
    height=static_cast<float>(plan_.display.height);
    return true;
}
}
