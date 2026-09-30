#pragma once
#include <cstdint>
#include <optional>

namespace rk {
enum class LoadingPicturePhase { Cold, AfterWorld };
struct LoadingPictureSelection {
    std::uint64_t frame{};
    LoadingPicturePhase phase{};
};

// One sample per loading context, after 30 consecutive distinct menu frames.
class LoadingPictureProbe {
public:
    std::optional<LoadingPictureSelection> observe(std::uint64_t frame,
        bool loading,bool mainMenu,bool providerSeen) noexcept {
        if(mainMenu)sawMainMenu_=true;
        if(!loading||mainMenu||!frame||!sawMainMenu_) {
            lastFrame_=0;
            stableFrames_=0;
            return std::nullopt;
        }
        if(frame==lastFrame_)return std::nullopt;
        stableFrames_=frame==lastFrame_+1?stableFrames_+1:1;
        lastFrame_=frame;
        if(stableFrames_<30)return std::nullopt;
        if(providerSeen) {
            if(warmCaptured_)return std::nullopt;
            warmCaptured_=true;
            return LoadingPictureSelection{frame,LoadingPicturePhase::AfterWorld};
        }
        if(coldCaptured_)return std::nullopt;
        coldCaptured_=true;
        return LoadingPictureSelection{frame,LoadingPicturePhase::Cold};
    }
private:
    std::uint64_t lastFrame_{};
    unsigned stableFrames_{};
    bool coldCaptured_{},warmCaptured_{};
    bool sawMainMenu_{};
};
}
