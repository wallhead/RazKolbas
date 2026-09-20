#pragma once
#include "rk/FrameContracts.hpp"
#include <cstdint>

namespace rk {
enum class ScenePhase { Dormant, World, Processing, NativeUi, Suspended };
struct OwnedScenePlan {
    Extent render{},display{};
    std::uint64_t generation{};
    bool valid() const noexcept;
};
// Render-thread policy only. Resource creation, GPU completion and publication
// remain the controller's responsibility; enterUi requires real publication.
class OwnedSceneDomain final {
public:
    bool configure(OwnedScenePlan plan) noexcept;
    bool begin(std::uint64_t frame,std::uint64_t generation,
        std::uint32_t renderThread=0) noexcept;
    bool startProcessing(std::uint64_t frame,std::uint64_t generation) noexcept;
    bool enterUi(std::uint64_t frame,std::uint64_t generation,
        bool currentImagePublished) noexcept;
    void suspend() noexcept { phase_=ScenePhase::Suspended; }
    ScenePhase phase() const noexcept { return phase_; }
    std::uint64_t frame() const noexcept { return frame_; }
    std::uint32_t renderThread() const noexcept { return renderThread_; }
    const OwnedScenePlan& plan() const noexcept { return plan_; }
    bool remapFullUiViewport(float& width,float& height,float x,float y,
        bool nativeTargetBound) const noexcept;
private:
    OwnedScenePlan plan_{};
    std::uint64_t frame_{};
    std::uint32_t renderThread_{};
    ScenePhase phase_{ScenePhase::Dormant};
};
}
